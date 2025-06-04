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
class COMPONENT_EXPORT(PERSISTENT_CACHE) SandboxedFile
    : public sql::SandboxedVfsFile {
 public:
  enum class AccessRights { kReadWrite, kReadOnly };

  SandboxedFile(base::File file, AccessRights access_rights);
  SandboxedFile(SandboxedFile& other) = delete;
  SandboxedFile& operator=(const SandboxedFile& other) = delete;
  SandboxedFile(SandboxedFile&& other);
  SandboxedFile& operator=(SandboxedFile&& other);
  ~SandboxedFile() override;

  SandboxedFile Copy() const;

  // Duplicates the underlying file without rendering it invalid. Use to share
  // file access to interfaces requiring a regular `base::File`.
  base::File DuplicateUnderlyingFile() const;

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
  AccessRights access_rights_;

  // One of the SQLite locking mode constants.
  int sqlite_lock_mode_;
};

}  // namespace persistent_cache

#endif  // COMPONENTS_PERSISTENT_CACHE_SQLITE_VFS_SANDBOXED_FILE_H_
