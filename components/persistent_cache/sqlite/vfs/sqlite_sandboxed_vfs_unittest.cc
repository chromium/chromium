// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/persistent_cache/sqlite/vfs/sqlite_sandboxed_vfs.h"

#include <memory>
#include <utility>

#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "components/persistent_cache/sqlite/vfs/sqlite_database_vfs_file_set.h"
#include "sql/database.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/sqlite/sqlite3.h"

namespace persistent_cache {

namespace {

const base::FilePath kNonExistentVirtualFilePath =
    base::FilePath::FromASCII("NotFound");

}  // namespace

class SqliteSandboxedVfsTest : public testing::Test {
 public:
  base::FilePath GetTemporaryDir() {
    scoped_temp_dirs_.emplace_back();
    CHECK(scoped_temp_dirs_.back().CreateUniqueTempDir());
    return scoped_temp_dirs_.back().GetPath();
  }

  SqliteVfsFileSet GetTemporaryVfsFileSet() {
    base::FilePath temporary_subdir = GetTemporaryDir();

    uint32_t flags = base::File::FLAG_OPEN_ALWAYS | base::File::FLAG_READ |
                     base::File::FLAG_WRITE;

    // Note: Specifically give nonsensical names to the files here to examplify
    // that using a vfs allows for their use not through their actual names.
    base::File db_file(temporary_subdir.AppendASCII("FIRST"), flags);
    base::File journal_file(temporary_subdir.AppendASCII("SECOND"), flags);

    return SqliteVfsFileSet(
        SandboxedFile(std::move(db_file),
                      SandboxedFile::AccessRights::kReadWrite),
        SandboxedFile(std::move(journal_file),
                      SandboxedFile::AccessRights::kReadWrite));
  }

 private:
  std::vector<base::ScopedTempDir> scoped_temp_dirs_;
};

TEST_F(SqliteSandboxedVfsTest, NoAccessWithoutRegistering) {
  SqliteVfsFileSet vfs_file_set = GetTemporaryVfsFileSet();
  EXPECT_FALSE(SqliteSandboxedVfsDelegate::GetInstance()
                   ->OpenFile(kNonExistentVirtualFilePath, 0)
                   .IsValid());
}

TEST_F(SqliteSandboxedVfsTest, AccessAfterRegistering) {
  SqliteVfsFileSet vfs_file_set = GetTemporaryVfsFileSet();

  SqliteSandboxedVfsDelegate::UnregisterRunner unregister_runner =
      SqliteSandboxedVfsDelegate::GetInstance()->RegisterSandboxedFiles(
          vfs_file_set.Copy());

  for (const auto& virtual_file_path_to_file : vfs_file_set.GetFiles()) {
    EXPECT_TRUE(SqliteSandboxedVfsDelegate::GetInstance()
                    ->OpenFile(virtual_file_path_to_file.first, 0)
                    .IsValid());
  }
}

TEST_F(SqliteSandboxedVfsTest, NoAccessAfterUnregistering) {
  SqliteVfsFileSet vfs_file_set = GetTemporaryVfsFileSet();

  // Register and immediately unregister.
  {
    SqliteSandboxedVfsDelegate::UnregisterRunner unregister_runner =
        SqliteSandboxedVfsDelegate::GetInstance()->RegisterSandboxedFiles(
            vfs_file_set.Copy());
  }

  for (const auto& virtual_file_path_to_file : vfs_file_set.GetFiles()) {
    EXPECT_FALSE(SqliteSandboxedVfsDelegate::GetInstance()
                     ->OpenFile(virtual_file_path_to_file.first, 0)
                     .IsValid());
  }
}

TEST_F(SqliteSandboxedVfsTest, AccessAfterReRegistering) {
  SqliteVfsFileSet vfs_file_set = GetTemporaryVfsFileSet();

  // Register and immediately unregister.
  {
    SqliteSandboxedVfsDelegate::UnregisterRunner unregister_runner =
        SqliteSandboxedVfsDelegate::GetInstance()->RegisterSandboxedFiles(
            vfs_file_set.Copy());
  }

  // Register again.
  SqliteSandboxedVfsDelegate::UnregisterRunner unregister_runner =
      SqliteSandboxedVfsDelegate::GetInstance()->RegisterSandboxedFiles(
          vfs_file_set.Copy());

  for (const auto& virtual_file_path_to_file : vfs_file_set.GetFiles()) {
    EXPECT_TRUE(SqliteSandboxedVfsDelegate::GetInstance()
                    ->OpenFile(virtual_file_path_to_file.first, 0)
                    .IsValid());
  }
}

TEST_F(SqliteSandboxedVfsTest, DeleteFileAlwaysImpossible) {
  SqliteVfsFileSet vfs_file_set = GetTemporaryVfsFileSet();

  // Impossible to delete non-registered files.
  for (const auto& virtual_file_path_to_file : vfs_file_set.GetFiles()) {
    EXPECT_EQ(SqliteSandboxedVfsDelegate::GetInstance()->DeleteFile(
                  virtual_file_path_to_file.first, true),
              SQLITE_NOTFOUND);
  }

  SqliteSandboxedVfsDelegate::UnregisterRunner unregister_runner =
      SqliteSandboxedVfsDelegate::GetInstance()->RegisterSandboxedFiles(
          vfs_file_set.Copy());

  // Impossible to delete registered files.
  for (const auto& virtual_file_path_to_file : vfs_file_set.GetFiles()) {
    EXPECT_EQ(SqliteSandboxedVfsDelegate::GetInstance()->DeleteFile(
                  virtual_file_path_to_file.first, true),
              SQLITE_IOERR_DELETE);
  }
}

TEST_F(SqliteSandboxedVfsTest, OpenFile) {
  SqliteVfsFileSet vfs_file_set = GetTemporaryVfsFileSet();

  int64_t length = 0;
  for (const auto& virtual_file_path_to_file : vfs_file_set.GetFiles()) {
    virtual_file_path_to_file.second.DuplicateUnderlyingFile().SetLength(
        length += 100);
  }

  SqliteSandboxedVfsDelegate::UnregisterRunner unregister_runner =
      SqliteSandboxedVfsDelegate::GetInstance()->RegisterSandboxedFiles(
          vfs_file_set.Copy());

  for (const auto& virtual_file_path_to_file : vfs_file_set.GetFiles()) {
    const base::File from_delegate =
        SqliteSandboxedVfsDelegate::GetInstance()->OpenFile(
            virtual_file_path_to_file.first, 0);
    base::File from_map =
        virtual_file_path_to_file.second.DuplicateUnderlyingFile();

    base::File::Info info_from_delegate;
    base::File::Info info_from_map;

    from_delegate.GetInfo(&info_from_delegate);
    from_map.GetInfo(&info_from_map);

    EXPECT_EQ(info_from_delegate.size, info_from_map.size);
  }
}

TEST_F(SqliteSandboxedVfsTest, SqliteIntegration) {
  SqliteVfsFileSet vfs_file_set = GetTemporaryVfsFileSet();
  SqliteSandboxedVfsDelegate::UnregisterRunner unregister_runner =
      SqliteSandboxedVfsDelegate::GetInstance()->RegisterSandboxedFiles(
          vfs_file_set.Copy());

  sql::Database db(sql::DatabaseOptions().set_vfs_name_discouraged(
                       SqliteSandboxedVfsDelegate::kSqliteVfsName),
                   "Test");
  EXPECT_TRUE(db.Open(vfs_file_set.GetDbVirtualFilePath()));
}

}  // namespace persistent_cache
