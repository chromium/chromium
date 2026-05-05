// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sqlite_vfs/sqlite_sandboxed_vfs.h"

#include <optional>
#include <tuple>
#include <utility>

#include "base/check.h"
#include "base/check_op.h"
#include "base/files/file.h"
#include "base/files/file_util.h"
#include "base/metrics/histogram_functions.h"
#include "base/not_fatal_until.h"
#include "base/notreached.h"
#include "base/strings/strcat.h"
#include "base/synchronization/lock.h"
#include "components/sqlite_vfs/file_system_id.h"
#include "components/sqlite_vfs/file_type.h"
#include "components/sqlite_vfs/metrics_util.h"
#include "components/sqlite_vfs/sandboxed_file.h"
#include "components/sqlite_vfs/sqlite_database_vfs_file_set.h"
#include "sql/sandboxed_vfs.h"
#include "sql/sandboxed_vfs_file.h"
#include "third_party/sqlite/sqlite3.h"

namespace sqlite_vfs {

namespace {

SqliteSandboxedVfsDelegate* g_instance = nullptr;

FileType GetFileType(int sqlite_requested_type) {
  static constexpr int kTypeMask =
      SQLITE_OPEN_MAIN_DB | SQLITE_OPEN_TEMP_DB | SQLITE_OPEN_TRANSIENT_DB |
      SQLITE_OPEN_MAIN_JOURNAL | SQLITE_OPEN_TEMP_JOURNAL |
      SQLITE_OPEN_SUBJOURNAL | SQLITE_OPEN_SUPER_JOURNAL | SQLITE_OPEN_WAL;

  switch (sqlite_requested_type & kTypeMask) {
    case SQLITE_OPEN_MAIN_DB:
      return FileType::kMainDb;
    case SQLITE_OPEN_TEMP_DB:
      return FileType::kTempDb;
    case SQLITE_OPEN_TRANSIENT_DB:
      return FileType::kTransientDb;
    case SQLITE_OPEN_MAIN_JOURNAL:
      return FileType::kMainJournal;
    case SQLITE_OPEN_TEMP_JOURNAL:
      return FileType::kTempJournal;
    case SQLITE_OPEN_SUBJOURNAL:
      return FileType::kSubjournal;
    case SQLITE_OPEN_SUPER_JOURNAL:
      return FileType::kSuperJournal;
    case SQLITE_OPEN_WAL:
      return FileType::kWal;
  }
  NOTREACHED();
}

// Returns true if `file1` and `file2` have the same cached ID.
bool IsSameFile(const SandboxedFile& file1, const SandboxedFile& file2) {
  const auto& id1 = file1.file_system_id();
  const auto& id2 = file2.file_system_id();
  return id1.has_value() && id2.has_value() && *id1 == *id2;
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
  [[maybe_unused]] static const bool registered = [] {
    sql::SandboxedVfs::Register(kSqliteVfsName,
                                std::make_unique<SqliteSandboxedVfsDelegate>(),
                                /*make_default=*/false);
    return true;
  }();
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
  CHECK(file_type == FileType::kMainDb || file_type == FileType::kMainJournal ||
        file_type == FileType::kWal);

  // If `file_name` is found in the mapping return the associated file.
  return it->second->TakeUnderlyingFile(file_type);
}

int SqliteSandboxedVfsDelegate::DeleteFile(const base::FilePath& file_path,
                                           bool /*sync_dir*/) {
  base::AutoLock lock(files_map_lock_);

  // Sandboxed processes are not capable of deleting files, so deletion is
  // simulated by truncating the file to zero.
  auto it = sandboxed_files_map_.find(file_path);
  if (it != sandboxed_files_map_.end()) {
    auto& file = it->second->GetFile();
    const auto file_error = file.SetLength(0) ? base::File::FILE_OK
                                              : base::File::GetLastFileError();
    base::UmaHistogramExactLinear(
        GetHistogramName(it->second->client(), "SetLengthResult",
                         it->second->file_type()),
        -file_error, -base::File::FILE_ERROR_MAX);
    return file_error == base::File::FILE_OK ? SQLITE_OK : SQLITE_IOERR_DELETE;
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

  // SQLite truncates files to zero length rather than deleting them in various
  // places (e.g., the main journal file when using journal_mode=TRUNCATE). To
  // accommodate this, the built-in VFSes all treat a zero-length file as if it
  // does not exist. The same must be done here. One side-effect of not doing
  // this is that the SQLite pager will treat the presence of an empty
  // write-ahead log file as an indication that the database is in WAL mode.
  if (it->second->GetFile().GetLength() == 0) {
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
  if (sqlite_vfs_file_set.has_wal_file()) {
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

  const SandboxedFile* incoming_db = sqlite_vfs_file_set.GetSandboxedDbFile();
  const base::UnguessableToken incoming_shm_guid =
      sqlite_vfs_file_set.is_single_connection()
          ? base::UnguessableToken()
          : sqlite_vfs_file_set.GetSharedLock().GetGUID();

  for (const auto& [virtual_path, sandboxed_file] : sandboxed_files_map_) {
    if (sandboxed_file->file_type() == FileType::kMainDb &&
        IsSameFile(*sandboxed_file, *incoming_db)) {
      const base::UnguessableToken registered_shm_guid =
          sandboxed_file->shared_locks_id();

      // If either database connection is opened in single-connection mode (no
      // shared locks segment GUID), it cannot co-exist with any other
      // connection targeting the same physical file in this process.
      CHECK(!registered_shm_guid.is_empty() && !incoming_shm_guid.is_empty(),
            base::NotFatalUntil::M151);

      // Two connections to the same database file with different shared locks
      // results in data corruption.
      CHECK_EQ(registered_shm_guid, incoming_shm_guid,
               base::NotFatalUntil::M151);
    }
  }

  auto [it, inserted] =
      sandboxed_files_map_.emplace(sqlite_vfs_file_set.GetDbVirtualFilePath(),
                                   sqlite_vfs_file_set.GetSandboxedDbFile());
  CHECK(inserted);
  std::tie(it, inserted) = sandboxed_files_map_.emplace(
      sqlite_vfs_file_set.GetJournalVirtualFilePath(),
      sqlite_vfs_file_set.GetSandboxedJournalFile());
  CHECK(inserted);
  if (sqlite_vfs_file_set.has_wal_file()) {
    std::tie(it, inserted) = sandboxed_files_map_.emplace(
        sqlite_vfs_file_set.GetWalJournalVirtualFilePath(),
        sqlite_vfs_file_set.GetSandboxedWalJournalFile());
    CHECK(inserted);
  }

  return UnregisterRunner(sqlite_vfs_file_set);
}

}  // namespace sqlite_vfs
