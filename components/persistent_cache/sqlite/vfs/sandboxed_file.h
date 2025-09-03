// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PERSISTENT_CACHE_SQLITE_VFS_SANDBOXED_FILE_H_
#define COMPONENTS_PERSISTENT_CACHE_SQLITE_VFS_SANDBOXED_FILE_H_

#include "base/component_export.h"
#include "base/files/file.h"
#include "sql/sandboxed_vfs_file.h"

namespace persistent_cache {

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

  SandboxedFile(base::File file, AccessRights access_rights);
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

 private:
  base::File underlying_file_;
  base::File opened_file_;
  AccessRights access_rights_;

  // One of the SQLite locking mode constants.
  int sqlite_lock_mode_;
};

}  // namespace persistent_cache

#endif  // COMPONENTS_PERSISTENT_CACHE_SQLITE_VFS_SANDBOXED_FILE_H_
