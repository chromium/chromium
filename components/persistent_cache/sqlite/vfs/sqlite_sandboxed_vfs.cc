// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/persistent_cache/sqlite/vfs/sqlite_sandboxed_vfs.h"

#include <mutex>
#include <optional>
#include <tuple>
#include <utility>

#include "base/check.h"
#include "base/check_op.h"
#include "base/files/file.h"
#include "base/files/file_util.h"
#include "base/notreached.h"
#include "base/synchronization/lock.h"
#include "sql/sandboxed_vfs.h"
#include "sql/sandboxed_vfs_file.h"
#include "third_party/sqlite/sqlite3.h"

namespace persistent_cache {

namespace {

std::once_flag g_register_vfs_once_flag;
SqliteSandboxedVfsDelegate* g_instance = nullptr;

SandboxedFile::FileType GetFileType(int sqlite_requested_type) {
  static constexpr int kTypeMask =
      SQLITE_OPEN_MAIN_DB | SQLITE_OPEN_TEMP_DB | SQLITE_OPEN_TRANSIENT_DB |
      SQLITE_OPEN_MAIN_JOURNAL | SQLITE_OPEN_TEMP_JOURNAL |
      SQLITE_OPEN_SUBJOURNAL | SQLITE_OPEN_SUPER_JOURNAL | SQLITE_OPEN_WAL;

  switch (sqlite_requested_type & kTypeMask) {
    case SQLITE_OPEN_MAIN_DB:
      return SandboxedFile::FileType::kMainDb;
    case SQLITE_OPEN_TEMP_DB:
      return SandboxedFile::FileType::kTempDb;
    case SQLITE_OPEN_TRANSIENT_DB:
      return SandboxedFile::FileType::kTransientDb;
    case SQLITE_OPEN_MAIN_JOURNAL:
      return SandboxedFile::FileType::kMainJournal;
    case SQLITE_OPEN_TEMP_JOURNAL:
      return SandboxedFile::FileType::kTempJournal;
    case SQLITE_OPEN_SUBJOURNAL:
      return SandboxedFile::FileType::kSubjournal;
    case SQLITE_OPEN_SUPER_JOURNAL:
      return SandboxedFile::FileType::kSuperJournal;
    case SQLITE_OPEN_WAL:
      return SandboxedFile::FileType::kWal;
  }
  NOTREACHED();
}

}  // namespace

SqliteSandboxedVfsDelegate::SqliteSandboxedVfsDelegate() {
  CHECK(!g_instance);
  g_instance = this;
}

SqliteSandboxedVfsDelegate::~SqliteSandboxedVfsDelegate() {
  CHECK_EQ(this, g_instance);
  g_instance = nullptr;
}

// static
SqliteSandboxedVfsDelegate* SqliteSandboxedVfsDelegate::GetInstance() {
  // When requesting the the global instance the first time make sure it exists
  // and register it.
  std::call_once(g_register_vfs_once_flag, []() {
    sql::SandboxedVfs::Register(kSqliteVfsName,
                                std::make_unique<SqliteSandboxedVfsDelegate>(),
                                /*make_default=*/false);
  });
  return g_instance;
}

sql::SandboxedVfsFile* SqliteSandboxedVfsDelegate::RetrieveSandboxedVfsFile(
    base::File file,
    base::FilePath file_path,
    sql::SandboxedVfsFileType file_type,
    sql::SandboxedVfs* vfs) {
  base::AutoLock lock(files_map_lock_);
  auto it = sandboxed_files_map_.find(file_path);
  if (it == sandboxed_files_map_.end()) {
    return nullptr;
  }

  it->second->OnFileOpened(std::move(file));

  return it->second;
}

base::File SqliteSandboxedVfsDelegate::OpenFile(const base::FilePath& file_path,
                                                int sqlite_requested_flags) {
  base::AutoLock lock(files_map_lock_);

  // If `file_name` is missing in the mapping there is no file to return.
  auto it = sandboxed_files_map_.find(file_path);
  if (it == sandboxed_files_map_.end()) {
    return base::File();
  }

  auto file_type = GetFileType(sqlite_requested_flags);

  // Only the main database and its rollback journal and/or write-ahead log are
  // supported.
  CHECK(file_type == SandboxedFile::FileType::kMainDb ||
        file_type == SandboxedFile::FileType::kMainJournal ||
        file_type == SandboxedFile::FileType::kWal);

  // If `file_name` is found in the mapping return the associated file.
  return it->second->TakeUnderlyingFile(file_type);
}

int SqliteSandboxedVfsDelegate::DeleteFile(const base::FilePath& file_path,
                                           bool /*sync_dir*/) {
  base::AutoLock lock(files_map_lock_);

  // Sandboxed processes are not capable of deleting files, so deletion is
  // simulated by truncating the file to zero. This operation is not permitted
  // if the file is currently open.
  auto it = sandboxed_files_map_.find(file_path);
  if (it != sandboxed_files_map_.end()) {
    if (it->second->IsValid()) {
      return SQLITE_IOERR_DELETE;
    }
    if (!it->second->GetFile().SetLength(0)) {
      return SQLITE_IOERR_DELETE;
    }
    return SQLITE_OK;
  }

  return SQLITE_NOTFOUND;
}

std::optional<sql::SandboxedVfs::PathAccessInfo>
SqliteSandboxedVfsDelegate::GetPathAccess(const base::FilePath& file_path) {
  base::AutoLock lock(files_map_lock_);

  // If `file_name` is missing in the mapping there is no access to return.
  auto it = sandboxed_files_map_.find(file_path);
  if (it == sandboxed_files_map_.end()) {
    return std::nullopt;
  }

  // The files will never be received without read access.
  // Write access is conditional on the file being opened for write.
  return sql::SandboxedVfs::PathAccessInfo{
      .can_read = true,
      .can_write = it->second->access_rights() ==
                   SandboxedFile::AccessRights::kReadWrite};
}

SqliteSandboxedVfsDelegate::UnregisterRunner::UnregisterRunner(
    const SqliteVfsFileSet& vfs_file_set)
    : vfs_file_set_(vfs_file_set) {}

SqliteSandboxedVfsDelegate::UnregisterRunner::~UnregisterRunner() {
  SqliteSandboxedVfsDelegate::GetInstance()->UnregisterSandboxedFiles(
      *vfs_file_set_);
}

// static
void SqliteSandboxedVfsDelegate::UnregisterSandboxedFiles(
    const SqliteVfsFileSet& sqlite_vfs_file_set) {
  base::AutoLock lock(files_map_lock_);

  auto num_erased =
      sandboxed_files_map_.erase(sqlite_vfs_file_set.GetDbVirtualFilePath());
  CHECK_EQ(num_erased, 1U);
  num_erased = sandboxed_files_map_.erase(
      sqlite_vfs_file_set.GetJournalVirtualFilePath());
  CHECK_EQ(num_erased, 1U);
  if (sqlite_vfs_file_set.wal_journal_mode()) {
    num_erased = sandboxed_files_map_.erase(
        sqlite_vfs_file_set.GetWalJournalVirtualFilePath());
    CHECK_EQ(num_erased, 1U);
  }
}

// static
SqliteSandboxedVfsDelegate::UnregisterRunner
SqliteSandboxedVfsDelegate::RegisterSandboxedFiles(
    const SqliteVfsFileSet& sqlite_vfs_file_set) {
  base::AutoLock lock(files_map_lock_);

  auto [it, inserted] =
      sandboxed_files_map_.emplace(sqlite_vfs_file_set.GetDbVirtualFilePath(),
                                   sqlite_vfs_file_set.GetSandboxedDbFile());
  CHECK(inserted);
  std::tie(it, inserted) = sandboxed_files_map_.emplace(
      sqlite_vfs_file_set.GetJournalVirtualFilePath(),
      sqlite_vfs_file_set.GetSandboxedJournalFile());
  CHECK(inserted);
  if (sqlite_vfs_file_set.wal_journal_mode()) {
    std::tie(it, inserted) = sandboxed_files_map_.emplace(
        sqlite_vfs_file_set.GetWalJournalVirtualFilePath(),
        sqlite_vfs_file_set.GetSandboxedWalJournalFile());
    CHECK(inserted);
  }

  return UnregisterRunner(sqlite_vfs_file_set);
}

}  // namespace persistent_cache
