#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <stdbool.h>
#include <sys/types.h>
#include <stdlib.h>
#include <ctype.h>
#include <signal.h>

/*
----Author: Eaton Ma ----
----   UPI: ema846   ----

*/

// declare field variables for reading input and print home pwd
int bufferLen = 10000;
char home[10000];

// declare field variables for history list
int rear = 0;
int front = -1;
char *historyList[10000];
char *bgHistoryList[10000];

// declare field variables to store background process information
int backgroundCount = 1;
int processIds[10000];
int backgroundPos = 0;
int globalid[10000];
int globalstatus[10000];
bool globalEnd[10000];

// declare field variables to store foreground process information
int foregroundCount = 1;
int globalForeground[10000];
int globalForegroundStatus[10000];
bool globalForegroundEnd[10000];
bool globalForegroundisPipe[10000];
int globalForegroundPipeNo[10000];

// declare field variables to be executed in jobs
char *jobsSearch;
char *jobsCommand[] = {"ps", "-o", "state,pid", NULL};

bool executeBuiltInCommand(char **input);
void executeSimpleCommand(char **command);
void pipeline(char **cmd, int pipeNo);
void getPipedCommands(char *command, int pipeNo, bool isBackground);
void checkbg();
void executeSimpleCommandBackground(char **command);
void pipelineBackground(char **cmd);

// this function stores the command entered by the user into the list
// it takes in the raw input from the command line
void enQueue(char *command)
{
  // front and rear points to the index of the last ten commands entered
  front++;
  historyList[front] = strdup(command);
  if (front - 9 > rear)
  {
    rear++;
  }
}

// this function stores the background command entered by the user into the list
// it takes in the raw input from the command line
void enQueueBg(char *command)
{
  bgHistoryList[backgroundCount - 1] = strdup(command);
}

// this function swaps the "h number" command for the actual command that "h number" was trying to call
// it takes in the raw input from the command line
void deQueue(char *command)
{
  historyList[front] = strdup(command);
}

// this function checks the raw input to see if it contains any pipelines
// it takes in the raw input from the command line
// the return value is the number of individual commands, therefore, it would return 1 if there is no pipeline,
// or return 2 if there is one pipeline for example "a | b"
int checkPipe(char *input)
{
  int pipeNo = 1;
  char *pipe = "|";

  // check if the input contains "|"
  if (strstr(input, pipe) != NULL)
  {
    for (int i = 0; i < strlen(input) - strlen(pipe) + 1; i++)
    {
      if (strstr(input + i, pipe) == input + i)
      {
        pipeNo++;
        i = i + strlen(pipe) - 1;
      }
    }
  }
  return pipeNo;
}

// this function parses the raw input
// it takes in the raw input from command line and a pointer to pointer char to store the parsed command
void parseFactory(char *input, char **parsedC)
{

  for (int i = 0; i < 10000; i++)
  {
    // parsing
    parsedC[i] = strsep(&input, " ");

    // when no more commands needed to be parsed, it stops
    if (parsedC[i] == NULL)
    {
      break;
    }
    // get rid of new line characters if there is any
    else if (strchr(parsedC[i], '\n') != NULL)
    {
      parsedC[i][strcspn(parsedC[i], "\n")] = 0;
    }
    // ignore space
    if (strlen(parsedC[i]) == 0)
    {
      i--;
    }
  }
}

// this function parses the raw input for background process
// it takes in the raw input from command line and a pointer to pointer char to store the parsed command
void parseFactoryBackground(char *input, char **parsedC)
{

  for (int i = 0; i < 10000; i++)
  {
    // parsing
    parsedC[i] = strsep(&input, " ");

    // once it reaches the end, it will store the last index into backgroundPos, which will be used in later functions
    // the rest is the same
    if (parsedC[i] == NULL)
    {
      backgroundPos = i - 1;
      break;
    }
    else if (strchr(parsedC[i], '\n') != NULL)
    {
      parsedC[i][strcspn(parsedC[i], "\n")] = 0;
    }
    if (strlen(parsedC[i]) == 0)
    {
      i--;
    }
  }
}

