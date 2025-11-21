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
#include "base/test/gmock_expected_support.h"
#include "base/types/expected_macros.h"
#include "components/persistent_cache/pending_backend.h"
#include "components/persistent_cache/sqlite/backend_storage_delegate.h"
#include "components/persistent_cache/sqlite/sqlite_backend_impl.h"
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
 protected:
  static constexpr int kOpenFlags = SQLITE_OPEN_READWRITE | SQLITE_OPEN_MAIN_DB;

  void SetUp() override { ASSERT_TRUE(temp_dir_.CreateUniqueTempDir()); }

  std::optional<SqliteVfsFileSet> CreateFilesAndBuildVfsFileSet() {
    std::optional<SqliteVfsFileSet> file_set;
    if (auto pending_backend = backend_storage_delegate_.MakePendingBackend(
            temp_dir_.GetPath(), base::FilePath(FILE_PATH_LITERAL("TEST")),
            /*single_connection=*/false);
        !pending_backend.has_value()) {
      ADD_FAILURE() << "Failed creating pending backend";
    } else {
      file_set = SqliteBackendImpl::BindToFileSet(*std::move(pending_backend));
      EXPECT_NE(file_set, std::nullopt) << "Failed creating pending backend";
    }
    return file_set;
  }

 private:
  base::ScopedTempDir temp_dir_;
  sqlite::BackendStorageDelegate backend_storage_delegate_;
};

TEST_F(SqliteSandboxedVfsTest, NoAccessWithoutRegistering) {
  ASSERT_OK_AND_ASSIGN(SqliteVfsFileSet vfs_file_set,
                       CreateFilesAndBuildVfsFileSet());
  EXPECT_FALSE(SqliteSandboxedVfsDelegate::GetInstance()
                   ->OpenFile(kNonExistentVirtualFilePath, kOpenFlags)
                   .IsValid());
}

TEST_F(SqliteSandboxedVfsTest, AccessAfterRegistering) {
  ASSERT_OK_AND_ASSIGN(SqliteVfsFileSet vfs_file_set,
                       CreateFilesAndBuildVfsFileSet());

  SqliteSandboxedVfsDelegate::UnregisterRunner unregister_runner =
      SqliteSandboxedVfsDelegate::GetInstance()->RegisterSandboxedFiles(
          vfs_file_set);

  for (const auto& virtual_file_path_to_file : vfs_file_set.GetFiles()) {
    EXPECT_TRUE(SqliteSandboxedVfsDelegate::GetInstance()
                    ->OpenFile(virtual_file_path_to_file.first, kOpenFlags)
                    .IsValid());
  }
}

TEST_F(SqliteSandboxedVfsTest, NoAccessAfterUnregistering) {
  ASSERT_OK_AND_ASSIGN(SqliteVfsFileSet vfs_file_set,
                       CreateFilesAndBuildVfsFileSet());

  // Register and immediately unregister.
  {
    SqliteSandboxedVfsDelegate::UnregisterRunner unregister_runner =
        SqliteSandboxedVfsDelegate::GetInstance()->RegisterSandboxedFiles(
            vfs_file_set);
  }

  for (const auto& virtual_file_path_to_file : vfs_file_set.GetFiles()) {
    EXPECT_FALSE(SqliteSandboxedVfsDelegate::GetInstance()
                     ->OpenFile(virtual_file_path_to_file.first, kOpenFlags)
                     .IsValid());
  }
}

TEST_F(SqliteSandboxedVfsTest, AccessAfterReRegistering) {
  ASSERT_OK_AND_ASSIGN(SqliteVfsFileSet vfs_file_set,
                       CreateFilesAndBuildVfsFileSet());

  // Register and immediately unregister.
  {
    SqliteSandboxedVfsDelegate::UnregisterRunner unregister_runner =
        SqliteSandboxedVfsDelegate::GetInstance()->RegisterSandboxedFiles(
            vfs_file_set);
  }

  // Register again.
  SqliteSandboxedVfsDelegate::UnregisterRunner unregister_runner =
      SqliteSandboxedVfsDelegate::GetInstance()->RegisterSandboxedFiles(
          vfs_file_set);

  for (const auto& virtual_file_path_to_file : vfs_file_set.GetFiles()) {
    EXPECT_TRUE(SqliteSandboxedVfsDelegate::GetInstance()
                    ->OpenFile(virtual_file_path_to_file.first, kOpenFlags)
                    .IsValid());
  }
}

