// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PERSISTENT_CACHE_SQLITE_VFS_SQLITE_DATABASE_VFS_FILE_SET_H_
#define COMPONENTS_PERSISTENT_CACHE_SQLITE_VFS_SQLITE_DATABASE_VFS_FILE_SET_H_

#include <memory>

#include "base/component_export.h"
#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/memory/unsafe_shared_memory_region.h"
#include "components/persistent_cache/lock_state.h"
#include "components/persistent_cache/sqlite/vfs/sandboxed_file.h"

namespace persistent_cache {

// Contains `SanboxedFile` representations of the files necessary to the use of
// an `sql::Database`.
//
// This class owns the `SandboxedFile` files and must outlive any use of them.
class COMPONENT_EXPORT(PERSISTENT_CACHE) SqliteVfsFileSet {
 public:
  SqliteVfsFileSet(std::unique_ptr<SandboxedFile> db_file,
                   std::unique_ptr<SandboxedFile> journal_file,
                   std::unique_ptr<SandboxedFile> wal_journal_file,
                   base::UnsafeSharedMemoryRegion shared_lock);
  SqliteVfsFileSet(SqliteVfsFileSet& other) = delete;
  SqliteVfsFileSet& operator=(const SqliteVfsFileSet& other) = delete;
  SqliteVfsFileSet(SqliteVfsFileSet&& other);
  SqliteVfsFileSet& operator=(SqliteVfsFileSet&& other);
  ~SqliteVfsFileSet();

  // The virtual paths to the files exposed to the database.
  base::FilePath GetDbVirtualFilePath() const;
  base::FilePath GetJournalVirtualFilePath() const;
  base::FilePath GetWalJournalVirtualFilePath() const;

  // Returns the histogram variant for the file at `virtual_file_path`.
  // - "DbFile" if `virtual_file_path` names a main database file.
  // - "JournalFile" if `virtual_file_path` names a main journal file.
  // - "WalJournalFile" if `virtual_file_path` names a write-ahead log file.
  // Crashes the process on unexpected values.
  static std::string_view GetVirtualFileHistogramVariant(
      const base::FilePath& virtual_file_path);

  SandboxedFile* GetSandboxedDbFile() const { return db_file_.get(); }
  SandboxedFile* GetSandboxedJournalFile() const { return journal_file_.get(); }
  SandboxedFile* GetSandboxedWalJournalFile() const {
    CHECK(wal_journal_mode());
    return wal_journal_file_.get();
  }

  bool read_only() const { return read_only_; }

  // The underlying handles.
  const base::File& GetDbFile() const;
  const base::File& GetJournalFile() const;
  const base::File& GetWalJournalFile() const;
  const base::UnsafeSharedMemoryRegion& GetSharedLock() const {
    return shared_lock_;
  }

  bool is_single_connection() const { return !shared_lock_.IsValid(); }

  bool wal_journal_mode() const { return !!wal_journal_file_; }

  // Marks the file as no longer suitable for use. Returns the state of the
  // shared db file lock at the moment of abandonment. Should only be used
  // through `Backend::Abandon()`.
  LockState Abandon();

 private:

  // The shared lock is absent if the file set supports only a single
  // connection.
  base::UnsafeSharedMemoryRegion shared_lock_;
  std::unique_ptr<SandboxedFile> db_file_;
  std::unique_ptr<SandboxedFile> journal_file_;

  // The write-ahead journal file is only present if
  std::unique_ptr<SandboxedFile> wal_journal_file_;

  // SQLite databases use standard naming for their files. Since the vfs might
  // register files for many databases at once it needs some way to
  // differentiate them. This is guaranteed to be unique because it is based on
  // a monotonically increasing integer.
  base::FilePath virtual_fs_path_;

  bool read_only_;
};

}  // namespace persistent_cache

#endif  // COMPONENTS_PERSISTENT_CACHE_SQLITE_VFS_SQLITE_DATABASE_VFS_FILE_SET_H_