// this function changes the directory as user specifed, if nothing is specified, it brings user to the directory of ash
// it takes in parsed commands
void cd(char **cdInput)
{
  // if user only types "cd", it will change directory to home, which is the directory ash is in
  if (cdInput[1] == NULL || strspn(cdInput[1], " \r\n\t") == strlen(cdInput[1]))
  {
    chdir(home);
  }
  else
  // if user has specified the directory, it will change to that directory only if it is a valid direcotry
  {
    if (chdir(cdInput[1]) != 0)
    {
      printf("cd: %s: No such file or directory\n", historyList[front]);
    }
  }
}

// this function prints out the last ten commands user has inputed,
// if user has specified a number, it will execute the command corresponding to that number if it is a valid command
// it takes in parsed commands
void history(char **hInput)
{
  // if user did not specify any jobs numbers
  // it will just print out the last ten commands user has entered
  if (hInput[1] == NULL)
  {
    int count = 1;
    for (int i = rear; i <= front; i++)
    {
      printf("    %d: %s", count, historyList[i]);
      count++;
    }
  }
  else
  // if a number is specified, then it will execute the specified command only if it is valid
  {
    char *parsedCommandsH[10000];
    int countH = 1;
    for (int i = rear; i <= front; i++)
    {
      int task = atoi(hInput[1]);
      // check if the number is a valid number
      if (countH == task)
      {
        // change the history list to the command that will be executed
        deQueue(historyList[i]);
        char *temp;
        temp = strdup(historyList[i]);
        bool isBackground = false;
        // same thing as main, just to find which type of command it is then execute it
        if (strstr(temp, "&"))
        {
          isBackground = true;
          enQueueBg(temp);
          int pipeNo = checkPipe(temp);
          if (pipeNo > 1)
          {

            getPipedCommands(temp, pipeNo, isBackground);
          }
          else
          {

            parseFactoryBackground(temp, parsedCommandsH);
            executeSimpleCommandBackground(parsedCommandsH);
          }
          isBackground = false;
        }
        else
        {
          int pipeNo = checkPipe(temp);
          if (pipeNo > 1)
          {
            getPipedCommands(temp, pipeNo, isBackground);
          }
          else
          {
            parseFactory(temp, parsedCommandsH);
            if (executeBuiltInCommand(parsedCommandsH))
            {
            }
            else
            {
              executeSimpleCommand(parsedCommandsH);
            }
          }
          break;
        }
      }
      countH++;
    }
  }
}

