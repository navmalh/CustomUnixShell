#include<stdio.h>
#include<stdlib.h>
#include<string.h>
#include<unistd.h>
#include<sys/wait.h>
#include<sys/types.h>
#include<sys/stat.h>
#include<fcntl.h>

// CommandType enum type is used to indicate
// the type of shell operator used in the
// user command.
enum CommandType
{
	Simple,				// there is no pipe or redirection in user command
	Output_Redirect,	// user is using > operator in the command
	Output_Append,		// user is using >> operator in the command
	Input_Redirect,		// user is using < operator in the command
	Piped_Chain,		// user is using one or more | operators in the command
	Double_Pipe,		// user is using || operator whose output should go to two other comma separated commands
	Triple_Pipe,		// user is using ||| operator whose output should go to three other comma separated commands
	Command_History		// user is trying to execute "cmdhist" command which mimics "history" command of some shells
};

// CommandInfo structure type is used to
// contain command related info associated
// with the user command.
typedef struct
{
	enum CommandType type;			// type of shell operator used in the user command
	char filename[256];				// file name in case of redirection operators (to/from which redirection needs to be done)
} CommandInfo;

// CommandHistNode structure type is used as one
// node of the command history linked list.
typedef struct CommandHistNode
{
	int iSequenceNo;				// sequence number starting from 1
	char command[256];				// actual command is stored here
	struct CommandHistNode *next;	// pointer to the next node
} CommandHistNode;

// Linked-list to store all the command history
CommandHistNode* cmdHistList = NULL;

// getUserInput() will take the command with
// arguments as entered by the user and return
// the same.
char* getUserInput()
{
	char *input = NULL;
	size_t BUFFSIZE = 0;

	// Read the line from the user
	if (getline(&input, &BUFFSIZE, stdin) == -1)
	{
		// Error in getline()
		perror("getline() error: ");
	}

	return input;
}

// getArguments() function will take the whole
// command ("input" parameter) and split it into
// tokens (arguments) and return the same.
// Additionally, an output parameter "pCommand"
// is returned to indicate if any redirection
// is part of the command & any related info.
char** getArguments(char *input, CommandInfo *pCommand)
{
	const int BUFF_SIZE = 128;
	char **arguments = malloc(BUFF_SIZE * sizeof(char *));
	char *delimiters = " \t\n";		// use these as delimiters for strtok()
	
	// Assume simple command to begin with
	// i.e. no pipe or redirection operator
	pCommand->type = Simple;
	strcpy(pCommand->filename, "");
	
	// Use strtok() to split the "input" parameter into
	// tokens (arguments)
	char *token = strtok(input, delimiters);
	int iPos = 0;

	// iRedirection is a flag variable to indicate that
	// a redirection operator viz. >, >> or < has been
	// found and file name is also processed, so we don't
	// expect any more arguments in the input.
	int iRedirection = 0;

	// Get all the tokens (arguments) from "input"
	// by using strtok() in a loop
	while (token && (!iRedirection))
	{
		// Handle special cases first
		if (strcmp(token, ">") == 0)
		{
			// Set the command type. No need
			// to save it in arguments.
			pCommand->type = Output_Redirect;
		}
		else if (strcmp(token, ">>") == 0)
		{
			// Set the command type. No need
			// to save it in arguments.
			pCommand->type = Output_Append;
		}
		else if (strcmp(token, "<") == 0)
		{
			// Set the command type. No need
			// to save it in arguments.
			pCommand->type = Input_Redirect;
		}
		else if (strcmp(token, "|") == 0)
		{
			// Set the command type.
			pCommand->type = Piped_Chain;

			// Store | symbol as a normal token, we will
			// segregate the chained commands later.
			arguments[iPos++] = token;
		}
		else if (strcmp(token, "||") == 0)
		{
			// Set the command type.
			pCommand->type = Double_Pipe;

			// Store || symbol as a normal token, we will
			// segregate the chained commands later.
			arguments[iPos++] = token;
		}
		else if (strcmp(token, "|||") == 0)
		{
			// Set the command type.
			pCommand->type = Triple_Pipe;

			// Store ||| symbol as a normal token, we will
			// segregate the chained commands later.
			arguments[iPos++] = token;
		}
		else if (strcmp(token, "cmdhist") == 0)
		{
			// Set the command type.
			pCommand->type = Command_History;

			// Store "cmdhist" symbol as a normal token, we
			// will segregate the chained commands later.
			arguments[iPos++] = token;
		}
		else
		{
			if ( (pCommand->type == Output_Redirect) ||
				 (pCommand->type == Output_Append) ||
				 (pCommand->type == Input_Redirect) )
			{
				// We are here because previous token
				// was a redirection operator. So this
				// must be the last token now which is
				// the file name. After that, we do not
				// expect any more tokens, so it will be
				// safer to exit this loop by setting
				// the iRedirection flag to 1. No need
				// to save the file name in arguments,
				// we just need to save it in pCommand.
				strcpy(pCommand->filename, token);
				iRedirection = 1;
			}
			else
			{
				// This is a normal token or argument.
				// E.g. in a command like "ls -l",
				// "ls" is one token, "-l" is another
				// token etc. Store this token (argument)
				// in the arguments array.
				arguments[iPos++] = token;
			}
		}

		// Get the next token (argument)
		// by using strtok().
		token = strtok(NULL, delimiters);
	}

	// arguments should be a NULL terminated array
	arguments[iPos] = NULL;

	return arguments;
}

