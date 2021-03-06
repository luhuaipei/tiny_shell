/***************************************************************************
 *  Title: Runtime environment 
 * -------------------------------------------------------------------------
 *    Purpose: Runs commands
 *    Author: Stefan Birrer
 *    Version: $Revision: 1.1 $
 *    Last Modification: $Date: 2006/10/13 05:24:59 $
 *    File: $RCSfile: runtime.c,v $
 *    Copyright: (C) 2002 by Stefan Birrer
 ***************************************************************************/
/***************************************************************************
 *  ChangeLog:
 * -------------------------------------------------------------------------
 *    $Log: runtime.c,v $
 *    Revision 1.1  2005/10/13 05:25:59  sbirrer
 *    - added the skeleton files
 *
 *    Revision 1.6  2002/10/24 21:32:47  sempi
 *    final release
 *
 *    Revision 1.5  2002/10/23 21:54:27  sempi
 *    beta release
 *
 *    Revision 1.4  2002/10/21 04:49:35  sempi
 *    minor correction
 *
 *    Revision 1.3  2002/10/21 04:47:05  sempi
 *    Milestone 2 beta
 *
 *    Revision 1.2  2002/10/15 20:37:26  sempi
 *    Comments updated
 *
 *    Revision 1.1  2002/10/15 20:20:56  sempi
 *    Milestone 1
 *
 ***************************************************************************/
#define __RUNTIME_IMPL__

/************System include***********************************************/
#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <fcntl.h>
#include <signal.h>

/************Private include**********************************************/
#include "runtime.h"
#include "io.h"

/************Defines and Typedefs*****************************************/
/*  #defines and typedefs should have their names in all caps.
 *  Global variables begin with g. Global constants with k. Local
 *  variables should be in all lower case. When initializing
 *  structures and arrays, line everything up in neat columns.
 */

/************Global Variables*********************************************/

#define NBUILTINCOMMANDS (sizeof BuiltInCommands / sizeof(char*))

typedef struct bgjob_l {
  int jobid;
  pid_t pid;
  int status;  // running 0, Done 1, suspend 2
  char* name;
  commandT* cmd;
  struct bgjob_l* next;
} bgjobL;

/* the pids of the background processes */
bgjobL *bgjobs;
/************Function Prototypes******************************************/
/* run command */
static void RunCmdFork(commandT*, bool);
/* runs an external program command after some checks */
static void RunExternalCmd(commandT*, bool);
/* resolves the path and checks for exutable flag */
static bool ResolveExternalCmd(commandT*);
/* forks and runs a external program */
static void Exec(commandT*, bool);
/* runs a builtin command */
static void RunBuiltInCmd(commandT*);
/* checks whether a command is a builtin command */
//static bool IsBuiltIn(char*);
static void Printjoblist();
static void addToBg(int pid,commandT* cmd,int stpStatus);
static void delFromBg(int pid,commandT* cmd);
bgjobL* FindJobid(pid_t pid);
void updateJobId();
void RunCmdRedirInOut(commandT* cmd, char* file_in,char* file_out);
/************External Declaration*****************************************/

/**************Implementation***********************************************/
int total_task;
void RunCmd(commandT** cmd, int n)
{
  int i;
  total_task = n;
  if(n == 1){
    RunCmdFork(cmd[0], TRUE);

  }else{
    RunCmdPipe(cmd[0], cmd[1]);  //Qest: only two task allowed ?
    for(i = 0; i < n; i++)
      ReleaseCmdT(&cmd[i]);
  }
}

