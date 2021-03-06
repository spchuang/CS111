// UCLA CS 111 Lab 1 main program
#include "command.h"
#include "command-internals.h"
#include "alloc.h"

#include <errno.h>
#include <error.h>
#include <getopt.h>
#include <stdio.h>
#include <stdlib.h> 
#include <sys/wait.h>
#include <sys/types.h>
#include <unistd.h>


static char const *program_name;
static char const *script_name;

//static variables
extern struct depend **depend_list;
extern int total_cmd;
extern char **file_list;
extern int total_file;

static void
usage (void)
{
  error (1, 0, "usage: %s [-pt] SCRIPT-FILE", program_name);
}

static int
get_next_byte (void *stream)
{
  return getc (stream);
}

bool no_dependency(int cmd_num)
{
  int i, j;
  for(i=0; i<total_file; i++){
    if(depend_list[cmd_num][i].output > 0){
      for(j=cmd_num-1; j>=0; j--){
        if(depend_list[j][i].input > 0 || depend_list[j][i].output > 0){
          return false;
        }
      }
    }else if(depend_list[cmd_num][i].input > 0){
      for(j=cmd_num-1; j>=0; j--){
        if(depend_list[j][i].output > 0){
          return false;
        }
      }    
    }
  }
  return true;
} 

void update_dependency(int cmd_num)
{
  int i;
  for(i=0; i<total_file; i++){
    depend_list[cmd_num][i].input = 0;
    depend_list[cmd_num][i].output = 0;
  }
  return;
}

int
main (int argc, char **argv)
{
  total_cmd = 0;
  total_file = 0;
  int command_number = 1;
  bool print_tree = false;
  bool time_travel = false;
  program_name = argv[0];

  for (;;)
    switch (getopt (argc, argv, "pt"))
      {
      case 'p': print_tree = true; break;
      case 't': time_travel = true; break;
      default: usage (); break;
      case -1: goto options_exhausted;
      }
 options_exhausted:;

  // There must be exactly one file argument.
  if (optind != argc - 1)
    usage ();

  script_name = argv[optind];
  FILE *script_stream = fopen (script_name, "r");
  if (! script_stream)
    error (1, errno, "%s: cannot open", script_name);
  command_stream_t command_stream =
    make_command_stream (get_next_byte, script_stream);

  command_t last_command = NULL;
  command_t command;
  
  if(!time_travel){
    while ((command = read_command_stream (command_stream)))
    {
      if (print_tree){
	    printf ("# %d\n", command_number++);
	    print_command (command);
	  }else{
	    last_command = command;
	    execute_command (command, time_travel);
	  }
    }
  }else{
    
    int i;
    //list for child process id. Each index represent the corresponding complete command
    // -1: executed & one
    //  0: hasn't been executed
    // >0: child executing
    pid_t *cpid_list = (pid_t*) checked_malloc(sizeof(pid_t) * total_cmd);
    for(i=0; i<total_cmd; i++){
      cpid_list[i] = 0;
    }
    bool done = false;
    //while there are still commands waiting to be run
    while(!done){
      cc_node_t temp_cmd = get_root(command_stream);
      int c_count = 0;
      while(temp_cmd){
        pid_t cpid;
          
        //fork and execute commands with no dependency problem and that the command hasn't been run
        if(cpid_list[c_count] ==0 && no_dependency(c_count)){
           cpid = fork();
           //child execute the command
           if(cpid == 0){
            execute_command (temp_cmd->c, time_travel);
            exit(0); //child exit
           }else if(cpid >0){
            //parent add child pid to the global array
            cpid_list[c_count] = cpid;

           }else{
            error(1, 0, "Forking process failed");  
           }
        
        }
        temp_cmd = temp_cmd->next;
        c_count++;
      
      }
      //wait for child
      for(i=0; i<total_cmd; i++){
        //if the child process is runnign the ith command
        if(cpid_list[i] >0){
          int status;
          waitpid(cpid_list[i], &status, 0);
          //update the dependency list (remove the ith row or something)
          update_dependency(i);
          //set the cpid to -1
          cpid_list[i] = -1;
        }
      }
      //if there are no more command
      done =  true;
      for(i=0; i<total_cmd; i++){
        if(cpid_list[i]==0){
          done = false;
        }
      }
    }
    free(cpid_list);
    while ((command = read_command_stream (command_stream))){}
    //deallocate dependcy lists
    int k;
    for(k=0; k<total_cmd; k++){
      free(depend_list[k]);
    }
    free(depend_list);
    free(file_list);
  }

  return print_tree || !last_command ? 0 : command_status (last_command);
}