// printArguments() function is just used for
// debugging purposes. It simply prints all the
// arguments present in "args" parameter (including
// the last NULL argument).
void printArguments(char **args)
{
	int iPos = 0;
	int iNullFound = 0;

	printf("Naveen: Printing all arguments.\n");

	while (!iNullFound)
	{
		// Print the argument
		printf("args[%d] = %s\n", iPos, args[iPos]);

		// Check if we have just printed the last
		// i.e. the NULL argument. If so, we should
		// exit this loop by setting the flag.
		if (args[iPos] == NULL)
			iNullFound = 1;

		// Go to the next argument.
		iPos++;
	}
}

// printExecutionStatus() function will print the
// pid and status as per parameters passed to it.
void printExecutionStatus(pid_t pid, int status)
{
	printf("\nPID of completed command = %d and ", pid);
	printf("Status of completed command = %d.\n", status);
}

// executeNormal() function is used for execution of a
// normal command without any redirection etc. The output
// is simply printed on the shell prompt. E.g. ls -l
void executeNormal(char **args)
{
	// Create a child process
	pid_t pid = fork();

	if (pid < 0)
	{
		perror("Error creating child process: ");
	}
	else if (pid == 0)
	{
		// In the child procss, simply run the command
		// by calling execvp() system call.
		execvp(args[0], args);
	}
	else
	{
		// In the parent process, wait for the child
		// process to complete
		int status;
		wait(&status);

		// Print the PID and the status of the command
		// (child process) just completed.
		printExecutionStatus(pid, status);
	}
}

