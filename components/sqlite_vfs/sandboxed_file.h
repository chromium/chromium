// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SQLITE_VFS_SANDBOXED_FILE_H_
#define COMPONENTS_SQLITE_VFS_SANDBOXED_FILE_H_

#include <stdint.h>

#include <memory>
#include <optional>
#include <vector>

#include "base/component_export.h"
#include "base/files/file.h"
#include "base/files/memory_mapped_file.h"
#include "base/unguessable_token.h"
#include "components/sqlite_vfs/file_system_id.h"
#include "components/sqlite_vfs/lock_state.h"
#include "components/sqlite_vfs/shared_locks.h"
#include "sql/sandboxed_vfs_file.h"

namespace sqlite_vfs {

enum class Client;
enum class FileType;

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

  // `shared_locks` must be specified only for the main database file, and only
  // when the database supports multiple connections. `wal_index_file` must be
  // specified only for the main database file, and only when a WAL-mode
  // database supports multiple connections.
  SandboxedFile(Client client,
                FileType file_type,
                base::File file,
                AccessRights access_rights,
                std::optional<SharedLocks> shared_locks = std::nullopt,
                base::UnguessableToken shared_locks_id = {},
                base::File wal_index_file = {});
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

  const std::optional<FileSystemId>& file_system_id() const {
    return file_system_id_;
  }
  base::UnguessableToken shared_locks_id() const { return shared_locks_id_; }

  // Returns a reference to the instance's file regardless of whether it is
  // open (`IsValid()` returns true) or closed (otherwise). The reference is
  // invalidated upon open/close.
  const base::File& GetFile() const;
  base::File& GetFile();

  // Returns a reference to the instance's WAL-index file handle if valid.
  const base::File& GetWalIndexFile() const { return wal_index_file_; }

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

  // Permanently marks this database as no longer suitable for use by any
  // connection. See `SqliteVfsFileSet::Abandon()` for details.
  LockState Abandon();

 private:
  // Returns true if this instance is likely opened for exclusive access.
  // Take care: this is only valid for FileType::kMainDb files.
  bool is_single_connection() const { return !shared_locks_; }

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

  // If valid, the cross-process shared locks by which the SQLite locking
  // protocols are implemented. Otherwise, this file is opened in exclusive mode
  // so no shared locks are required. Only used for the main database file.
  std::optional<SharedLocks> shared_locks_;
  base::UnguessableToken shared_locks_id_;

  // The WAL-index file. Only used for the main database file, and only when
  // opened for sharing (exclusive locking mode off).
  base::File wal_index_file_;

  // ID of the main database file for double-open detection.
  std::optional<FileSystemId> file_system_id_;

  // Mapped pages of the WAL-index file. Only used for the main database file,
  // and only when opened for sharing (exclusive locking mode off).
  std::vector<std::unique_ptr<base::MemoryMappedFile>> shm_mappings_;
};

}  // namespace sqlite_vfs

#endif  // COMPONENTS_SQLITE_VFS_SANDBOXED_FILE_H_
