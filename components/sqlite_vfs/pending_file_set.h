// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SQLITE_VFS_PENDING_FILE_SET_H_
#define COMPONENTS_SQLITE_VFS_PENDING_FILE_SET_H_

#include "base/component_export.h"
#include "base/files/file.h"
#include "base/memory/unsafe_shared_memory_region.h"

namespace sqlite_vfs {

// State required to open a DB via SQLite VFS.
// The DB can be opened by creating a SqliteVfsFileSet with Bind() and
// registering it with SqliteSandboxedVfsDelegate::RegisterSandboxedFiles().
struct COMPONENT_EXPORT(PENDING_FILE_SET) PendingFileSet {
  PendingFileSet();
  PendingFileSet(PendingFileSet&&);
  PendingFileSet& operator=(PendingFileSet&&);
  ~PendingFileSet();

  base::File db_file;
  base::File journal_file;

  // An optional write-ahead log file, specified only if this DB uses a
  // write-ahead log rather than a rollback journal.
  base::File wal_file;

  // An optional read-write region of memory shared by all processes accessing
  // `db_file_` that holds the locking state for the database. Locks are not
  // released upon abnormal process termination.
  base::UnsafeSharedMemoryRegion shared_lock;

  // False if this DB is read-only, true if read/write.
  bool read_write = false;
};

}  // namespace sqlite_vfs

#endif  // COMPONENTS_SQLITE_VFS_PENDING_FILE_SET_H_