// executeOutputRedirect() function is used for execution of
// a command which uses > or >> operators to redirect/append
// output to a file.
//
// E.g. ls -l > abc.txt
// or
// ls -l >> abc.txt
//
// The filename is passed as a parameter to this function.
// The iAppend parameter is a flag which should be passed
// as 0 in case of > operator and as 1 in case of >> operator.
void executeOutputRedirect(char **args, char *filename, int iAppend)
{
	// Create pipe for communication
	int p[2];
	pipe(p);

	// Create a child process
	pid_t pid = fork();

	if (pid < 0)
	{
		perror("Error creating child process: ");
	}
	else if (pid == 0)
	{
		close(p[0]);  // we don't need read end of pipe in child process, we only want to write in pipe in child process
		dup2(p[1], STDOUT_FILENO);  // close standard output and write to write end of pipe instead
		execvp(args[0], args);		// run the command, it will write to write end of pipe now
	}
	else
	{
		// Use the read end of pipe in the parent process
		// here and pass the data read from pipe into a new
		// file.
		close(p[1]);	// we don't need write end of pipe in parent process, we only want to read from pipe in parent process
		
		int status;
		wait(&status);	// wait for the child process to finish i.e. wait for command to execute and write to the pipe, so that you can read it

		// Now child has executed and written to the pipe.
		// Simply read from the pipe now and store it in
		// "buff" character buffer.
		char buff[4096];
		int iNumRead = read(p[0], buff, sizeof(buff));

		int iFlags;

		if (iAppend)
		{
			// File is already existing, we need to
			// append to existing file.
			iFlags = O_WRONLY | O_APPEND;
		}
		else
		{
			// File may or may not exist, we will create
			// new file or overwrite existing one if it
			// already exists.
			iFlags = O_CREAT | O_WRONLY | O_TRUNC;
		}

		// Open the file to which we should redirect output
		int fd = open(filename, iFlags, 0644);

		// Write the contents of "buff" to the output file
		write(fd, buff, iNumRead);

		// Print the PID and the status of the command
		// (child process) just completed.
		printExecutionStatus(pid, status);
	}
}

// executeInputRedirect() function is used for execution of
// a command which uses < operator to read input from an
// existing file. E.g. wc -l < abc.txt
// The filename is passed as a parameter to this function.
void executeInputRedirect(char **args, char *filename)
{
	// Create a child process
	pid_t pid = fork();

	if (pid < 0)
	{
		perror("Error creating child process: ");
	}
	else if (pid == 0)
	{
		// In child process, let's read the contents
		// of input file and use the same to run the
		// command. Don't use standard input for running
		// the command.
		int fd = open(filename, O_RDONLY);  // open the input file in read-only mode
		dup2(fd, STDIN_FILENO);	// close standard input and read from input file instead
		execvp(args[0], args);		// run the command, it will read from the file now
	}
	else
	{
		// In the parent process, wait for the child
		// process to complete
		int status;
		wait(&status);

		// Print the PID and the status of the command
		// (child process) just completed.
		printExecutionStatus(pid, status);
	}
}

// splitChainedCommands() is a helper function to split
// the input array of strings into multiple arrays of strings.
// E.g. a command like "ls -l | wc -w | wc" will be present
// in input parameter "args" as { "ls, "-l", "|", "wc", "-w", "|", "wc", NULL }.
// However, this can't be processed as a single command,
// we need to split "args" into multiple arrays of strings
// E.g.  arguments[0] = { "ls", "-l", NULL }
//       arguments[1] = { "wc", "-w", NULL }
//       arguments[2] = { "wc", NULL }
char*** splitChainedCommands(char **args, int *pNumCommands)
{
	const int BUFF_SIZE = 128;
	char ***arguments = malloc(BUFF_SIZE * sizeof(char **));
	int i = 0;
	int j = 0;
	int iArgsPos = 0;
	int iNullFound = 0;
	*pNumCommands = 1;  // this will be the number of pipes plus one

	while (!iNullFound)
	{
		if (j == 0)
			arguments[i] = malloc(BUFF_SIZE * sizeof(char *));

		if (args[iArgsPos] == NULL)
		{
			// This is end of the last command,
			// so no more arguments after this.
			arguments[i][j] = NULL;
			iNullFound = 1;  // flag is set to exit the while loop
		}
		else if (strcmp(args[iArgsPos],"|") == 0)
		{
			// This is end of one command
			arguments[i][j] = NULL;

			// Reset i and j for next command
			i++;
			j = 0;

			// Increment the number of piped commands
			(*pNumCommands)++;
		}
		else
		{
			// Store the command argument and
			// increment j for next argument.
			arguments[i][j++] = args[iArgsPos];
		}

		// Go to the next argument.
		iArgsPos++;
	}

	return arguments;
}

