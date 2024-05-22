This component provides a named system lock that allows for synchronization
across multiple processes without relying on lockfiles. Linux, MacOS, and
Windows are supported.

## Example usage

## Implementation

The lock is implemented per platform:

* Linux - As a pthread mutex.
* MacOS - Using `bootstrap_check_in()`, interpreting ownership of receive rights
on a Mach service name as ownership of a lock.
* Windows - As a kernel mutex.

### Linux-specific notes

The lock is implemented using a pthread mutex in shared memory. Contenders
attempt to open a POSIX shared memory object, creating the object if it does not
exist. The mutex is configured with the `PTHREAD_MUTEX_ROBUST` attribute to
ensure that it remains recoverable if the process holding the lock exits
abnormally.

Due to the nature of the `shm_unlink` system call, it is impossible for any
contending process to determine if it is safe to destroy the shared memory
object. Consider the following sequence of processes A, B, and C:

1. A: Shared memory foo does not exist. Create the shared memory object.
1. A: Creates and acquires the mutex lock in shared memory.
1. B: Shared memory foo exists. Open the existing shared memory object.
1. A: Release the mutex lock and shm_unlink “foo”. Note: Process B can still use
the shared memory until it closes it. Future attempts to open foo will fail with
ENOENT. foo can be recreated.
1. C: Shared memory foo does not exist. Create the shared memory object.
1. B: Acquire the mutex lock in shared memory.
1. C: Creates and acquires the mutex lock in shared memory.

In the sequence above, unlinking the shared memory created a situation in which
processes B and C hold the lock simultaneously. Thus, by design, the lock uses a
leaky mutex in shared memory. The leak occurs once per named lock and is around
40 bytes.