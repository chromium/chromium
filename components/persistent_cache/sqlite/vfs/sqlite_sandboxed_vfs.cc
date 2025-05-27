// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/persistent_cache/sqlite/vfs/sqlite_sandboxed_vfs.h"

#include <mutex>
#include <optional>
#include <utility>

#include "base/files/file.h"
#include "base/files/file_util.h"
#include "base/synchronization/lock.h"
#include "sql/sandboxed_vfs.h"
#include "sql/sandboxed_vfs_file.h"
#include "sql/sandboxed_vfs_file_impl.h"
#include "third_party/sqlite/sqlite3.h"

namespace persistent_cache {

namespace {

std::once_flag g_register_vfs_once_flag;
SqliteSandboxedVfsDelegate* g_instance = nullptr;

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
  // TODO(crbug.com/377475540): Specialize the sql::SandboxedVfsFile for the
  // needs of persistent cache.
  return new sql::SandboxedVfsFileImpl(std::move(file), std::move(file_path),
                                       file_type, vfs);
}

base::File SqliteSandboxedVfsDelegate::OpenFile(
    const base::FilePath& file_path,
    int /*sqlite_requested_flags*/) {
  base::AutoLock lock(files_map_lock_);

  // If `file_name` is missing in the mapping there is no file to return.
  auto it = sandboxed_files_map_.find(file_path);
  if (it == sandboxed_files_map_.end()) {
    return base::File();
  }

  // If `file_name` is found in the mapping return the associated file.
  return it->second.DuplicateUnderlyingFile();
}

int SqliteSandboxedVfsDelegate::DeleteFile(const base::FilePath& file_path,
                                           bool /*sync_dir*/) {
  base::AutoLock lock(files_map_lock_);

  // Sandboxed processes are not capable of deleting files. This completely
  // prevents databases using SqliteSandboxedVfsDelegate from using
  // `sql::Database::Delete()`.
  auto it = sandboxed_files_map_.find(file_path);
  if (it != sandboxed_files_map_.end()) {
    return SQLITE_IOERR_DELETE;
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
      .can_write = it->second.access_rights() ==
                   SandboxedFile::AccessRights::kReadWrite};
}

SqliteSandboxedVfsDelegate::UnregisterRunner::UnregisterRunner(
    SqliteVfsFileSet vfs_file_set)
    : vfs_file_set_(std::move(vfs_file_set)) {}

SqliteSandboxedVfsDelegate::UnregisterRunner::~UnregisterRunner() {
  SqliteSandboxedVfsDelegate::GetInstance()->UnregisterSandboxedFiles(
      vfs_file_set_);
}

// static
void SqliteSandboxedVfsDelegate::UnregisterSandboxedFiles(
    const SqliteVfsFileSet& sqlite_vfs_file_set) {
  base::AutoLock lock(files_map_lock_);

  for (auto& kv : sqlite_vfs_file_set.GetFiles()) {
    size_t num_erased = sandboxed_files_map_.erase(kv.first);
    CHECK_EQ(num_erased, 1ull)
        << "Unregistering the same file set more than once should never happen";
  }
}

// static
SqliteSandboxedVfsDelegate::UnregisterRunner
SqliteSandboxedVfsDelegate::RegisterSandboxedFiles(
    SqliteVfsFileSet sqlite_vfs_file_set) {
  base::AutoLock lock(files_map_lock_);

  for (auto& kv : sqlite_vfs_file_set.GetFiles()) {
    auto [it, inserted] =
        sandboxed_files_map_.emplace(kv.first, std::move(kv.second));
    CHECK(inserted)
        << "Registering the same file set more than once should never happen";
  }

  return UnregisterRunner(std::move(sqlite_vfs_file_set));
}

}  // namespace persistent_cache