// executeSingleCommand() is used to run a given command as a child process.
// The given command is passed as "args" parameter to this function.
// This function returns the PID and the status of the executed child
// process. It can do input/output from any file descriptors given to it.
int executeSingleCommand(char **args, int fdin, int fdout, pid_t *pidCommand)
{
	pid_t pid;
	int status;

	// Create a child process to run the given command.
	pid = fork();

	if (pid == 0)
    {
		// Redirect standard input to fdin
		// if it is required.
		if (fdin != STDIN_FILENO)
			dup2(fdin, STDIN_FILENO);

		// Redirect standard output to fdin
		// if it is required.
		if (fdout != STDOUT_FILENO)
			dup2(fdout, STDOUT_FILENO);

		// Now run the given command as a child process
		execvp(args[0], args);
    }

	// Parent process waits for child to complete.
	wait(&status);
	*pidCommand = pid;
	return status;
}

// executePipedChain() function is used to execute a command
// with one or more | operators in it. Internally, it will
// run each command as child process and pass it's output
// to the next command through a pipe.
void executePipedChain(char **args)
{
	pid_t pid;
	int fdin, fdout;
	int p[2];
	int status;
	int iNumCommands;	// this will contain the total number of commands
	int i;

	// Note that a command like "ls -l | wc -w | wc" will be present
	// in input parameter "args" as { "ls, "-l", "|", "wc", "-w", "|", "wc", NULL }.
	// However, this can't be processed as a single command,
	// we need to split "args" into multiple arrays of strings
	//
	// E.g.  arguments[0] = { "ls", "-l", NULL }
	//       arguments[1] = { "wc", "-w", NULL }
	//       arguments[2] = { "wc", NULL }
	//
	// We will call splitChainedCommands() function to do this
	// splitting of commands.
	char ***arguments = splitChainedCommands(args, &iNumCommands);
	
	// Run all the commands in the piped chain
	// sequentially in this for loop.
	for (i = 0; i < iNumCommands; ++i)
	{
		pipe(p);

		// For first process in the chain, read
		// from standard input (keyboard) only.
		if (i == 0)
			fdin = STDIN_FILENO;

		// For the last command/process in the chain,
		// write to standard output (terminal). For all
		// the preceding commands/processes in the chain,
		// write to write end of pipe.
		if (i == (iNumCommands-1))
			fdout = STDOUT_FILENO;
		else
			fdout = p[1];

		// Now run this command/process as a child process. This
		// will be done by executeSingleCommand() function.
		status = executeSingleCommand(arguments[i], fdin, fdout, &pid);

		// Parent process doesn't need write end of pipe,
		// it will be used by child to write.
		close(p[1]);

		// The next command will read from the read end
		// of the pipe, so let's save it.
		fdin = p[0];
	}

	// Print the PID and the status of the command
	// (child process) just completed.
	printExecutionStatus(pid, status);
}