TEST_F(SqliteSandboxedVfsTest, DeleteFile) {
  ASSERT_OK_AND_ASSIGN(SqliteVfsFileSet vfs_file_set,
                       CreateFilesAndBuildVfsFileSet());

  // Impossible to delete non-registered files.
  for (const auto& virtual_file_path_to_file : vfs_file_set.GetFiles()) {
    EXPECT_EQ(SqliteSandboxedVfsDelegate::GetInstance()->DeleteFile(
                  virtual_file_path_to_file.first, true),
              SQLITE_NOTFOUND);
  }

  SqliteSandboxedVfsDelegate::UnregisterRunner unregister_runner =
      SqliteSandboxedVfsDelegate::GetInstance()->RegisterSandboxedFiles(
          vfs_file_set);

  // Get a file to test with.
  auto files = vfs_file_set.GetFiles();
  auto it = files.begin();
  const auto& file_path_to_delete = it->first;
  auto& file_to_delete = it->second;

  // Impossible to delete registered and opened files.
  file_to_delete->OnFileOpened(
      file_to_delete->TakeUnderlyingFile(SandboxedFile::FileType::kMainDb));
  EXPECT_TRUE(file_to_delete->IsValid());
  EXPECT_EQ(SqliteSandboxedVfsDelegate::GetInstance()->DeleteFile(
                file_path_to_delete, true),
            SQLITE_IOERR_DELETE);

  // Write to the file, then close it.
  const std::string content = "hello";
  EXPECT_EQ(file_to_delete->Write(content.data(), content.size(), 0),
            SQLITE_OK);
  file_to_delete->Close();
  EXPECT_FALSE(file_to_delete->IsValid());

  // Now it's possible to delete the registered and closed file.
  EXPECT_EQ(SqliteSandboxedVfsDelegate::GetInstance()->DeleteFile(
                file_path_to_delete, true),
            SQLITE_OK);
  EXPECT_EQ(file_to_delete->UnderlyingFileForTesting().GetLength(), 0);
}

TEST_F(SqliteSandboxedVfsTest, OpenFile) {
  ASSERT_OK_AND_ASSIGN(SqliteVfsFileSet vfs_file_set,
                       CreateFilesAndBuildVfsFileSet());

  int64_t length = 0;
  for (auto& virtual_file_path_to_file : vfs_file_set.GetFiles()) {
    virtual_file_path_to_file.second->UnderlyingFileForTesting().SetLength(
        length += 100);
  }

  SqliteSandboxedVfsDelegate::UnregisterRunner unregister_runner =
      SqliteSandboxedVfsDelegate::GetInstance()->RegisterSandboxedFiles(
          vfs_file_set);

  for (const auto& virtual_file_path_to_file : vfs_file_set.GetFiles()) {
    base::File::Info info_from_map;
    virtual_file_path_to_file.second->UnderlyingFileForTesting().GetInfo(
        &info_from_map);

    // Simulate an open from the VFS.
    base::File from_delegate =
        SqliteSandboxedVfsDelegate::GetInstance()->OpenFile(
            virtual_file_path_to_file.first, kOpenFlags);

    base::File::Info info_from_delegate;
    from_delegate.GetInfo(&info_from_delegate);
    EXPECT_EQ(info_from_delegate.size, info_from_map.size);

    // Simulate the binding done by the VFS.
    virtual_file_path_to_file.second->OnFileOpened(std::move(from_delegate));

    base::File::Info info_from_opened_file;
    virtual_file_path_to_file.second->OpenedFileForTesting().GetInfo(
        &info_from_opened_file);
    EXPECT_EQ(info_from_opened_file.size, info_from_map.size);
  }
}

TEST_F(SqliteSandboxedVfsTest, SqliteIntegration) {
  ASSERT_OK_AND_ASSIGN(SqliteVfsFileSet vfs_file_set,
                       CreateFilesAndBuildVfsFileSet());
  SqliteSandboxedVfsDelegate::UnregisterRunner unregister_runner =
      SqliteSandboxedVfsDelegate::GetInstance()->RegisterSandboxedFiles(
          vfs_file_set);

  sql::Database db(sql::DatabaseOptions().set_vfs_name_discouraged(
                       SqliteSandboxedVfsDelegate::kSqliteVfsName),
                   "Test");
  EXPECT_TRUE(db.Open(vfs_file_set.GetDbVirtualFilePath()));
}

}  // namespace persistent_cache
