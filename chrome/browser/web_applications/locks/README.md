# [WebAppProvider](../README.md) > Locks

*   **Team:** {Team name/email}

## Overview

This directory contains the implementation of the locking mechanism for the web
app system. Locks are used to ensure that only one command is operating on a
particular resource at a time. This prevents race conditions and ensures that
operations on shared resources (like the web app system storage or a shared
`WebContents`) are properly sequenced, which is critical due to the asynchronous
nature of many web app operations.

## Usage

Locks are acquired via the `WebAppLockManager`, and are typically managed by
`WebAppCommand`s. A command specifies the lock it needs via its
`LockDescription`. The `WebAppCommandManager` ensures the command doesn't
execute until the requested lock is acquired.

When a lock is granted, the lock object provides access to relevant resources
through accessor interfaces:

-   `WithAppResources`: Provides access to managers like `WebAppRegistrar`,
    `WebAppSyncBridge`, and `WebAppInstallFinalizer` for operations on specific
    apps.
-   `WithSharedWebContentsResources`: Provides access to the shared
    `WebContents` instance used for background operations.

### Deadlock Prevention

A deadlock can occur if the four Coffman conditions are met: mutual exclusion,
hold-and-wait, no preemption, and circular wait. This system is designed to
prevent deadlocks by breaking two of these conditions:

-   **Circular Wait**: This is prevented by enforcing a strict global lock
    acquisition order. All lock requests must follow this hierarchy, making
    circular dependencies impossible. The order is:
    -   `NoopLock`
    -   `SharedWebContentsLock`
    -   `AppLock`
    -   `AllAppsLock`
-   **Hold-and-Wait**: This condition is avoided by the command architecture
    granting only one lock, and having upgrades consume the old lock. Other than
    that, it is currently mostly the responsibility of the reviewer to make sure
    no one is trying to acquire more than one lock at a time, or doing an
    operation that implicitly has a lock and and waits on the second lock (like
    scheduling a command from within another command and waiting on the second
    command to finish before completing the first command).

### Lock Upgrading and Composition

The system supports acquiring additional locks during an operation, which can be
thought of as a lock "upgrade". This is useful when a command doesn't know all
the resources it needs upfront.

This is implemented by composing locks. When a new lock is acquired, the system
implicitly combines it with the existing lock. From the perspective of the
command, it continues to hold a single, more capable, lock object. This design
abstracts away the complexity of managing multiple lock instances, ensuring that
an operation only ever holds one lock object from this system and preventing
misuse.

For example, a `NoopLock` can be upgraded to an `AppLock`, or a
`SharedWebContentsLock` can be composed with an `AppLock` to produce a
`SharedWebContentsWithAppLock`. The available lock transitions are explicitly
defined in the `WebAppLockManager`.

### Invariants / Caveats

-   Accessing a lock before it is granted will CHECK-fail.
-   Accessing a lock after the `WebAppProvider` system has shut down (e.g.
    during profile destruction) will CHECK-fail. Using `WebAppCommand`s is the
    best way to ensure lock usage is shutdown-safe.

## Testing (for users of the system)

Code that uses the lock system is best tested at the `WebAppCommand` level,
allowing the `WebAppLockManager` to handle the lock acquisition as it would in
production.

## Architecture

The `WebAppLockManager` is the central class for managing locks. It is built on
top of a more generic `PartitionedLockManager`, which manages exclusive and
shared locks on a set of partitioned IDs.

The `WebAppLockManager` defines two lock partitions:

-   **`kStatic`**: For singleton resources like the shared web contents or the
    set of all apps.
-   **`kApp`**: For individual web apps, keyed by their `AppId`.

This partitioning and a strict lock ordering prevents deadlocks.

### Lock Types

Different types of locks are available, each corresponding to a different set of
underlying `PartitionedLock`s:

-   **`NoopLock`**: A lock that doesn't lock any specific resource. It's useful
    as a base lock that can be upgraded later in an operation.
-   **`AppLock`**: Acquires a shared lock on all apps and an exclusive lock on
    one or more specific `AppId`s. This allows multiple commands to operate on
    different apps simultaneously.
-   **`AllAppsLock`**: Acquires an exclusive lock on all apps. This is for
    operations that need to inspect or modify the entire set of web apps, such
    as uninstalling all apps. It blocks all `AppLock` operations.
-   **`SharedWebContentsLock`**: Acquires an exclusive lock on the shared
    `WebContents` used for background tasks like fetching manifests or
    installing apps.
-   **`SharedWebContentsWithAppLock`**: A combination of `SharedWebContentsLock`
    and `AppLock`, for operations that need exclusive access to both the shared
    "WebContents" and specific apps.

### Testing (for developers of the system)

The primary tests for this system are in `web_app_lock_manager_unittest.cc`,
which covers the acquisition, release, and blocking behavior of the various lock
types. The underlying `PartitionedLockManager` is tested in
`partitioned_lock_manager_unittest.cc`.

## Relevant Context

These files and searches were identified as critical during investigation for
this document, or working in this system.

*   **Files:**
    *   `//chrome/browser/web_applications/locks/web_app_lock_manager.h`
    *   `//chrome/browser/web_applications/locks/web_app_lock_manager.cc`
    *   `//chrome/browser/web_applications/locks/lock.h`
    *   `//chrome/browser/web_applications/locks/partitioned_lock_manager.h`
*   **Key Classes:**
    *   `WebAppLockManager`
    *   `WebAppCommand`
    *   `Lock`
    *   `PartitionedLockManager`