// executeDoublePipeHelper() function takes three command arguments as input.
// It will execute the first command and it's output is used as input to run
// the second and third commands. The outputs of second and third commands are
// displayed on the standard output.
void executeDoublePipeHelper(char **firstCommand, char **secondCommand, char **thirdCommand)
{
	pid_t pid;
	int status;
	int p1[2];
	pipe(p1);

	// Run the first command as a child process.
	pid = fork();

	if (pid == 0)
	{
		close(p1[0]);  // we don't need read end of p1 pipe in first command
		dup2(p1[1], STDOUT_FILENO);  // close standard output and write to write end of p1 pipe instead
		execvp(firstCommand[0], firstCommand);		// run the first command, it will write to write end of p1 pipe now
	}

	// Use the read end of pipe p1 in the parent process
	// here and pass the data read from pipe into a new
	// file.
	close(p1[1]);	// we don't need write end of p1 pipe in parent process, we only want to read from p1 pipe in parent process
	wait(&status);
	
	// Read from read end of p1 pipe and
	// store it in "buff" variable
	char buff[4096];
	int iNumRead = read(p1[0], buff, sizeof(buff));

	// Now "buff" variable has the output of the
	// first command before || operator.

	// Write the contents of "buff" variable into
	// the write end of a new pipe p2. The read
	// end of pipe p2 will be used later as input
	// for the second command.
	int p2[2];
	pipe(p2);
	write(p2[1], buff, iNumRead);

	// Run the second command as a child process.	
	pid = fork();

	if (pid == 0)
	{
		// Use read end of p2 pipe as input to run the second command
		close(p2[1]);				// we don't need the write end of p2 pipe
		dup2(p2[0], STDIN_FILENO);	// don't read from standard input, read from read end of p2 pipe instead
		execvp(secondCommand[0], secondCommand);
	}
	else
	{
		close(p2[0]);
		close(p2[1]);
		wait(&status);
	}

	// Write the contents of "buff" variable into
	// the write end of a new pipe p3. The read
	// end of pipe p3 will be used later as input
	// for the third command.
	int p3[2];
	pipe(p3);
	write(p3[1], buff, iNumRead);

	// Run the third command as a child process.	
	pid = fork();

	if (pid == 0)
	{
		// Use read end of p3 pipe as input to run the third command
		close(p3[1]);				// we don't need the write end of p3 pipe
		dup2(p3[0], STDIN_FILENO);	// don't read from standard input, read from read end of p3 pipe instead
		execvp(thirdCommand[0], thirdCommand);
	}
	else
	{
		close(p3[0]);
		close(p3[1]);
		wait(&status);
	}

	printExecutionStatus(pid, status);
}

// executeDoublePipe() function is used to execute a command
// with || double-pipe operator in it. This command looks like:
// 			ls -l || wc -w, wc -l
// i.e. the output of first command before || operator should
// be passed as input to the next two comma-separated commands.
// In the above example, output of ls -l should be passed as
// input to both wc -w and wc -l commands.
void executeDoublePipe(char **args)
{
	const int BUFF_SIZE = 32;
	int iCurrentCommand = 1;
	int i = 0, j = 0, k = 0, iPos = 0;
	int iNullFound = 0;

	// The given "args" parameter actually contains arguments
	// for 3 commands. The first command is the one before the
	// || operator whose output is to be passed to the second
	// and third commands which are the two comma separated
	// commands. The below while loop will split "args" into
	// 3 separate commands.
	char **firstCommand   = malloc(BUFF_SIZE * sizeof(char *));
	char **secondCommand  = malloc(BUFF_SIZE * sizeof(char *));
	char **thirdCommand   = malloc(BUFF_SIZE * sizeof(char *));

	while (!iNullFound)
	{
		switch (iCurrentCommand)
		{
		case 1:
			if (strcmp(args[iPos], "||") == 0)
				iCurrentCommand++;	// now first command ends, second starts
			else
				firstCommand[i++] = args[iPos];
			break;
		case 2:
			int iLen = strlen(args[iPos]);

			if (strcmp(args[iPos],",") == 0)
			{
				iCurrentCommand++;	// now second command ends, third starts
			}
			else if (args[iPos][iLen-1] == ',')
			{
				args[iPos][iLen-1] = '\0';
				secondCommand[j++] = args[iPos];
				iCurrentCommand++;	// now second command ends, third starts
			}
			else
			{
				secondCommand[j++] = args[iPos];
			}

			break;
		case 3:
			thirdCommand[k++] = args[iPos];
			break;
		default:
			// We shouldn't reach here!
			break;
		}

		// Check if we have just printed the last
		// i.e. the NULL argument. If so, we should
		// exit this loop by setting the flag.
		if (args[iPos] == NULL)
			iNullFound = 1;

		// Go to the next argument.
		iPos++;
	}

	// Now we have split args into 3 separate command arguments,
	// simply pass them to a helper function to run.
	executeDoublePipeHelper(firstCommand, secondCommand, thirdCommand);
}

