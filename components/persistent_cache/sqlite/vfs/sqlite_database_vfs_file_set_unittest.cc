// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/persistent_cache/sqlite/vfs/sqlite_database_vfs_file_set.h"

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/test/gmock_expected_support.h"
#include "components/persistent_cache/pending_backend.h"
#include "components/persistent_cache/sqlite/backend_storage_delegate.h"
#include "components/persistent_cache/sqlite/constants.h"
#include "components/persistent_cache/sqlite/sqlite_backend_impl.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace persistent_cache {

namespace {

class SqliteVfsFileSetTest : public testing::Test {
 protected:
  static constexpr base::FilePath::StringViewType kBaseName =
      FILE_PATH_LITERAL("TEST");

  void SetUp() override { ASSERT_TRUE(temp_dir_.CreateUniqueTempDir()); }

  const base::FilePath& GetTempDir() const { return temp_dir_.GetPath(); }

  std::pair<base::FilePath, base::FilePath> GetFilePaths() {
    return {temp_dir_.GetPath().Append(kBaseName).AddExtension(
                sqlite::kDbFileExtension),
            temp_dir_.GetPath().Append(kBaseName).AddExtension(
                sqlite::kJournalFileExtension)};
  }

  std::optional<SqliteVfsFileSet> CreateFilesAndBuildVfsFileSet() {
    std::optional<SqliteVfsFileSet> file_set;
    if (auto pending_backend = backend_storage_delegate_.MakePendingBackend(
            temp_dir_.GetPath(), base::FilePath(kBaseName));
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

// Tests that creating and destroying a file set doesn't delete the files.
TEST_F(SqliteVfsFileSetTest, FilesAreNotDeleted) {
  auto [one, two] = GetFilePaths();

  {
    ASSERT_OK_AND_ASSIGN(auto file_set, CreateFilesAndBuildVfsFileSet());

    ASSERT_PRED1(base::PathExists, one);
    ASSERT_PRED1(base::PathExists, two);
  }

  ASSERT_PRED1(base::PathExists, one);
  ASSERT_PRED1(base::PathExists, two);
}

// Tests that a file set's files can be deleted while it's in use and are
// absent upon destruction.
TEST_F(SqliteVfsFileSetTest, FilesCanBeDeleted) {
  auto [one, two] = GetFilePaths();

  {
    ASSERT_OK_AND_ASSIGN(auto file_set, CreateFilesAndBuildVfsFileSet());

    ASSERT_PRED1(base::PathExists, one);
    ASSERT_PRED1(base::PathExists, two);

    ASSERT_PRED1(base::DeleteFile, one);
    ASSERT_PRED1(base::DeleteFile, two);
  }

  ASSERT_FALSE(base::PathExists(one));
  ASSERT_FALSE(base::PathExists(two));

  // No other files should have been left behind.
  ASSERT_PRED1(base::IsDirectoryEmpty, GetTempDir());
}

}  // namespace

}  // namespace persistent_cache
