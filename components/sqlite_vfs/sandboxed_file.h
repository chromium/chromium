// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SQLITE_VFS_SANDBOXED_FILE_H_
#define COMPONENTS_SQLITE_VFS_SANDBOXED_FILE_H_

#include <stdint.h>

#include "base/component_export.h"
#include "base/files/file.h"
#include "base/memory/shared_memory_safety_checker.h"
#include "base/memory/unsafe_shared_memory_region.h"
#include "components/sqlite_vfs/lock_state.h"
#include "sql/sandboxed_vfs_file.h"

namespace sqlite_vfs {

enum class Client;
enum class FileType;

// The lock shared state is encoded over 32-bits:
//   3 3 2 2 2 2 2 2 2 2 2 2 1 1 1 1 1 1 1 1 1 1
//   1 0 9 8 7 6 5 4 3 2 1 0 9 8 7 6 5 4 3 2 1 0 9 8 7 6 5 4 3 2 1 0
//  +---+-+-+-----------------------+-------------------------------+
//  |A|P|R|0|                     SHARED COUNT                      |
//  +---+-+-+-----------------------+-------------------------------+
//
// Where
//
//   SHARED COUNT: The number of SHARED locks held by readers.
//   A: Whether the lock is abandoned. If set no further use is permitted.
//   R: The RESERVED lock is held. New shared locks are still permitted.
//   P: The PENDING lock is held. No new shared locks are permitted while any
//      process holds the PENDING lock.
//
// A process holds the EXCLUSIVE lock when it holds the PENDING lock and the
// SHARED COUNT is zero.
using SharedAtomicLock = base::subtle::SharedAtomic<uint32_t>;

// Represents a file to be exposed to sql::Database via
// SqliteSandboxedVfsDelegate.
//
// This class can be bound to a sqlite3_file to which ownership is relinquished
// to SQLite. It's not copyable or movable to ensure it doesn't become invalid
// outside of SQLite's control.
class COMPONENT_EXPORT(SQLITE_VFS) SandboxedFile
    : public sql::SandboxedVfsFile {
 public:
  enum class AccessRights { kReadWrite, kReadOnly };

  SandboxedFile(Client client,
                FileType file_type,
                base::File file,
                AccessRights access_rights,
                base::WritableSharedMemoryMapping mapped_shared_lock =
                    base::WritableSharedMemoryMapping());
  SandboxedFile(SandboxedFile& other) = delete;
  SandboxedFile& operator=(const SandboxedFile& other) = delete;
  SandboxedFile(SandboxedFile&& other) = delete;
  SandboxedFile& operator=(SandboxedFile&& other) = delete;
  ~SandboxedFile() override;

  // Called by the VFS to take the underlying base::File. `file_type` indicates
  // the type of file that is being opened. Concretely, this dance occurs when a
  // file is opened:
  //
  // SandboxedVfs::Open
  //   -- Acquire the base::File
  //   SqliteSandboxedVfsDelegate::OpenFile
  //     SandboxedFile::TakeUnderlyingFile
  //   -- Pass it back to SandboxedFile
  //   SqliteSandboxedVfsDelegate::RetrieveSandboxedVfsFile
  //     SandboxedFile::OnFileOpened  base::File TakeUnderlyingFile();
  base::File TakeUnderlyingFile(FileType file_type);

  // Called by the VFS when the file is successfully opened.
  void OnFileOpened(base::File file);

  // Used for unittests.
  base::File& UnderlyingFileForTesting() { return underlying_file_; }
  base::File& OpenedFileForTesting() { return opened_file_; }

  // Returns true if this is a valid opened file.
  bool IsValid() const { return opened_file_.IsValid(); }

  AccessRights access_rights() const { return access_rights_; }
  Client client() const { return client_; }
  FileType file_type() const { return file_type_; }

  // Returns a reference to the instance's file regardless of whether it is
  // open (`IsValid()` returns true) or closed (otherwise). The reference is
  // invalidated upon open/close.
  const base::File& GetFile() const;
  base::File& GetFile();

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

  // Marks this instance as not suitable for use anymore. Once called the effect
  // is permanent. After this call `Lock()` will not succeed anymore and
  // communicate the abandonment through the error code returned which
  // lets code using the class observe the change. Returns the type of lock
  // holder left over after abandonment.
  LockState Abandon();

 private:
  // Returns true if this instance is likely opened for exclusive access.
  // Take care: this is only valid for FileType::kMainDb files.
  bool is_single_connection() const { return !mapped_shared_lock_.IsValid(); }

  // Returns a pointer to the lock state, which is shared across other instances
  // of SandboxedFile via shared memory.
  SharedAtomicLock& GetSharedAtomicLock();

  // Acquire/release a lock on the underlying file. This is used when the
  // creator wishes this to be the only connection allowed to the database.
  bool AcquireSingleConnectionlock();
  void ReleaseSingleConnectionlock();

  const Client client_;

  // The type of database file this represents.
  const FileType file_type_;

  base::File underlying_file_;
  base::File opened_file_;
  const AccessRights access_rights_;

  // One of the SQLite locking mode constants which represent the current lock
  // state of this connection (see: https://www.sqlite.org/lockingv3.html).
  int sqlite_lock_mode_ = SQLITE_LOCK_NONE;

  // If valid, the cross-process shared lock by which the SQLite locking
  // algorithm is implemented. Otherwise, this file is opened in exclusive mode
  // so no shared lock is required.
  base::WritableSharedMemoryMapping mapped_shared_lock_;
};

}  // namespace sqlite_vfs

#endif  // COMPONENTS_SQLITE_VFS_SANDBOXED_FILE_H_