// executeTriplePipeHelper() function takes four command arguments as input.
// It will execute the first command and it's output is used as input to run
// the second, third and fourth commands. The outputs of second, third and
// fourth commands are displayed on the standard output.
void executeTriplePipeHelper(char **firstCommand, char **secondCommand,
							 char **thirdCommand, char **fourthCommand)
{
	pid_t pid;
	int status;
	int p1[2];
	pipe(p1);

	// Run the first command as a child process.
	pid = fork();

	if (pid == 0)
	{
		close(p1[0]);  // we don't need read end of p1 pipe in first command
		dup2(p1[1], STDOUT_FILENO);  // close standard output and write to write end of p1 pipe instead
		execvp(firstCommand[0], firstCommand);		// run the first command, it will write to write end of p1 pipe now
	}

	// Use the read end of pipe p1 in the parent process
	// here and pass the data read from pipe into a new
	// file.
	close(p1[1]);	// we don't need write end of p1 pipe in parent process, we only want to read from p1 pipe in parent process
	wait(&status);
	
	// Read from read end of p1 pipe and
	// store it in "buff" variable
	char buff[4096];
	int iNumRead = read(p1[0], buff, sizeof(buff));

	// Now "buff" variable has the output of the
	// first command before ||| operator.

	// Write the contents of "buff" variable into
	// the write end of a new pipe p2. The read
	// end of pipe p2 will be used later as input
	// for the second command.
	int p2[2];
	pipe(p2);
	write(p2[1], buff, iNumRead);

	// Run the second command as a child process.
	pid = fork();

	if (pid == 0)
	{
		// Use read end of p2 pipe as input to run the second command
		close(p2[1]);				// we don't need the write end of p2 pipe
		dup2(p2[0], STDIN_FILENO);	// don't read from standard input, read from read end of p2 pipe instead
		execvp(secondCommand[0], secondCommand);
	}
	else
	{
		close(p2[0]);
		close(p2[1]);
		wait(&status);
	}

	// Write the contents of "buff" variable into
	// the write end of a new pipe p3. The read
	// end of pipe p3 will be used later as input
	// for the third command.
	int p3[2];
	pipe(p3);
	write(p3[1], buff, iNumRead);

	// Run the third command as a child process.
	pid = fork();

	if (pid == 0)
	{
		// Use read end of p3 pipe as input to run the third command
		close(p3[1]);				// we don't need the write end of p3 pipe
		dup2(p3[0], STDIN_FILENO);	// don't read from standard input, read from read end of p3 pipe instead
		execvp(thirdCommand[0], thirdCommand);
	}
	else
	{
		close(p3[0]);
		close(p3[1]);
		wait(&status);
	}

	// Write the contents of "buff" variable into
	// the write end of a new pipe p4. The read
	// end of pipe p4 will be used later as input
	// for the fourth command.
	int p4[2];
	pipe(p4);
	write(p4[1], buff, iNumRead);

	// Run the fourth command as a child process.
	pid = fork();

	if (pid == 0)
	{
		// Use read end of p4 pipe as input to run the fourth command
		close(p4[1]);				// we don't need the write end of p4 pipe
		dup2(p4[0], STDIN_FILENO);	// don't read from standard input, read from read end of p4 pipe instead
		execvp(fourthCommand[0], fourthCommand);
	}
	else
	{
		close(p4[0]);
		close(p4[1]);
		wait(&status);
	}

	printExecutionStatus(pid, status);
}

