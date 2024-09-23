// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "components/reporting/util/file.h"

#include <string>

#include "base/files/file.h"
#include "base/files/scoped_temp_dir.h"
#include "base/test/test_file_util.h"
#include "components/reporting/util/status_macros.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::StrEq;

namespace reporting {
namespace {

constexpr char kNewFile[] = "to_create.txt";
constexpr char kWriteDataOne[] = "hello world!";
constexpr char kWriteDataTwo[] = "bye world!";
constexpr char kMultiLineData[] = "12\n34\n56\n78\n";
constexpr size_t kMultiLineDataLineLength = 3;
constexpr size_t kMultiLineDataLines = 4;
constexpr size_t kOverFlowPos = 256;

void RemoveAndTruncateTest(const base::FilePath& file_path,
                           uint32_t pos,
                           int expected_lines_removed) {
  const auto remove_status = RemoveAndTruncateLine(file_path, 0);
  ASSERT_TRUE(remove_status.has_value()) << remove_status.error();
  const auto read_status = MaybeReadFile(file_path, 0);
  ASSERT_TRUE(read_status.has_value()) << read_status.error();
  ASSERT_THAT(
      read_status.value(),
      StrEq(
          &kMultiLineData[expected_lines_removed * kMultiLineDataLineLength]));
}

TEST(FileTest, DeleteFileWarnIfFailed) {
  // This test briefly tests DeleteFileWarnIfFailed, as it mostly calls
  // DeleteFile(), which should be more extensively tested in base.
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  const auto dir_path = temp_dir.GetPath();
  ASSERT_TRUE(base::DirectoryExists(dir_path));

  base::FilePath file_path;
  ASSERT_TRUE(base::CreateTemporaryFileInDir(dir_path, &file_path));

  // Delete an existing file with no permission.
  // Don't test on Fuchsia: No file permission support. See
  // base/files/file_util_unittest.cc for some similar tests being skipped.
#if !BUILDFLAG(IS_FUCHSIA)
  {
    // On Windows, we open the file to prevent it from being deleted. Otherwise,
    // we modify the directory permission to prevent it from being deleted.
#if BUILDFLAG(IS_WIN)
    base::File file(file_path, base::File::FLAG_OPEN | base::File::FLAG_READ);
    ASSERT_TRUE(file.IsValid());
#else   // BUILDFLAG(IS_WIN)
    base::FilePermissionRestorer restore_permissions_for(dir_path);
    // Get rid of the write permission from temp_dir
    ASSERT_TRUE(base::MakeFileUnwritable(dir_path));
    // Ensure no deletion permission
    ASSERT_FALSE(base::PathIsWritable(dir_path));
#endif  // BUILDFLAG(IS_WIN)
    ASSERT_TRUE(base::PathExists(file_path));
    ASSERT_FALSE(DeleteFileWarnIfFailed(file_path))
        << "Deletion of an existing file without permission should fail";
  }
#endif  // !BUILDFLAG(IS_FUCHSIA)

  {
    // Delete with permission
    ASSERT_TRUE(base::PathIsWritable(dir_path));  // Ensure deletion permission
    ASSERT_TRUE(base::PathExists(file_path));
    ASSERT_TRUE(DeleteFileWarnIfFailed(file_path))
        << "Deletion of an existing file should succeed";
    ASSERT_FALSE(base::PathExists(file_path)) << "File failed to be deleted";
  }

  // Delete a non-existing file
  {
    ASSERT_FALSE(base::PathExists(file_path));
    ASSERT_TRUE(DeleteFileWarnIfFailed(file_path))
        << "Deletion of a nonexisting file should succeed";
  }
}

TEST(FileTest, DeleteFilesWarnIfFailed) {
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  const auto dir_path = temp_dir.GetPath();
  ASSERT_TRUE(base::DirectoryExists(dir_path));

  base::FilePath file_path;
  ASSERT_TRUE(base::CreateTemporaryFileInDir(dir_path, &file_path));

  // empty the directory
  ASSERT_TRUE(DeleteFilesWarnIfFailed(base::FileEnumerator(
      dir_path, /*recursive=*/false, base::FileEnumerator::FILES,
      FILE_PATH_LITERAL("*"))))
      << "Failed to delete " << file_path.MaybeAsASCII();
  ASSERT_FALSE(base::PathExists(file_path))
      << "Deletion succeeds but " << file_path.MaybeAsASCII()
      << " still exists.";
}

TEST(FileTest, DeleteFilesWarnIfFailedSubSubDir) {
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  const auto dir_path = temp_dir.GetPath();
  ASSERT_TRUE(base::DirectoryExists(dir_path));

  ASSERT_TRUE(
      base::CreateDirectory(dir_path.Append(FILE_PATH_LITERAL("subdir0"))));
  ASSERT_TRUE(base::CreateDirectory(
      dir_path.Append(FILE_PATH_LITERAL("subdir0/subdir1"))));
  ASSERT_TRUE(base::CreateDirectory(
      dir_path.Append(FILE_PATH_LITERAL("subdir0/subdir1/subdir2"))));

  // empty the directory
  ASSERT_TRUE(DeleteFilesWarnIfFailed(base::FileEnumerator(
      dir_path, /*recursive=*/true,
      base::FileEnumerator::FILES | base::FileEnumerator::DIRECTORIES)));
  ASSERT_FALSE(base::PathExists(dir_path.Append(FILE_PATH_LITERAL("subdir0"))))
      << dir_path << " is not empty.";
}

TEST(FileTest, ReadWriteFile) {
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  const auto dir_path = temp_dir.GetPath();
  ASSERT_TRUE(base::DirectoryExists(dir_path));

  base::FilePath file_path;
  ASSERT_TRUE(base::CreateTemporaryFileInDir(dir_path, &file_path));

  auto write_status = MaybeWriteFile(file_path, kWriteDataOne);
  ASSERT_OK(write_status) << write_status;

  auto read_status = MaybeReadFile(file_path, /*offset=*/0);
  ASSERT_TRUE(read_status.has_value()) << read_status.error();
  EXPECT_EQ(read_status.value(), kWriteDataOne);

  // Overwrite file.
  write_status = MaybeWriteFile(file_path, kWriteDataTwo);
  ASSERT_OK(write_status) << write_status;

  read_status = MaybeReadFile(file_path, /*offset=*/0);
  ASSERT_TRUE(read_status.has_value()) << read_status.error();
  EXPECT_EQ(read_status.value(), kWriteDataTwo);

  // Read file at an out of bounds index
  read_status = MaybeReadFile(file_path, kOverFlowPos);
  ASSERT_FALSE(read_status.has_value());
  EXPECT_EQ(read_status.error().error_code(), error::DATA_LOSS);
}

TEST(FileTest, AppendLine) {
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  const auto dir_path = temp_dir.GetPath();
  ASSERT_TRUE(base::DirectoryExists(dir_path));

  base::FilePath file_path;
  ASSERT_TRUE(base::CreateTemporaryFileInDir(dir_path, &file_path));

  // Create files.
  auto status = AppendLine(dir_path.AppendASCII(kNewFile), kWriteDataOne);
  ASSERT_OK(status) << status;

  status = AppendLine(file_path, kWriteDataOne);
  auto read_status = MaybeReadFile(file_path, /*offset=*/0);
  ASSERT_TRUE(read_status.has_value()) << read_status.error();
  ASSERT_EQ(read_status.value(), base::StrCat({kWriteDataOne, "\n"}));

  status = AppendLine(file_path, kWriteDataTwo);
  read_status = MaybeReadFile(file_path, /*offset=*/0);
  ASSERT_TRUE(read_status.has_value()) << read_status.error();
  ASSERT_EQ(read_status.value(),
            base::StrCat({kWriteDataOne, "\n", kWriteDataTwo, "\n"}));
}

TEST(FileTest, RemoveAndTruncateLine) {
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  const auto dir_path = temp_dir.GetPath();
  ASSERT_TRUE(base::DirectoryExists(dir_path));

  base::FilePath file_path;
  ASSERT_TRUE(base::CreateTemporaryFileInDir(dir_path, &file_path));

  const auto write_status = MaybeWriteFile(file_path, kMultiLineData);
  ASSERT_OK(write_status) << write_status;

  // Load test data into string for substr method.
  const std::string multi_line_ref(kMultiLineData);
  int expected_lines_removed = 1;

  // Remove at beginning of line
  RemoveAndTruncateTest(file_path, 0, expected_lines_removed++);

  // Remove at middle of line
  RemoveAndTruncateTest(file_path, kMultiLineDataLineLength / 2,
                        expected_lines_removed++);

  // Remove at end of line
  RemoveAndTruncateTest(file_path, kMultiLineDataLineLength - 1,
                        expected_lines_removed++);

  // Remove at end of file
  const auto lines_left = kMultiLineDataLines - expected_lines_removed;
  RemoveAndTruncateTest(file_path, kMultiLineDataLineLength * lines_left - 1,
                        expected_lines_removed);
}

}  // namespace
}  // namespace reporting