// this function prints out the state of any background processes, along with the command
void jobs()
{

  int fd[2];
  pipe(fd);
  pid_t pid, wpid;
  int status;
  pid = fork();

  // here im trying to call "ps -o state,pid" to get all the processes running
  // the child process will execute the command above
  if (pid == 0)
  {
    //----child process----//
    // here im using a pipe so that the output of the ps command is redirected into a variable
    dup2(fd[1], STDOUT_FILENO);

    if (execvp(jobsCommand[0], jobsCommand) == -1)
    {
      perror("ERROR: EXECVP FAILURE");
      exit(EXIT_FAILURE);
    }
  }
  else if (pid == -1)
  {
    // Error forking
    perror("ERROR: FAILED TO FORK");
    exit(EXIT_FAILURE);
  }
  else
  {
    //----parent process----//
    wpid = waitpid(pid, &status, WUNTRACED);

    char psOutput[10000];
    memset(psOutput, 0, sizeof(psOutput));
    ssize_t size = read(fd[0], psOutput, 1000000);
    int count = 0;
    int pids[10000];
    int stop = 0;
    char state;
    int checkedP[10000];
    for (int i = 0; i < 10000; i++)
    {
      checkedP[i] = -1;
    }
    int checkedCounter = 0;
    bool unchecked = true;

    // now the variable psOutput contains all the output from ps command
    // im looping it until the end
    while (psOutput[stop] != '\0')
    {
      // if the char is a number, i will store it into a temp int array called pids
      if (psOutput[stop] >= 48 && psOutput[stop] <= 57)
      {
        pids[count] = psOutput[stop];

        // usually the output would be something like S    23131
        // a letter indicate the state of the process and its process number
        // after the last digit of the process number it will be followed by a new line chararter
        // therefore, this below if statement checks if we have reached to the last digit of the process number
        if (psOutput[stop + 1] == '\n')
        {

          count++;

          // we would loop through all background processes, to see if it has already been checked
          // what checked means will be explained down later,
          // if it has been checked, we will not check it again
          for (int j = 0; j < backgroundCount - 1; j++)
          {

            for (int i = 0; i < 10000; i++)
            {
              if (checkedP[i] == j)
              {
                unchecked = false;
                break;
              }
            }

            // basically what checked means is that it has been detected as a finish process
            // and I have printed its done statement
            if (globalEnd[j] && unchecked)
            {
              printf("    [%d] <Done>   %s", j + 1, bgHistoryList[j]);
            }

            checkedP[checkedCounter] = j;
            checkedCounter++;
            unchecked = true;

            char temp[100];
            memset(temp, 0, sizeof(temp));
            // here, temp stores the process id of executed background processes
            // the pids array stores the process id which is found from our ps command above
            // which means pids stores other process ids which is not intended to be checked
            sprintf(temp, "%d", globalid[j]);
            int recordedCount = 0;
            for (int i = 0; i < 100; i++)
            {
              if (temp[i] == '\0')
              {
                recordedCount = i;
                break;
              }
            }
            int counter = 0;

            // first of all we will check if the executed background process's process id has the same number
            // of digits as the process ids we captured from ps command
            if (recordedCount == count)
            {
              while (temp[counter] == pids[counter])
              {
                counter++;
                if (counter == count)
                {
                  int tempstop = stop;

                  // now this while loop is trying to find the letter which represents the process state,
                  // recall that the ps command output comes in "S 12312" for example, we will go index back one by one
                  // until we have reached a letter, then it will go through the below switch condition and print out
                  // correspdoning statuses
                  while (1)
                  {
                    if (psOutput[tempstop] >= 65 && psOutput[tempstop <= 90])
                    {
                      state = psOutput[tempstop];
                      switch (state)
                      {
                      case 'S':
                        printf("    [%d] <%s> %s", j + 1, "sleeping", bgHistoryList[j]);
                        break;

                      case 'D':
                        printf("    [%d] <%s> %s", j + 1, "uninterruptible sleeping", bgHistoryList[j]);
                        break;

                      case 'R':
                        printf("    [%d] <%s> %s", j + 1, "running", bgHistoryList[j]);
                        break;

                      case 'T':
                        printf("    [%d] <%s> %s", j + 1, "stopped", bgHistoryList[j]);
                        break;

                      case 'X':
                        printf("    [%d] <%s> %s", j + 1, "dying", bgHistoryList[j]);
                        break;

                      default:
                        break;
                      }
                      break;
                    }
                    else
                    {
                      tempstop--;
                    }
                  }

                  break;
                }
              }
            }
            unchecked = true;
          }

          for (int i = 0; i < count; i++)
          {
            pids[i] = 0;
          }
          count = 0;
        }
        else
        {
          count++;
        }
      }
      stop++;
    }
  }
}

// this function continues the stopped foreground process in foreground
// if user specified a job number, it will continue that specific process
// or else it will continue the last stopped process
// the function takes in parsed commands
void fg(char **fgInput)
{

  // if no job number is specified, it will send continue signal to that process
  // then wait for it to finish
  if (fgInput[1] == NULL)
  {
    kill(globalForeground[foregroundCount - 2], SIGCONT);
    waitpid(globalForeground[foregroundCount - 2], &globalForegroundStatus[foregroundCount - 2], WUNTRACED);
  }
  else
  {
    int pid = atoi(fgInput[1]);
    kill(globalForeground[pid - 1], SIGCONT);
    waitpid(globalForeground[pid - 2], &globalForegroundStatus[pid - 2], WUNTRACED);
  }
}

