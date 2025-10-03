// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/persistent_cache/sqlite/vfs/sqlite_database_vfs_file_set.h"

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/test/gmock_expected_support.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace persistent_cache {

namespace {

class SqliteVfsFileSetTest : public testing::Test {
 protected:
  void SetUp() override { ASSERT_TRUE(temp_dir_.CreateUniqueTempDir()); }

  const base::FilePath& GetTempDir() const { return temp_dir_.GetPath(); }

 private:
  base::ScopedTempDir temp_dir_;
};

// Tests that creating and destroying a file set doesn't delete the files.
TEST_F(SqliteVfsFileSetTest, FilesAreNotDeleted) {
  base::FilePath one = GetTempDir().Append(FILE_PATH_LITERAL("one"));
  base::FilePath two = GetTempDir().Append(FILE_PATH_LITERAL("one"));

  ASSERT_FALSE(base::PathExists(one));
  ASSERT_FALSE(base::PathExists(two));

  auto file_set = SqliteVfsFileSet::Create(one, two);
  ASSERT_TRUE(file_set.has_value());

  ASSERT_PRED1(base::PathExists, one);
  ASSERT_PRED1(base::PathExists, two);

  file_set.reset();

  ASSERT_PRED1(base::PathExists, one);
  ASSERT_PRED1(base::PathExists, two);
}

// Tests that a file set's files can be deleted while it's in use and are
// absent upon destruction.
TEST_F(SqliteVfsFileSetTest, FilesCanBeDeleted) {
  base::FilePath one = GetTempDir().Append(FILE_PATH_LITERAL("one"));
  base::FilePath two = GetTempDir().Append(FILE_PATH_LITERAL("one"));

  ASSERT_FALSE(base::PathExists(one));
  ASSERT_FALSE(base::PathExists(two));

  auto file_set = SqliteVfsFileSet::Create(one, two);
  ASSERT_TRUE(file_set.has_value());

  ASSERT_PRED1(base::PathExists, one);
  ASSERT_PRED1(base::PathExists, two);

  ASSERT_PRED1(base::DeleteFile, one);
  ASSERT_PRED1(base::DeleteFile, two);

  file_set.reset();

  ASSERT_FALSE(base::PathExists(one));
  ASSERT_FALSE(base::PathExists(two));

  // No other files should have been left behind.
  ASSERT_PRED1(base::IsDirectoryEmpty, GetTempDir());
}

}  // namespace

}  // namespace persistent_cache
