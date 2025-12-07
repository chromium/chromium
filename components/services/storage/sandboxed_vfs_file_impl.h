// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SERVICES_STORAGE_SANDBOXED_VFS_FILE_IMPL_H_
#define COMPONENTS_SERVICES_STORAGE_SANDBOXED_VFS_FILE_IMPL_H_

#include "base/component_export.h"
#include "base/files/file.h"
#include "base/files/file_path.h"
#include "sql/sandboxed_vfs.h"
#include "sql/sandboxed_vfs_file.h"

namespace storage {

// SQLite VFS file implementation that works in a sandboxed process.
//
// An instance is created when SQLite calls into SandboxedVfs::Open(). The
// instance is deleted by a call to SandboxedVfsFileImpl::Close().
//
// The SQLite VFS API includes a complex locking strategy documented in
// https://www.sqlite.org/lockingv3.html
//
// This implementation uses a simplified locking strategy, where we grab an
// exclusive lock when entering any of the modes that prepare for a transition
// to EXCLUSIVE. (These modes are RESERVED and PENDING). This approach is easy
// to implement on top of base::File's locking primitives, at the cost of some
// false contention, which makes us slower under high concurrency.
//
// SQLite's built-in VFSes use the OS support for locking a range of bytes in
// the file, rather locking than the whole file.
class SandboxedVfsFileImpl : public sql::SandboxedVfsFile {
 public:
  SandboxedVfsFileImpl(base::File file,
                       base::FilePath file_path,
                       sql::SandboxedVfsFileType file_type,
                       sql::SandboxedVfs* vfs);
  ~SandboxedVfsFileImpl() override;

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
  // Constructed from a file handle passed from the browser process.
  base::File file_;
  // One of the SQLite locking mode constants.
  int sqlite_lock_mode_;
  // The SandboxedVfs that created this instance.
  const raw_ptr<sql::SandboxedVfs> vfs_;
  // Tracked to check assumptions about SQLite's locking protocol.
  const sql::SandboxedVfsFileType file_type_;
  // Used to identify the file in IPCs to the browser process.
  const base::FilePath file_path_;
};

}  // namespace storage

#endif  // COMPONENTS_SERVICES_STORAGE_SANDBOXED_VFS_FILE_IMPL_H_