// executeTriplePipe() function is used to execute a command
// with ||| triple-pipe operator in it. This command looks like:
// 			ls -l || wc -w, grep abc, wc -l
// i.e. the output of first command before ||| operator should
// be passed as input to the next three comma-separated commands.
// In the above example, output of ls -l should be passed as
// input to wc -w, grep abc and wc -l commands.
void executeTriplePipe(char **args)
{
	const int BUFF_SIZE = 32;
	int iCurrentCommand = 1;
	int i = 0, j = 0, k = 0, l = 0, iPos = 0;
	int iNullFound = 0;
	int iLen = 0;

	// The given "args" parameter actually contains arguments
	// for 4 commands. The first command is the one before the
	// ||| operator whose output is to be passed to the second,
	// third and fourth commands which are the 3 comma-separated
	// commands. The below while loop will split "args" into
	// 4 separate commands.
	char **firstCommand   = malloc(BUFF_SIZE * sizeof(char *));
	char **secondCommand  = malloc(BUFF_SIZE * sizeof(char *));
	char **thirdCommand   = malloc(BUFF_SIZE * sizeof(char *));
	char **fourthCommand   = malloc(BUFF_SIZE * sizeof(char *));

	while (!iNullFound)
	{
		switch (iCurrentCommand)
		{
		case 1:
			if (strcmp(args[iPos], "|||") == 0)
				iCurrentCommand++;	// now first command ends, second starts
			else
				firstCommand[i++] = args[iPos];
			break;
		case 2:
			iLen = strlen(args[iPos]);

			if (strcmp(args[iPos],",") == 0)
			{
				iCurrentCommand++;	// now second command ends, third starts
			}
			else if (args[iPos][iLen-1] == ',')
			{
				args[iPos][iLen-1] = '\0';
				secondCommand[j++] = args[iPos];
				iCurrentCommand++;	// now second command ends, third starts
			}
			else
			{
				secondCommand[j++] = args[iPos];
			}

			break;
		case 3:
			iLen = strlen(args[iPos]);

			if (strcmp(args[iPos],",") == 0)
			{
				iCurrentCommand++;	// now third command ends, fourth starts
			}
			else if (args[iPos][iLen-1] == ',')
			{
				args[iPos][iLen-1] = '\0';
				thirdCommand[k++] = args[iPos];
				iCurrentCommand++;	// now third command ends, fourth starts
			}
			else
			{
				thirdCommand[k++] = args[iPos];
			}

			break;
		case 4:
			fourthCommand[l++] = args[iPos];
			break;
		default:
			// We shouldn't reach here!
			break;
		}

		// Check if we have just printed the last
		// i.e. the NULL argument. If so, we should
		// exit this loop by setting the flag.
		if (args[iPos] == NULL)
			iNullFound = 1;

		// Go to the next argument.
		iPos++;
	}

	// Now we have split args into 4 separate command arguments,
	// simply pass them to a helper function to run.
	executeTriplePipeHelper(firstCommand, secondCommand, thirdCommand, fourthCommand);
}

// getCommandFromHistory() function will search
// the linked list of command history and return
// the command with a given sequence number.
char* getCommandFromHistory(int iSequenceNo)
{
	char *command = malloc(sizeof(char) * 256);
	CommandHistNode *p = cmdHistList;

	while (p && (p->iSequenceNo != iSequenceNo))
		p = p->next;

	if (p == NULL)
		strcpy(command, "");	// couldn't find that sequence number in list
	else
		strcpy(command, p->command);

	return command;
}

// insertIntoCommandHistory() is used to insert
// a new CommandHistNode into the cmdHistList.
void insertIntoCommandHistory(int iSequenceNo, char *command)
{
	CommandHistNode* node = (CommandHistNode*) malloc(sizeof(CommandHistNode));
	node->iSequenceNo = iSequenceNo;
	strcpy(node->command, command);
	node->next = NULL;

	if (!cmdHistList)
	{
		// List is currently empty, so simply
		// point the list to the new node
		cmdHistList = node;
	}
	else
	{
		CommandHistNode *p = cmdHistList;

		while (p->next)
			p = p->next;

		// Now p points to the last node in the list.
		// Append the new node after it.
		p->next = node;
	}
}

void executeCommand(int);	// forward declaration