// this function continues the stopped foreground process in background
// if user specified a job number, it will continue that specific process
// or else it will continue the last stopped process
// the function takes in parsed commands
void bg(char **bgInput)
{

  // if no job number is specified, it will send continue signal to that process
  // then dont wait for it to finish
  if (bgInput[1] == NULL)
  {
    kill(globalForeground[foregroundCount - 2], SIGCONT);
  }
  else
  {
    int pid = atoi(bgInput[1]);
    kill(globalForeground[pid - 1], SIGCONT);
  }
}

// this function terminates the stopped foreground process
// if user specified a job number, it will terminates that specific process
// or else it will terminates the last stopped process
// the function takes in parsed commands
void killP(char **kInput)
{
  if (kInput[1] == NULL)
  {
    kill(globalForeground[foregroundCount - 2], SIGKILL);
  }
  else
  {
    int pid = atoi(kInput[1]);
    kill(globalForeground[pid - 1], SIGKILL);
  }
}

// this function reads the user input to see if it is a built in command, if it is then it will return true and execute it
// it takes parsed commands
bool executeBuiltInCommand(char **input)
{
  char *builtInCommands[7];
  builtInCommands[0] = "cd";
  builtInCommands[1] = "h";
  builtInCommands[2] = "history";
  builtInCommands[3] = "jobs";
  builtInCommands[4] = "fg";
  builtInCommands[5] = "bg";
  builtInCommands[6] = "kill";

  int command = -1;

  char *check;

  // the below code until next comment is to remove any space in between
  while (isspace((unsigned char)*input[0]))
  {
    input[0]++;
  }

  check = input[0] + strlen(input[0]) - 1;
  while (check > input[0] && isspace((unsigned char)*check))
  {
    check--;
  }

  check[1] = '\0';

  // find the number corresponding to the built-in commands
  for (int i = 0; i < 7; i++)
  {
    if (strcmp(input[0], builtInCommands[i]) == 0)
    {
      command = i;
      break;
    }
  }

  // execute built-in commands
  if (command == -1)
  {
    return false;
  }
  else if (command == 0)
  {
    cd(input);
    return true;
  }
  else if (command == 1 || command == 2)
  {
    history(input);
    return true;
  }
  else if (command == 3)
  {
    jobs();
    return true;
  }
  else if (command == 4)
  {
    fg(input);
    return true;
  }
  else if (command == 5)
  {
    bg(input);
    return true;
  }
  else if (command == 6)
  {
    killP(input);
    return true;
  }
}

// this function execute a non-pipeline command in the foreground
// it takes in parsed commands
void executeSimpleCommand(char **command)
{

  pid_t pid, wpid;
  int status;
  pid = fork();

  if (pid == 0)
  {

    //----child process----//
    if (execvp(command[0], command) == -1)
    {
      perror("ERROR: EXECVP FAILURE");
      exit(EXIT_FAILURE);
    }
  }
  else if (pid == -1)
  {
    // Error forking
    perror("ERROR: FAILED TO FORK");
    exit(EXIT_FAILURE);
  }
  else
  {
    //----parent process----//
    // stores information into arrays will be used for other functions
    globalForeground[foregroundCount - 1] = pid;
    globalForegroundEnd[foregroundCount - 1] = false;
    globalForegroundStatus[foregroundCount - 1] = status;
    globalForegroundisPipe[foregroundCount - 1] = false;
    globalForegroundPipeNo[foregroundCount - 1] = 1;
    // the parent waits for the child to change state to finish
    waitpid(pid, &status, WUNTRACED);
    globalForegroundStatus[foregroundCount - 1] = status;
    globalForegroundEnd[foregroundCount - 1] = true;
    foregroundCount++;
  }
}

