#include <netinet/in.h>
#include <signal.h>
#include <pthread.h>
#include <sys/stat.h>
#include <fcntl.h>

#include "ybruf.h"
#define O_RDONLY      0x0000	
#define O_WRONLY      0x0001
#define O_RDWR        0x0002

/* Tomas helped us with this link https://stackoverflow.com/questions/17954432/creating-a-daemon-in-linux */ 
// Global parameters
static const int MAX_SERVERS = 1;
static int n_servers = 0;
static pthread_mutex_t counter_in_use = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t too_many_servers = PTHREAD_COND_INITIALIZER;

// Port for the server
static int PORT = 8000;

// Are we done yet?
static bool done = false;

static bool app_initialize(int argn, char *argv[])
{
  /* Check usage */
  // More than one arg OR one arg AND not a positive number?
  if ((argn > 2) || ((argn == 2) && ((PORT = atoi(argv[1])) <= 0))) {
    fprintf(stderr, "USAGE: %s [port-number]\n", argv[0]);
    return false;
  }
  // Daemonize. YOUR CODE HERE
  // Consult https://stackoverflow.com/questions/3095566/linux-daemonize
  // (1) fork, let the parent _exit successfully
  pid_t pid = fork();
  if(pid > 0){
    // switch socket slot, need to accept connection
    exit(EXIT_SUCCESS);
  }
  if (pid < 0){
    exit(EXIT_FAILURE);
  }
  // (2)  setsid
    if (setsid() < 0)
    {
      exit(EXIT_FAILURE);
    }
  // (3) fork, let the parent _exit successfully
  pid=fork();
  if(pid > 0){
    // switch socket slot, need to accept connection 
    exit(EXIT_SUCCESS);
    // switch socket slot, need to accept connection 
  }
  if (pid < 0){
    exit(EXIT_FAILURE);
  }
  /* (4) Change working directory */
  if (-1 == chdir(APP_WD)) {
    perror(APP_WD);
    return false;
  }
  chdir("/");
  // (5) umask(0)
  umask(0);
  
  // (6) dup2 of 0,1,2 to /dev/null
  dup2(open("/dev/null", O_RDONLY), STDIN_FILENO);
  dup2(open("/dev/null", O_WRONLY), STDOUT_FILENO);
  dup2(open("/dev/null", O_RDWR), STDERR_FILENO);
  
  /* Save the process ID */
  FILE *pidfile;
  if (!(pidfile = fopen(APP_PIDFILE, "w"))) {
    perror(APP_PIDFILE);
    return false;
  } else {
    fprintf(pidfile, "%d", getpid());
    fclose(pidfile);
  }
  // (6) dup2 of 0,1,2 to /dev/null
  dup2(open("/dev/null", O_RDONLY), STDIN_FILENO);
  dup2(open("/dev/null", O_WRONLY), STDOUT_FILENO);
  dup2(open("/dev/null", O_RDWR), STDERR_FILENO);
  /* Initialize the cache */
  init_cache();
  return true;
}
/* 
 * Terminate the main loop 
 * kill -USR1 `cat /tmp/app/app.pid`
 */
static void app_terminate(int signo)
{
  // Log the termination signal
  syslog(LOG_INFO, "Terminated by signal %d", signo);

  switch (signo) {
  case SIGUSR1:			// "Soft" termination
    done = true;
    break;
  case SIGUSR2:			// "Hard" termination
    exit(EXIT_SUCCESS);
  default:			// Do nothing
    break;
  }
}

// A thread worker; it simply starts `process_request`
static void *worker(void *arg) {
  bool status = process_request(*(int*)arg);
  free(arg);

  // YOUR CODE TO DECREMENT THE NUMBER OF SERVERS AND POSSIBLY START
   /* Tried to implement the bonus portion of the code EB */
 // pthread_mutex_lock(&counter_in_use);
 // if(n_servers == 0){
 //   n_servers = n_servers-1;
 // }
 // if (n_servers < MAX_SERVERS){
  //  pthread_mutex_unlock(&counter_in_use);
 // }
 // pthread_mutex_unlock(&counter_in_use);
  
  // ANOTHER WORKER HERE:
  // ...
  
  pthread_exit((void*)status);
}

// The main function 
int main(int argn, char *argv[])
{
  // Inilialize the process
  bool status = app_initialize(argn, argv);
  if (status == false)
    return EXIT_FAILURE;

  // Create a socket
  struct sockaddr_in serv_addr, cli_addr; 
  int sockfd = socket(AF_INET, SOCK_STREAM, 0);
  if (-1 == sockfd) {
    syslog(LOG_ERR, "socket: %s", strerror(errno));
    return EXIT_FAILURE;
  }

  // Bind the socket and start listening on it
  bzero((char *) &serv_addr, sizeof(serv_addr));
  serv_addr.sin_family = AF_INET;
  serv_addr.sin_addr.s_addr = INADDR_ANY;
  serv_addr.sin_port = htons(PORT);
  if (   -1 == bind(sockfd, (struct sockaddr *) &serv_addr, sizeof(serv_addr))
      || -1 == listen(sockfd, 5 /* backlog */)) {
    syslog(LOG_ERR, "bind/listen: %s", strerror(errno));
    return EXIT_FAILURE;
  }

  // Register the signal handler(s)
  signal(SIGUSR1, app_terminate);
  signal(SIGUSR2, app_terminate);

  /* Log the initialization message */
  syslog(LOG_INFO, "Started on port %d", PORT);

  /* Prepare thread attributes */
  pthread_t t;
  pthread_attr_t attr;
  pthread_attr_init(&attr); // Always succeeds
  pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
  
  // Make sure that macOS threads have enough space to run
#ifdef __APPLE__  
  if (pthread_attr_setstacksize(&attr,
				(MAX_RQ_SIZE / getpagesize() + 1)
				* getpagesize())) {
    syslog(LOG_ERR, "pthread_attr_setstacksize: %s", strerror(errno));
    return EXIT_FAILURE;
  };
#endif

  // The main loop
  while (!done) {
    unsigned clilen = sizeof(cli_addr);
    int newsockfd = accept(sockfd, (struct sockaddr *)&cli_addr, &clilen);
    if (-1 == newsockfd) { // Forget about it
      syslog(LOG_ERR, "accept: %s", strerror(errno));
      continue;
    }

    // YOUR CODE TO CHECK THE NUMBER OF SERVERS AND POSSIBLY WAIT - HERE:
    
    // ...
    
    // Create a worker to fetch and process the request
    int *newsockfd_ptr = malloc(sizeof(int));
    if(!newsockfd_ptr)  abort();	/* Not supposed to happen */
    *newsockfd_ptr = newsockfd;
    if (pthread_create(&t, &attr, worker, (void*)newsockfd_ptr)) {
      syslog(LOG_ERR, "pthread: %s", strerror(errno));
    } else {
      // YOUR CODE TO INCREMENT THE NUMBER OF SERVERS - HERE:
     // pthread_mutex_lock(&counter_in_use);
     /* Tried to implement the bonus portion of the code EB */ 
     // if(n_servers==0){
      //  n_servers = n_servers + 1;
      //  pthread_mutex_unlock(&counter_in_use);
     // }
      // ...
    }
    // DO NOT FORGET TO UNLOCK THE LOCK!
     // pthread_mutex_unlock(&counter_in_use);
    // ...
  }
  pthread_attr_destroy(&attr);
  return EXIT_SUCCESS;
}