// executeCommandHistory() function is used to execute
// "cmdhist" command entered by user. This "cmdhist" command
// tries to mimic the functionality of "history" command
// available in some Unix shells.
void executeCommandHistory(char **args, int iSequenceNo)
{
	CommandHistNode *p = cmdHistList;

	// If there was no numeric argument after
	// "cmdhist" command
	if (args[1] == NULL)
	{
		while (p)
		{
			printf("%d\t%s", p->iSequenceNo, p->command);
			p = p->next;
		}
	}
	else
	{
		// There was numeric argument after
		// "cmdhist" command. User wants last
		// n commands only from the history.
		int n = atoi(args[1]);

		int iStartNo = iSequenceNo - n + 1;

		while (p)
		{
			if (p->iSequenceNo >= iStartNo)
				printf("%d\t%s", p->iSequenceNo, p->command);

			p = p->next;
		}

	}

	printf("Enter command number (or Enter to quit): ");
	char *sSeqNo = getUserInput();
	int iSeqNo = atoi(sSeqNo);

	// Ignore Enter key, otherwise execute the command
	if (iSeqNo > 0)
		executeCommand(iSeqNo);
}

// executeCommand() function will show the shell prompt
// to the user, take one command as input and executes
// the same based on the type of command entered. The
// parameter iUseHistorySeqNo is passed as 0 if the command
// has to be manually entered by user on the shell prompt,
// otherwise it will contain the sequence number if it has
// to be run by using the command history.
void executeCommand(int iUseHistorySeqNo)
{
	int status;
	char *input = NULL;
	char **arguments = NULL;

	// iSequenceNo will track the sequence numbers of
	// the commands being executed and record the same
	// in the command history alongwith the command.
	static int iSequenceNo = 0;

	// Print the current working directory
	// followed by shell prompt
	char directory[256];
	getcwd(directory, sizeof(directory));
	printf("%s$ ", directory);

	if (iUseHistorySeqNo == 0)
	{
		// Get the command entered by user
		// on the shell prompt
		input = getUserInput();
	}
	else if (iUseHistorySeqNo > 0)
	{
		// Get the command from the command
		// history linked list
		input = getCommandFromHistory(iUseHistorySeqNo);
	}

	// If the command entered is an empty string,
	// do nothing and return.
	if ((strcmp(input, "") == 0) || (strcmp(input, "\n") == 0))
		return;

	// Increment command sequence number
	iSequenceNo++;

	// Keep a record of this command in the
	// command history.
	insertIntoCommandHistory(iSequenceNo, input);

	// Split the whole string input into multiple arguments
	CommandInfo ci;
	arguments = getArguments(input, &ci);

	switch (ci.type)
	{
	case Simple:
		// There is no redirection or pipe in the command.
		// Execute it normally.
		executeNormal(arguments);
		break;
	case Output_Redirect:
		// There is > redirection operator in command.
		// Execute it accordingly (iAppend flag should
		// be passed as 0).
		executeOutputRedirect(arguments, ci.filename, 0);
		break;
	case Output_Append:
		// There is >> redirection operator in command.
		// Execute it accordingly (iAppend flag should
		// be passed as 1).
		executeOutputRedirect(arguments, ci.filename, 1);
		break;
	case Input_Redirect:
		// There is < redirection operator in command.
		// Execute it accordingly.
		executeInputRedirect(arguments, ci.filename);
		break;
	case Piped_Chain:
		// There is one or more | operators in command.
		// Execute it accordingly.
		executePipedChain(arguments);
		break;
	case Double_Pipe:
		// There is || double-pipe operator in command.
		// Execute it accordingly.
		executeDoublePipe(arguments);
		break;
	case Triple_Pipe:
		// There is ||| triple-pipe operator in command.
		// Execute it accordingly.
		executeTriplePipe(arguments);
		break;
	case Command_History:
		// It is a "cmdhist" command which tries to mimic the
		// "history" command available in some Unix shells.
		executeCommandHistory(arguments, iSequenceNo);
		break;
	default:
		// Unexpected, do error handling etc.
		break;
	}

	// Deallocate any memory that's allocated
	// for arguments
	free(arguments);
	arguments = NULL;
}

int main()
{
	// Use infinite loop to show the shell prompt
	// and allow user to run commands repeatedly.
	while (1)
		executeCommand(0);

	return 0;
}