// this function execute a pipeline command in the foreground
// it takes in parsed commands
void pipeline(char **command, int pipeNo)
{
  int fd[2];
  int status;
  pid_t pid, wpid;
  int fdd = 0; /* Backup */

  char *individualCommand[10000];
  int count = 0;
  int commandpos = 0;

  // char** {"ls","-l",NULL,"wc",NULL,"|"}
  // the line above shows a possible pipe line command format this function accepts
  // each individual command is parsed, then between each commands there will be a null to separate
  // at the very end of there will be a "|" to indicate it has reached to the end

  // as specified above, the terminating condition is "|"
  while (command[commandpos] != "|")
  {
    int i = 0;

    // specified above as well, the end of an individual command will be null
    while (command[count] != NULL)
    {
      // storing the parsed individual command into a temp char pointer
      individualCommand[i] = command[count];
      count++;
      i++;
    }

    count++;
    commandpos = commandpos + 1 + i;

    // sharing bidiflow
    pipe(fd);
    if ((pid = fork()) == -1)
    {
      perror("fork");
      exit(1);
    }
    else if (pid == 0)
    {
      dup2(fdd, 0);
      // if it is not the last individual command
      if (command[commandpos] != "|")
      {
        dup2(fd[1], 1);
      }
      close(fd[0]);
      execvp(individualCommand[0], individualCommand);
      exit(1);
    }
    else
    {
      globalForeground[foregroundCount - 1] = pid;
      globalForegroundEnd[foregroundCount - 1] = false;
      globalForegroundStatus[foregroundCount - 1] = status;
      globalForegroundisPipe[foregroundCount - 1] = true;
      globalForegroundPipeNo[foregroundCount - 1] = pipeNo;
      wpid = waitpid(pid, &status, WUNTRACED);
      globalForegroundStatus[foregroundCount - 1] = status;
      globalForegroundEnd[foregroundCount - 1] = true;
      foregroundCount++;
      close(fd[1]);
      fdd = fd[0];
    }

    for (int j = 0; j < i; j++)
    {
      individualCommand[j] = NULL;
    }
  }
}

// this function execute a pipeline command in the background
// it takes in parsed commands
void pipelineBackground(char **command)
{
  int fd[2];
  int status;
  pid_t pid, wpid;
  int fdd = 0;

  char *individualCommand[10000];
  int count = 0;
  int commandpos = 0;

  while (command[commandpos] != "|")
  {
    int i = 0;

    while (command[count] != NULL)
    {
      individualCommand[i] = command[count];
      count++;
      i++;
    }

    count++;
    commandpos = commandpos + 1 + i;

    // sharing bidiflow
    pipe(fd);
    if ((pid = fork()) == -1)
    {
      perror("fork");
      exit(1);
    }
    else if (pid == 0)
    {
      dup2(fdd, 0);
      if (command[commandpos] != "|")
      {
        dup2(fd[1], 1);
      }

      close(fd[0]);
      execvp(individualCommand[0], individualCommand);
      exit(1);
    }
    else
    {
      // the only difference is that the parent will not wait for the child to finish
      waitpid(pid, &status, WNOHANG);

      // if its the last individual command
      // it will store information which will be used in other functions
      if (command[commandpos] == "|")
      {
        processIds[backgroundCount - 1] = pid;
        globalid[backgroundCount - 1] = pid;
        globalstatus[backgroundCount - 1] = status;
        globalEnd[backgroundCount - 1] = false;
        printf("    [%d]    %d\n", backgroundCount, processIds[backgroundCount - 1]);
        backgroundCount++;
      }

      close(fd[1]);
      fdd = fd[0];
    }

    for (int j = 0; j < i; j++)
    {
      individualCommand[j] = NULL;
    }
  }
}

// this function parse the pipeline commands
// it takes in raw input, number of pipelines, whether the command is background or not
void getPipedCommands(char *command, int pipeNo, bool isBackground)
{

  char *splitPipe[10000];

  char *pipeSymbol = "|";
  char *endSymbol = "&";
  char *parsedCommands[10000];
  char *parsedBackgroundCommands[10000];
  int count = 0;

  // for each individual commands
  for (int i = 0; i < pipeNo; i++)
  {
    // store the unparsed commands
    splitPipe[i] = strsep(&command, pipeSymbol);
    for (int j = 0; j < 10000; j++)
    {
      // parse the individual commands
      parsedCommands[j + count] = strsep(&splitPipe[i], " ");

      if (parsedCommands[j + count] == NULL)
      {
        parsedCommands[j + count] = NULL;
        count += j;
        break;
      }
      else if (strchr(parsedCommands[j + count], '\n') != NULL)
      {
        parsedCommands[j + count][strcspn(parsedCommands[j + count], "\n")] = 0;
      }
      if (strlen(parsedCommands[j + count]) == 0)
      {
        j--;
      }
    }
    count++;
  }

  // if it is a bkacground process, then remove the ampersand
  if (isBackground)
  {
    int counter = 0;
    for (int i = 0; i < count; i++)
    {
      if (i != count - 2)
      {
        parsedBackgroundCommands[counter] = parsedCommands[i];
        counter++;
      }
    }
    parsedBackgroundCommands[count - 1] = pipeSymbol;
  }
  else
  {
    parsedCommands[count] = pipeSymbol;
  }

  if (isBackground)
  {
    pipelineBackground(parsedBackgroundCommands);
  }
  else
  {
    pipeline(parsedCommands, pipeNo);
  }
}

