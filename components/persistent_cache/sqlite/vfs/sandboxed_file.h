// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PERSISTENT_CACHE_SQLITE_VFS_SANDBOXED_FILE_H_
#define COMPONENTS_PERSISTENT_CACHE_SQLITE_VFS_SANDBOXED_FILE_H_

#include <stdint.h>

#include "base/component_export.h"
#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/memory/shared_memory_safety_checker.h"
#include "base/memory/unsafe_shared_memory_region.h"
#include "sql/sandboxed_vfs_file.h"

namespace persistent_cache {

// The lock shared state is encoded over 32-bits:
//   3 3 2 2 2 2 2 2 2 2 2 2 1 1 1 1 1 1 1 1 1 1
//   1 0 9 8 7 6 5 4 3 2 1 0 9 8 7 6 5 4 3 2 1 0 9 8 7 6 5 4 3 2 1 0
//  +---+-+-+-----------------------+-------------------------------+
//  |0|P|R|0|                     SHARED COUNT                      |
//  +---+-+-+-----------------------+-------------------------------+
//
// Where
//
//   SHARED COUNT: The number of SHARED locks held by readers.
//   R: The RESERVED lock is held. New shared locks are still permitted.
//   P: The PENDING lock is held. No new shared locks are permitted while any
//      process holds the PENDING lock.
//
// A process holds the EXCLUSIVE lock when it holds the PENDING lock and the
// SHARED COUNT is zero.
using LockState = base::subtle::SharedAtomic<uint32_t>;

// Represents a file to be exposed to sql::Database via
// SqliteSandboxedVfsDelegate.
//
// This class can be bound to a sqlite3_file to which ownership is relinquished
// to SQLite. It's not copyable or movable to ensure it doesn't become invalid
// outside of SQLite's control.
class COMPONENT_EXPORT(PERSISTENT_CACHE) SandboxedFile
    : public sql::SandboxedVfsFile {
 public:
  enum class AccessRights { kReadWrite, kReadOnly };

  // `file_path` is the optional path to the file. It may be omitted when
  // `access_rights` is `kReadOnly` or if when `access_rights` is `kReadWrite`
  // and `DuplicateFiles()` will never be used to obtain a read-only handle to
  // the file.
  SandboxedFile(base::File file,
                base::FilePath file_path,
                AccessRights access_rights,
                base::WritableSharedMemoryMapping mapped_shared_lock =
                    base::WritableSharedMemoryMapping());
  SandboxedFile(SandboxedFile& other) = delete;
  SandboxedFile& operator=(const SandboxedFile& other) = delete;
  SandboxedFile(SandboxedFile&& other) = delete;
  SandboxedFile& operator=(SandboxedFile&& other) = delete;
  ~SandboxedFile() override;

  // Called by the VFS to take the underlying base::File. Concretely,
  // this dance occurs when a file is opened:
  //
  // SandboxedVfs::Open
  //   -- Acquire the base::File
  //   SqliteSandboxedVfsDelegate::OpenFile
  //     SandboxedFile::TakeUnderlyingFile
  //   -- Pass it back to SandboxedFile
  //   SqliteSandboxedVfsDelegate::RetrieveSandboxedVfsFile
  //     SandboxedFile::OnFileOpened  base::File TakeUnderlyingFile();
  base::File TakeUnderlyingFile();

  // Called by the VFS when the file is successfully opened.
  void OnFileOpened(base::File file);

  // Used for unittests.
  base::File& UnderlyingFileForTesting() { return underlying_file_; }
  base::File& OpenedFileForTesting() { return opened_file_; }

  // Returns true if this is a valid opened file.
  bool IsValid() const { return opened_file_.IsValid(); }

  AccessRights access_rights() const { return access_rights_; }

  // Returns a handle to the file with either read-write or read-only access;
  // or an invalid File in case of error. To emit a read-only handle from an
  // instance with read-write access to the file, the path to the underlying
  // file must have been provided at construction.
  base::File DuplicateFile(AccessRights access_rights);

  // sqlite3_file implementation.
  int Close() override;
  int Read(void* buffer, int size, sqlite3_int64 offset) override;
  int Write(const void* buffer, int size, sqlite3_int64 offset) override;
  int Truncate(sqlite3_int64 size) override;
  int Sync(int flags) override;
  int FileSize(sqlite3_int64* result_size) override;
  int Lock(int mode) override;
  int Unlock(int mode) override;
  int CheckReservedLock(int* has_reserved_lock) override;
  int FileControl(int opcode, void* data) override;
  int SectorSize() override;
  int DeviceCharacteristics() override;
  int ShmMap(int page_index,
             int page_size,
             int extend_file_if_needed,
             void volatile** result) override;
  int ShmLock(int offset, int size, int flags) override;
  void ShmBarrier() override;
  int ShmUnmap(int also_delete_file) override;
  int Fetch(sqlite3_int64 offset, int size, void** result) override;
  int Unfetch(sqlite3_int64 offset, void* fetch_result) override;

  int LockModeForTesting() const { return sqlite_lock_mode_; }

 private:
  // Returns a pointer to the lock state, which is shared across other instances
  // of SandboxedFile via shared memory.
  LockState& GetLockState();

  // The path to the underlying file. Only set for the creator of the file; not
  // for other consumers to which it has been shared.
  const base::FilePath file_path_;
  base::File underlying_file_;
  base::File opened_file_;
  const AccessRights access_rights_;

  // One of the SQLite locking mode constants which represent the current lock
  // state of this connection (see: https://www.sqlite.org/lockingv3.html).
  int sqlite_lock_mode_ = SQLITE_LOCK_NONE;

  // The actual shared locks across processes to implement the SQLite algorithm
  // and from which `sqlite_lock_mode_` is coming from.
  base::WritableSharedMemoryMapping mapped_shared_lock_;
};

}  // namespace persistent_cache

#endif  // COMPONENTS_PERSISTENT_CACHE_SQLITE_VFS_SANDBOXED_FILE_H_