void RunCmdFork(commandT* cmd, bool fork)
{

  if (cmd->argc<=0)
    return;
  if (ResolveExternalCmd(cmd))//IsBuiltIn(*(cmd[0].argv))) //is builtin?
  {
    RunBuiltInCmd(cmd);
  }
  else
  {
    RunExternalCmd(cmd, fork);

  }
}
void GetBgpToFg(commandT* cmd){
  //Implementation of fg command
  bgjobL *temp = bgjobs;
  int status;
  while(temp!=NULL){
    if(temp->jobid == atoi(cmd->argv[1]))
    {
      kill(-temp->pid,SIGCONT);
      fgpid = temp->pid;
      commandT* cmd_new;
      cmd_new = temp->cmd;
      delFromBg(temp->pid,cmd);
      waitpid(temp->pid,&status,WUNTRACED);
      if(WTERMSIG(status) == 127){
        fgpid=-1;
        addToBg(temp->pid,cmd_new,2);
        return;
      }
    }
    temp = temp->next;
  }
}
void RunCmdBg(commandT* cmd)
{
  // Implementation of bg command
  bgjobL *temp = bgjobs;
    while(temp!=NULL){
      if(temp->jobid == atoi(cmd[0].argv[1]))
      {
        kill(-temp->pid,SIGCONT);
        temp->status = 0;
        strcat(temp->name," &");
        return;
      }
      temp = temp->next;    
    }
  
}
void RunCmdPipe(commandT* cmd1, commandT* cmd2)
{

}

void RunCmdRedirOut(commandT* cmd, char* file)
{
  int out;
  out = open(file,O_WRONLY|O_CREAT,0666); // Should also be symbolic values for access rights
  dup2(out,STDOUT_FILENO);
  close(out);

}

void RunCmdRedirIn(commandT* cmd, char* file)
{
  int in;
  in = open(file,O_RDONLY);
  dup2(in,STDIN_FILENO);
  close(in);
}

void RunCmdRedirInOut(commandT* cmd, char* file_in,char* file_out){
  int in;
  int out;
  in = open(file_in,O_RDONLY);
  dup2(in,STDIN_FILENO);
  close(in);
  out = open(file_out,O_WRONLY|O_CREAT,0666); // Should also be symbolic values for access rights
  dup2(out,STDOUT_FILENO);
  close(out);
}
/*Try to run an external command*/
static void RunExternalCmd(commandT* cmd, bool fork)
{
 // printf("---------------------In RunExternalCmd------------------------\n");
  //bg,fg,cd
  if (strcmp(cmd->argv[0],"fg") ==0 ){

    GetBgpToFg(cmd);
    return;
  }
  if (strcmp(cmd->argv[0],"bg") == 0){
    RunCmdBg(cmd);
    return;    
  }else if(strcmp(cmd->argv[0],"jobs") == 0){
    Printjoblist();
    return;
  }
  if (ResolveExternalCmd(cmd)){
    Exec(cmd, fork);
  }
  else {
    printf("%s: command not found\n", cmd->argv[0]);
    fflush(stdout);
    ReleaseCmdT(&cmd);
  }
 // printf("---------------------out RunExternalCmd------------------------\n");
}

/*Find the executable based on search list provided by environment variable PATH*/
static bool ResolveExternalCmd(commandT* cmd)
{
  char *pathlist, *c;
  char buf[1024];
  int i, j;
  struct stat fs;

  if(strchr(cmd->argv[0],'/') != NULL){
    if(stat(cmd->argv[0], &fs) >= 0){
      if(S_ISDIR(fs.st_mode) == 0)
        if(access(cmd->argv[0],X_OK) == 0){/*Whether it's an executable or the user has required permisson to run it*/
          cmd->name = strdup(cmd->argv[0]);
          return TRUE;
        }
    }
    return FALSE;
  }
  pathlist = getenv("PATH");
  //printf("%s",pathlist);
  if(pathlist == NULL) return FALSE;
  i = 0;
  while(i<strlen(pathlist)){
    c = strchr(&(pathlist[i]),':');
    if(c != NULL){
      for(j =  0; c != &(pathlist[i]); i++, j++)
        buf[j] = pathlist[i];
      i++;
    }
    else{
      for(j = 0; i < strlen(pathlist); i++, j++)
        buf[j] = pathlist[i];
    }
    buf[j] = '\0';
    strcat(buf, "/");
    strcat(buf,cmd->argv[0]);
    if(stat(buf, &fs) >= 0){
      if(S_ISDIR(fs.st_mode) == 0)
        if(access(buf,X_OK) == 0){/*Whether it's an executable or the user has required permisson to run it*/
          cmd->name = strdup(buf); 
          return TRUE;
        }
    }
  }
  return FALSE; /*The command is not found or the user don't have enough priority to run.*/
}