// this function execute a non-pipeline command in the background
// it takes parsed commands
void executeSimpleCommandBackground(char **command)
{

  pid_t pid, rpid;
  int status;
  pid = fork();
  processIds[backgroundCount - 1] = pid;
  command[backgroundPos] = NULL;

  if (pid == 0)
  {
    //----child process----//
    if (execvp(command[0], command) == -1)
    {

      perror("ERROR: EXECVP FAILURE");
      exit(EXIT_FAILURE);
    }
  }
  else if (pid == -1)
  {
    // Error forking
    perror("ERROR: FAILED TO FORK");
    exit(EXIT_FAILURE);
  }
  else
  {
    //----parent process----//
    // the parent does not wait for child process to finish
    waitpid(pid, &status, WNOHANG);
    globalid[backgroundCount - 1] = pid;
    globalstatus[backgroundCount - 1] = status;
    globalEnd[backgroundCount - 1] = false;
    printf("    [%d]    %d\n", backgroundCount, processIds[backgroundCount - 1]);
    backgroundCount++;
  }
}

// this function checks if any background process is finished
void checkbg()
{
  pid_t rpid;

  for (int i = 0; i < backgroundCount - 1; i++)
  {
    rpid = waitpid(globalid[i], &globalstatus[i], WNOHANG);

    if (rpid == globalid[i] && !globalEnd[i])
    {
      printf("    [%d] <Done>   %s", i + 1, bgHistoryList[backgroundCount - 2]);
      globalEnd[i] = true;
    }
  }
}

// handles ctrl-z
void signalHandler(int sig_num)
{
  signal(SIGTSTP, signalHandler);
}

int main()
{

  char input[10000];
  char *parsedCommands[10000];
  char *checkHistory[10000];
  bool isBackground = false;
  printf("\033[H\033[J");
  getcwd(home, sizeof(home));
  signal(SIGTSTP, signalHandler);

  while (1)
  {
    checkbg();
    printf("ash>");

    // read from user input
    if (!fgets(input, bufferLen, stdin))
    {
      exit(EXIT_FAILURE);
    }

    // if its empty line, continue the while loop
    if (input[0] == '\n' && input[1] == '\0')
    {
      continue;
    }
    else
    {
      // for reading test files
      if (isatty(STDIN_FILENO) == 0)
      {
        printf("%s", input);
      }
      // add the command into history list
      enQueue(input);

      // check if its a background process
      if (strstr(input, "&"))
      {
        isBackground = true;
        // add the command into background history list
        enQueueBg(input);
        // check if it is a pipeline
        int pipeNo = checkPipe(input);
        if (pipeNo > 1)
        {
          getPipedCommands(input, pipeNo, isBackground);
        }
        else
        {
          parseFactoryBackground(input, parsedCommands);
          executeSimpleCommandBackground(parsedCommands);
        }
        isBackground = false;
        continue;
      }

      // if its not a background process, proceed the same
      int pipeNo = checkPipe(input);
      if (pipeNo > 1)
      {

        getPipedCommands(input, pipeNo, isBackground);
      }
      else
      {

        parseFactory(input, parsedCommands);

        if (executeBuiltInCommand(parsedCommands))
        {
          continue;
        }
        else
        {

          executeSimpleCommand(parsedCommands);
          continue;
        }
      }
    }
  }

  return 0;
}