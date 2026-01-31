# SQLite VFS

This component provides a sandboxed SQLite Virtual File System (VFS). It is
used by `components/persistent_cache` to provide a sandboxed SQLite database
access.

## Usage

A trusted process (that can access the filesystem) can create a `PendingFileSet`
object. This `PendingFileSet` object is then used by clients to connect to a
database in a sandboxed process.

`persistent_cache::PersistentCache` is one such client that utilizes a
`PendingFileSet`. Specifically, `persistent_cache::SqliteBackendImpl` (located
in `components/persistent_cache/sqlite/sqlite_backend_impl.cc`) takes a
`PendingFileSet`, registers the `SqliteVfsFileSet` with
`SqliteSandboxedVfsDelegate::RegisterSandboxedFiles()`, and then opens the
database using `sql::Database` with the virtual file path returned by
`SqliteVfsFileSet::GetDbVirtualFilePath()`, allowing access from a sandboxed
process.
