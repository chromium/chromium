// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SQLITE_VFS_PENDING_FILE_SET_H_
#define COMPONENTS_SQLITE_VFS_PENDING_FILE_SET_H_

#include "base/byte_size.h"
#include "base/component_export.h"
#include "base/files/file.h"
#include "base/memory/unsafe_shared_memory_region.h"
#include "build/build_config.h"

namespace sqlite_vfs {

// State required to open a DB via SQLite VFS. The DB can be opened by creating
// a SqliteVfsFileSet with Bind() and registering it with
// SqliteSandboxedVfsDelegate::RegisterSandboxedFiles(). A database opened with
// exclusive locking disabled requires a `PendingFileSet` with the optional
// `shared_lock` member.
// A rollback journal handle (`journal_file`) is only required if the database
// is opened in rollback mode, or is transitioning to/from it.
// A database opened in WAL-mode requires a `PendingFileSet` that includes the
// optional `wal_file` handle and, if exclusive locking is disabled, the
// optional `wal_index_file` handle. A read/write file set for a shareable
// WAL-mode database will also have a `wal_index_file_read_only` handle so that
// a read-only file set may be shared.
struct COMPONENT_EXPORT(PENDING_FILE_SET) PendingFileSet {
  PendingFileSet();
  PendingFileSet(PendingFileSet&&);
  PendingFileSet& operator=(PendingFileSet&&);
  ~PendingFileSet();

  // Returns the total size of the database file and its rollback journal and
  // write-ahead log, if present. The temporary WAL-index file isn't included.
  // Files whose size can't be read are skipped. Performs blocking I/O.
  base::ByteSize ComputeTotalFileSize() const;

  // Returns true if `db_file` was newly created when this instance was made,
  // rather than an existing database file being opened. This is only known in
  // the process that made this instance; it is always false once the instance
  // has been sent to another process.
  bool IsNewDatabase() const;

  base::File db_file;
  base::File journal_file;
  base::File wal_file;
  base::File wal_index_file;
#if !BUILDFLAG(IS_WIN)
  base::File wal_index_file_read_only;
#endif

  // An optional read-write region of memory shared by all processes accessing
  // `db_file_` that holds the locking state for the database. Locks are not
  // released upon abnormal process termination.
  base::UnsafeSharedMemoryRegion shared_lock;

  // False if these handles grant read-only access to their respective objects.
  // In this case, the database should be opened in read-only mode.
  bool read_write = false;

  // True if this file set was created for a WAL-mode database. An instance may
  // have a `wal_file` handle (and an optional `wal_index_file` handle) even
  // when `wal_mode` is false when migrating an existing WAL-mode database back
  // to using a rollback journal. The same is true for the `journal_file` handle
  // when `wal_mode` is true.
  bool wal_mode = false;
};

}  // namespace sqlite_vfs

#endif  // COMPONENTS_SQLITE_VFS_PENDING_FILE_SET_H_
