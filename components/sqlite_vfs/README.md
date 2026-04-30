# SQLite Sandboxed VFS

`sqlite_vfs` provides a custom SQLite Virtual File System (VFS) for Chromium
that allows [`sql::Database`](/sql/database.h) to operate within sandboxed
processes where direct filesystem access is restricted.

Instead of using standard file paths to access data, this VFS operates on
pre-opened `base::File` handles that are securely passed to other processes via
mojo.

## Features

`sqlite_vfs` supports:

* Multiple read/write and read-only connections to a database.
* Exclusive read/write access to a database when only one connection is
  needed/desired.
* Use of either a rollback journal or a write-ahead log ("WAL-mode").
* Bidirectional migration of existing databases to/from WAL-mode.

## Usage Guide

### Simplified setup

The fundamental lifecycle for a database is:

1. A client picks a directory in which database files will reside.
2. A client picks a distinct `base_name` for each database that will reside in
   the above directory.
3. The client calls [`MakePendingFileSet()`](vfs_utils.h) to create the files
   and configure them as desired. This initial
   [`PendingFileSet`](pending_file_set.h) provides read/write access to the
   database. It can be used as-is or passed to another process for use.
4. The client calls [`SqliteVfsFileSet::Bind()`](sqlite_database_vfs_file_set.h)
   to consume a `PendingFileSet` and produce a `SqliteVfsFileSet` for use by the
   process.
5. The client calls [`RegisterSandboxedFiles()`](sqlite_sandboxed_vfs.h) to
   register the file set's files for use by SQLite.
6. Finally, the client creates a `sql::Database`, specifying the name of
   `sqlite_vfs`'s VFS, and opens the database with the virtual path of the file
   set's main database file.

### Shared databases

Multiple connections to a database can be made (provided that the original call
to `MakePendingFileSet()` specified `single_connection=false`) by way of
[`ShareConnection()`](vfs_utils.h). A shared connection can be restricted to
read-only access.

### Abandonment

[`SqliteVfsFileSet::Abandon()`](sqlite_database_vfs_file_set.h) can be called
to mark a database as no longer suitable for use by any connection. This causes
most, if not all, operations on the database to fail with `SQLITE_IOERR_LOCK`
(a.k.a. `sql::SqliteResultCode::kIoLock` and `sql::SqliteErrorCode::kIoLock`).
Client code interacting with databases should take this error as a signal to
promptly release all resources and close the database.

If `Abandon()` returns `LockState::kNotHeld`, then it is safe to create a new
file set for the same database. Otherwise, [`DeleteFiles()`](vfs_utils.h) should
be called to delete the database's files, as the files cannot be used until all
clients have closed their connections, and there is no mechanism by which this
can be coordinated.

## Important Considerations

* Creating multiple file sets for the same database concurrently via
  `MakePendingFileSet()` without abandonment as described above will lead to
  database corruption. There is no protection against this -- it is the client's
  responsibility to never do this.

## Implementation Details

### The WAL-index

A database using a write-ahead log that supports multiple connections requires a
WAL-index file that is mapped into each process's address space. The index is
populated on-demand based on the contents of the write-ahead log by the first
read/write connection to a database.

To avoid leaving a stale index on-disk in case of abnormal termination,
`MakePendingFileSet` creates a randomly-named file for the index and ensures
that it is deleted on close. On POSIX systems, the file is unlinked immediately
after creation. On Windows, the file is opened with `FILE_FLAG_DELETE_ON_CLOSE`
so that the OS deletes it when the last handle is closed.

To allow sharing read-only connections on POSIX, where it is not possible to
reduce access to a file descriptor during duplication, `MakePendingFileSet`
opens a second descriptor to the WAL-index file with read-only access. This is
passed along to the `SqliteVfsFileSet` so that it can be used for an eventual
request to share the file set for read-only access.

### Mapping the WAL-index

[`SandboxedFile::ShmMap`](sandboxed_file.cc) maintains an independent mapped
region for each 32KB page of the WAL-index. A read/write connection will grow
the file as-needed to satisfy these mappings. A read-only connection
communicates its nature to SQLite by returning `SQLITE_READONLY_CANTINIT` for
any request that requires growing the file, and `SQLITE_READONLY` on success for
any request that it can satisfy. In the former case, SQLite will automatically
maintain a private WAL-index for the connection until a subsequent request
succeeds after a read/write connection has grown the file. Since a read-only
connection cannot, itself, cause the WAL-index to grow, this fallback only
happens if the read-only connection opens the database before any read/write
connection.