static void Exec(commandT* cmd, bool forceFork)
{
//  printf("------------------------------In Exec-----------------------------------\n");
  sigset_t mask;
  sigemptyset(&mask);
  sigaddset(&mask,SIGCHLD);
  int pid = fork();
  int status;
  if (pid < 0){
    perror("Exec fork fail");
  }else if( pid == 0 ){
    setpgid(0, 0);
    execv(cmd[0].argv[0],cmd[0].argv);
  }else{
      sigprocmask(SIG_UNBLOCK,&mask,NULL);
      if(cmd[0].bg==0){
      fgpid = pid;
      waitpid(pid,&status,WUNTRACED);
      if(WTERMSIG(status) == 127){
        // stopped by SIGTSTP
        fgpid=-1;
        //add it to backbground 
        addToBg(pid,cmd,2);
      }
      return;
    }else if (cmd[0].bg==1){
      //run command background
      //add the bg process to the bgjoblist
      addToBg(pid,cmd,0);
    }
  }
}

static void addToBg(int pid,commandT* cmd,int stpStatus){
      char *namebuf = (char*)malloc(sizeof(char)*50);
      bgjobL *new_node; 
      new_node = (bgjobL *)malloc(sizeof(bgjobL));
      strcat(namebuf,cmd->cmdline);
      if(cmd->bg==1){
        strcat(namebuf,"&");
        strcat(namebuf,"\0");
      }
      new_node->name = namebuf;  
      new_node->pid = pid;
      new_node->next = NULL;
      new_node->status = stpStatus;
      new_node->cmd = cmd;
      bgjobL* current = bgjobs;

      if (current==NULL){
        bgjobs = new_node;
        bgjobs->jobid = 1;
        if(stpStatus==2){
        int i=0;
        for(i=0; i < strlen(new_node->name);i++){
          if (new_node->name[i]=='&')
            new_node->name[i] = '\0'; 
        }
 
          printf("\n[%d]   Stopped                 %s\n",bgjobs->jobid,bgjobs->name);
	  fflush(stdout);
	}
        return;
      }
      while(current->next!= NULL){
        current = current->next;
      }
      current->next = new_node;
      current->next->jobid = current->jobid + 1;
      if (stpStatus==2){
        int i=0;
        for(i=0; i < strlen(new_node->name);i++){
          if (new_node->name[i]=='&')
            new_node->name[i] = '\0'; 
        }
        printf("\n[%d]   Stopped                 %s\n",current->next->jobid,current->next->name);
	  fflush(stdout);
	}
}
static void delFromBg(int pid,commandT* cmd){
      bgjobL* current = bgjobs;
      bgjobL* last_node = bgjobs;
      while(current != NULL){
        if (pid == bgjobs->pid){
          // Job ID
          if (bgjobs->next!=NULL){
            bgjobs->next->jobid -= 1;
          }

          bgjobs = bgjobs->next;
          last_node = bgjobs; 
          }else{
            if (pid == current->pid){
            //delete and update next 
              last_node->next = current->next;              
              bgjobL * next_node = current->next;
              while(next_node != NULL){
                next_node -> jobid -= 1;
               next_node = next_node->next;          
              }
            }
          }
          current = current->next;
    }
};


static void RunBuiltInCmd(commandT* cmd)
{
  
  int status;
  sigset_t mask;
  sigemptyset(&mask);
  sigaddset(&mask,SIGCHLD);
  int pid=fork();
  if(pid == 0)
  {
    if ((cmd->is_redirect_in==1) && (cmd->is_redirect_out==1)){
      RunCmdRedirInOut(cmd,cmd->redirect_in,cmd->redirect_out);
    }else if (cmd->is_redirect_in==1){
      RunCmdRedirIn(cmd,cmd->redirect_in);
    }else if(cmd->is_redirect_out==1){
      RunCmdRedirOut(cmd,cmd->redirect_out);
    }
    setpgid(0, 0);
    execv(cmd->name , cmd->argv);
  }else if(pid < 0)
    perror("Fork Error, in RunBuiltInCmd");
  else {
        {
          if(cmd[0].bg==0){
            fgpid = pid;
            waitpid(pid,&status,WUNTRACED);
          if(WTERMSIG(status) == 127){
                  // stopped by SIGTSTP
                  fgpid=-1;
                  //add it to backbground
                  addToBg(pid,cmd,2);            
          }
            return;
          }else if (cmd[0].bg==1){
            sigprocmask(SIG_UNBLOCK,&mask,NULL);
            //run command background
            //add the bg process to the bgjoblist
            addToBg(pid,cmd,0);
          }
        }
    }
}
bgjobL* FindJobid(int pid){
  bgjobL* current = bgjobs;
  while(current!=NULL){
    if (current->pid == pid){
      return current;
    }
    current = current->next;
  }
  return NULL;
}

