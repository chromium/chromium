// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PERSISTENT_CACHE_SQLITE_VFS_SQLITE_SANDBOXED_VFS_H_
#define COMPONENTS_PERSISTENT_CACHE_SQLITE_VFS_SQLITE_SANDBOXED_VFS_H_

#include <memory>
#include <optional>
#include <string>

#include "base/component_export.h"
#include "base/containers/flat_map.h"
#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/memory/ref_counted.h"
#include "base/synchronization/lock.h"
#include "components/persistent_cache/sqlite/vfs/sandboxed_file.h"
#include "components/persistent_cache/sqlite/vfs/sqlite_database_vfs_file_set.h"
#include "sql/sandboxed_vfs.h"
#include "sql/sandboxed_vfs_file.h"

namespace persistent_cache {

using SandboxedFileMap = base::flat_map<std::string, SandboxedFile>;

// Implements an sql::SandboxedVfs::Delegate which operates on registered
// base::File objects. Use this in processes that cannot directly open files but
// can obtain them through other means, like a Mojo call.
//
//  Usage:
//
//  // Acquire a vfs file set, for example via a Mojo call to a process
//  // which is allowed to open files.
//  SqliteVfsFileSet vfs_file_set = CreateFilesAndBuildVfsFileSet();
//
//  // Register the file set for use by any sql::Database in this process
//  // that uses the `SqliteSandboxedVfsDelegate`.
//  auto unregister_runner = SqliteSandboxedVfsDelelegate::GetInstance()
//    ->RegisterSandboxedFiles(vfs_file_set.Copy());
//
//  // Create an `sql::Database` which uses the
//  // `SqliteSandboxedVfsDelegate`.
//  sql::Database db(sql::DatabaseOptions().
//      set_vfs_name_discouraged(
//          SqliteSandboxedVfsDelegate::kSqliteVfsName), "Test");
//
//  // Open the database using the virtual file path obtained from the
//  // `SqliteVfsFileSet`.
//  db.Open(vfs_file_set.GetDbVirtualFilePath());
class COMPONENT_EXPORT(PERSISTENT_CACHE) SqliteSandboxedVfsDelegate
    : public sql::SandboxedVfs::Delegate {
 public:
  // There is only one vfs registered to handle an arbitrary number of file
  // mappings so only one name is needed.
  static constexpr const char kSqliteVfsName[] = "sqlite_sandboxed_vfs";

  SqliteSandboxedVfsDelegate();
  ~SqliteSandboxedVfsDelegate() override;

  static SqliteSandboxedVfsDelegate* GetInstance();

  sql::SandboxedVfsFile* RetrieveSandboxedVfsFile(
      base::File file,
      base::FilePath file_path,
      sql::SandboxedVfsFileType file_type,
      sql::SandboxedVfs* vfs) override;

  // `sql::SandboxedVfs::Delegate` overrides.
  [[nodiscard]] base::File OpenFile(const base::FilePath& file_path,
                                    int sqlite_requested_flags) override;
  [[nodiscard]] int DeleteFile(const base::FilePath& file_path,
                               bool sync_dir) override;
  [[nodiscard]] std::optional<sql::SandboxedVfs::PathAccessInfo> GetPathAccess(
      const base::FilePath& file_path) override;

  // Object whose lifetime is tied to the registration of a VFS file set.
  class COMPONENT_EXPORT(PERSISTENT_CACHE) UnregisterRunner {
   public:
    explicit UnregisterRunner(SqliteVfsFileSet vfs_file_set);
    UnregisterRunner(UnregisterRunner& other) = delete;
    UnregisterRunner& operator=(const UnregisterRunner& other) = delete;

    ~UnregisterRunner();

   private:
    SqliteVfsFileSet vfs_file_set_;
  };

  // Make files provided in `sqlite_vfs_file_set` available in the vfs. Returns
  // a runner object that will take care of unregistering the files on
  // destruction.
  [[nodiscard]] UnregisterRunner RegisterSandboxedFiles(
      SqliteVfsFileSet sqlite_vfs_file_set);

 private:
  // Make files provided in `sqlite_vfs_file_set` unavailable in the vfs.
  // Calling this more than once or for a file set that was never registered
  // will have no effect.
  void UnregisterSandboxedFiles(const SqliteVfsFileSet& sqlite_vfs_file_set);

  // Provides exclusive access to the underlying data structure.
  base::Lock files_map_lock_;
  base::flat_map<base::FilePath, SandboxedFile> sandboxed_files_map_
      GUARDED_BY(files_map_lock_);
};

}  // namespace persistent_cache

#endif  // COMPONENTS_PERSISTENT_CACHE_SQLITE_VFS_SQLITE_SANDBOXED_VFS_H_
