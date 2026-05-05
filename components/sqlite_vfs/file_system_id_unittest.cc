// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sqlite_vfs/file_system_id.h"

#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/test/gmock_expected_support.h"
#include "base/test/metrics/histogram_tester.h"
#include "components/sqlite_vfs/client.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace sqlite_vfs {

class FileSystemIdTest : public testing::Test {
 protected:
  void SetUp() override { ASSERT_TRUE(temp_dir_.CreateUniqueTempDir()); }

  base::ScopedTempDir temp_dir_;
};

TEST_F(FileSystemIdTest, InvalidFileReturnsDefaultId) {
  base::HistogramTester histogram_tester;
  base::File invalid_file;
  std::optional<FileSystemId> id = GetFileSystemId(Client::kTest, invalid_file);

  EXPECT_EQ(id, std::nullopt);
  histogram_tester.ExpectUniqueSample("SandboxedVfs.GetFileSystemIdError.Test",
                                      -base::File::FILE_ERROR_FAILED, 1);
}

TEST_F(FileSystemIdTest, SameFileSameId) {
  base::FilePath file_path = temp_dir_.GetPath().AppendASCII("test_file");

  // Open handle 1
  base::File file1(file_path, base::File::FLAG_CREATE_ALWAYS |
                                  base::File::FLAG_READ |
                                  base::File::FLAG_WRITE);
  ASSERT_TRUE(file1.IsValid());

  // Open handle 2 to the same file
  base::File file2(file_path, base::File::FLAG_OPEN | base::File::FLAG_READ);
  ASSERT_TRUE(file2.IsValid());

  ASSERT_OK_AND_ASSIGN(FileSystemId id1, GetFileSystemId(Client::kTest, file1));
  ASSERT_OK_AND_ASSIGN(FileSystemId id2, GetFileSystemId(Client::kTest, file2));

  EXPECT_EQ(id1, id2);
}

TEST_F(FileSystemIdTest, DifferentFilesDifferentIds) {
  base::FilePath file_path1 = temp_dir_.GetPath().AppendASCII("test_file_1");
  base::FilePath file_path2 = temp_dir_.GetPath().AppendASCII("test_file_2");

  base::File file1(file_path1, base::File::FLAG_CREATE_ALWAYS |
                                   base::File::FLAG_READ |
                                   base::File::FLAG_WRITE);
  ASSERT_TRUE(file1.IsValid());

  base::File file2(file_path2, base::File::FLAG_CREATE_ALWAYS |
                                   base::File::FLAG_READ |
                                   base::File::FLAG_WRITE);
  ASSERT_TRUE(file2.IsValid());

  ASSERT_OK_AND_ASSIGN(FileSystemId id1, GetFileSystemId(Client::kTest, file1));
  ASSERT_OK_AND_ASSIGN(FileSystemId id2, GetFileSystemId(Client::kTest, file2));

  EXPECT_NE(id1, id2);
}

}  // namespace sqlite_vfs