void Printjoblist(){
  bgjobL* current = bgjobs;
  char *tempStatus="";
  while(current != NULL)
  {
    if (current->status==0)
      tempStatus = "Running";
    else if (current->status == 1)
      tempStatus = "Done   ";
    else
      tempStatus = "Stopped";
    printf("[%d]   %s                 %s\n",current->jobid,tempStatus ,current->name);
    fflush(stdout);
    current = current->next;
  }
}
void updateJobId(){
  bgjobL* i = bgjobs;
  int y=1;
  while(i!=NULL){
    i->jobid = y;
    y++;
    i = i->next;
  }
  free(i);
}
void CheckJobs()
{
  //goes through the list of your jobs, remove jobs that finished, 
  //and print that a "Done" status for any background job. 
  int status;
  if(bgjobs == NULL){
    return;
  }
  bgjobL* current = bgjobs;
  bgjobL* last_node = bgjobs;
  pid_t return_pid;
  //debug
  while(current!= NULL){
    return_pid = waitpid(current->pid,&status,WNOHANG);//WCONTINUED);
    if(return_pid == -1){
      /* error */
      current = NULL;
    }else if(return_pid == 0) {
      /* child is still running */
      if(WIFSTOPPED(status)==TRUE){
        current->status = 2;  //suspend
      }
      last_node = current;
      current = current -> next;
    }else if(return_pid == current->pid) {
      /* child is finished. update joblist*/
      if (return_pid == bgjobs->pid){
        //Job ID
        current->status = 1 ; //DONE
        int i=0;
        for(i=0; i < strlen(bgjobs->name);i++){
          if (bgjobs->name[i]=='&')
            bgjobs->name[i] = '\0'; 
        }
        printf("[%d]   Done                    %s\n",bgjobs->jobid,bgjobs->name);    
	fflush(stdout);
        bgjobs = bgjobs->next;
        last_node = bgjobs;
        current = bgjobs;          
        }else{
        //delete and update next 
        int i=0;
        for(i=0; i < strlen(current->name);i++){
          if (current->name[i]=='&')
            current->name[i] = '\0'; 
        }
        printf("[%d]   Done                    %s\n",current->jobid,current->name);        
	fflush(stdout);
          last_node->next = current->next;
          bgjobL * next_node = current->next;
          while(next_node != NULL){
            next_node = next_node->next;          
          }
          current = current->next;
        }
    }
  }
}


commandT* CreateCmdT(int n)
{
  int i;
  commandT * cd = malloc(sizeof(commandT) + sizeof(char *) * (n + 1));
  cd -> name = NULL;
  cd -> cmdline = NULL;
  cd -> is_redirect_in = cd -> is_redirect_out = 0;
  cd -> redirect_in = cd -> redirect_out = NULL;
  cd -> argc = n;
  for(i = 0; i <=n; i++)
    cd -> argv[i] = NULL;
  return cd;
}

/*Release and collect the space of a commandT struct*/
void ReleaseCmdT(commandT **cmd){
  int i;
  if((*cmd)->name != NULL) free((*cmd)->name);
  if((*cmd)->cmdline != NULL) free((*cmd)->cmdline);
  if((*cmd)->redirect_in != NULL) free((*cmd)->redirect_in);
  if((*cmd)->redirect_out != NULL) free((*cmd)->redirect_out);
  for(i = 0; i < (*cmd)->argc; i++)
    if((*cmd)->argv[i] != NULL) free((*cmd)->argv[i]);
  free(*cmd);
}
