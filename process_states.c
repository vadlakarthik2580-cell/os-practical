/*
 * process_states.c
 * ---------------------------------------------------------------
 * Demonstrates:
 *   - fork() creating a parent and a child process
 *   - Displaying PID and PPID of both processes
 *   - Observing process states (Running, Sleeping, Zombie, Terminated)
 *     at different stages of execution by reading /proc/<pid>/stat
 *
 * Build   :  gcc -Wall -o process_states process_states.c
 * Run     :  ./process_states
 * Platform:  Ubuntu / any Linux (relies on /proc filesystem)
 * ---------------------------------------------------------------
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <string.h>

/* ---------------------------------------------------------------
 * get_process_state()
 * Reads /proc/<pid>/stat and returns the single-letter state code
 * that the Linux kernel maintains for that process:
 *   R - Running or runnable
 *   S - Sleeping (waiting for an event)
 *   D - Uninterruptible sleep (usually I/O)
 *   Z - Zombie (terminated, not yet reaped by parent)
 *   T - Stopped (e.g. by a signal)
 * ------------------------------------------------------------- */
char get_process_state(pid_t pid)
{
    char path[64];
    char state = '?';
    FILE *fp;

    snprintf(path, sizeof(path), "/proc/%d/stat", pid);
    fp = fopen(path, "r");
    if (fp == NULL)
        return 'X';   /* process no longer exists / already reaped */

    /* Format of /proc/pid/stat:  pid (comm) state ppid ...
     * The comm field can itself contain spaces/parentheses, so we
     * search for the LAST ')' and read the state right after it.   */
    char buffer[512];
    if (fgets(buffer, sizeof(buffer), fp) != NULL) {
        char *close_paren = strrchr(buffer, ')');
        if (close_paren != NULL)
            sscanf(close_paren + 2, " %c", &state);
    }

    fclose(fp);
    return state;
}

const char *decode_state(char c)
{
    switch (c) {
        case 'R': return "Running / Runnable";
        case 'S': return "Sleeping (interruptible)";
        case 'D': return "Uninterruptible Sleep (I/O)";
        case 'Z': return "Zombie (terminated, awaiting reap)";
        case 'T': return "Stopped";
        case 'X': return "Terminated / No longer exists";
        default:  return "Unknown";
    }
}

int main(void)
{
    pid_t pid;

    printf("===================================================\n");
    printf(" BEFORE fork()\n");
    printf("===================================================\n");
    printf("Process (about to fork) -> PID: %d | PPID: %d | State: %s\n\n",
           getpid(), getppid(), decode_state(get_process_state(getpid())));

    /* IMPORTANT: flush stdout before fork(). Otherwise, any buffered
     * output sitting in the C library's I/O buffer gets duplicated
     * into the child process's copy of memory and printed AGAIN
     * when the child (or parent) later flushes its buffer.        */
    fflush(stdout);

    pid = fork();

    if (pid < 0) {
        /* fork failed */
        perror("fork() failed");
        exit(EXIT_FAILURE);
    }
    else if (pid == 0) {
        /* ---------------- CHILD PROCESS ---------------- */
        printf("===================================================\n");
        printf(" CHILD PROCESS (just created)\n");
        printf("===================================================\n");
        printf("Child  -> PID: %d | PPID: %d | State: %s\n",
               getpid(), getppid(), decode_state(get_process_state(getpid())));

        printf("Child  -> doing some work (simulated with sleep)...\n");
        sleep(3);   /* while sleeping, parent can observe state 'S' */
printf("Child  -> PID: %d finishing execution now.\n", getpid());
        exit(42);   /* child terminates with exit code 42 */
    }
    else {
        /* ---------------- PARENT PROCESS ---------------- */
        printf("===================================================\n");
        printf(" PARENT PROCESS (right after fork)\n");
        printf("===================================================\n");
        printf("Parent -> PID: %d | PPID: %d | Child PID: %d\n",
               getpid(), getppid(), pid);
        printf("Parent -> Child's state immediately after fork: %s\n\n",
               decode_state(get_process_state(pid)));

        /* Give the child a moment to start sleeping, then sample state */
        usleep(500000); /* 0.5 sec */
        printf("Parent -> Child's state while child is sleeping(): %s\n\n",
               decode_state(get_process_state(pid)));

        /* Wait for child to finish WITHOUT reaping immediately,
         * so we can catch it as a zombie for demonstration.        */
        sleep(3); /* child will have called exit() by now */
        printf("Parent -> Child's state right after child exit() "
               "(before wait()): %s\n\n",
               decode_state(get_process_state(pid)));

        int status;
        pid_t reaped = waitpid(pid, &status, 0);  /* reap the zombie */

        printf("===================================================\n");
        printf(" AFTER waitpid() - child has been reaped\n");
        printf("===================================================\n");
        if (reaped == pid) {
            if (WIFEXITED(status))
                printf("Parent -> Reaped child PID %d, exit status = %d\n",
                       reaped, WEXITSTATUS(status));
            else
                printf("Parent -> Child %d terminated abnormally\n", reaped);
        }
        printf("Parent -> Child's state after reaping: %s\n",
               decode_state(get_process_state(pid)));

        printf("\nParent -> PID: %d | PPID: %d | State: %s | Exiting now.\n",
               getpid(), getppid(), decode_state(get_process_state(getpid())));
    }

    return 0;
}

 
