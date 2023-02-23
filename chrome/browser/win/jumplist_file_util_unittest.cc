// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/win/jumplist_file_util.h"

#include <string>

#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

// Random text to write into a file.
constexpr char kFileContent[] = "I'm random context.";

// Maximum files allowed to delete and maximum attempt failures allowed.
// For unit tests purpose only.
const int kFileDeleteLimitForTest = 1;

// Simple function to dump some text into a new file.
void CreateTextFile(const base::FilePath& file_name,
                    const std::string& contents) {
  ASSERT_TRUE(base::WriteFile(file_name, contents));
  ASSERT_TRUE(base::PathExists(file_name));
}

}  // namespace

class JumpListFileUtilTest : public testing::Test {
 protected:
  // A temporary directory where all file IO operations take place .
  base::ScopedTempDir temp_dir_;

  // Get the path to the temporary directory.
  const base::FilePath& temp_dir_path() { return temp_dir_.GetPath(); }

  // Create a unique temporary directory.
  void SetUp() override { ASSERT_TRUE(temp_dir_.CreateUniqueTempDir()); }
};

TEST_F(JumpListFileUtilTest, DeleteDirectoryContent) {
  base::FilePath dir_path = temp_dir_path();

  // Create a file.
  base::FilePath file_name =
      dir_path.Append(FILE_PATH_LITERAL("TestDeleteFile.txt"));
  ASSERT_NO_FATAL_FAILURE(CreateTextFile(file_name, kFileContent));

  // Delete the directory content using DeleteDirectoryContent(). The file
  // should be deleted and the directory remains.
  DeleteDirectoryContent(dir_path, kFileDeleteLimit);
  EXPECT_FALSE(PathExists(file_name));
  EXPECT_TRUE(DirectoryExists(dir_path));
}

TEST_F(JumpListFileUtilTest, DeleteSubDirectory) {
  base::FilePath dir_path = temp_dir_path();

  // Create a subdirectory.
  base::FilePath test_subdir =
      dir_path.Append(FILE_PATH_LITERAL("TestSubDirectory"));
  ASSERT_NO_FATAL_FAILURE(CreateDirectory(test_subdir));

  // Delete the directory using DeleteDirectory(), which should fail because
  // a subdirectory exists. Therefore, both root directory and sub-directory
  // should still exist.
  DeleteDirectory(dir_path, kFileDeleteLimit);
  EXPECT_TRUE(DirectoryExists(dir_path));
  EXPECT_TRUE(DirectoryExists(test_subdir));

  // Delete the subdirectory alone should be working.
  DeleteDirectory(test_subdir, kFileDeleteLimit);
  EXPECT_TRUE(DirectoryExists(dir_path));
  EXPECT_FALSE(DirectoryExists(test_subdir));
}

TEST_F(JumpListFileUtilTest, DeleteMaxFilesAllowed) {
  base::FilePath dir_path = temp_dir_path();

  // Create 2 files.
  base::FilePath file_name =
      dir_path.Append(FILE_PATH_LITERAL("TestDeleteFile1.txt"));
  ASSERT_NO_FATAL_FAILURE(CreateTextFile(file_name, kFileContent));

  file_name = dir_path.Append(FILE_PATH_LITERAL("TestDeleteFile2.txt"));
  ASSERT_NO_FATAL_FAILURE(CreateTextFile(file_name, kFileContent));

  // Delete the directory content using DeleteDirectoryContent().
  // Sine the maximum files allowed to delete is 1, only 1 out of the 2
  // files is deleted. Therefore, the directory is not empty yet.
  DeleteDirectoryContent(dir_path, kFileDeleteLimitForTest);
  EXPECT_FALSE(base::IsDirectoryEmpty(dir_path));

  // Delete another file, and now the directory is empty.
  DeleteDirectoryContent(dir_path, kFileDeleteLimitForTest);
  EXPECT_TRUE(base::IsDirectoryEmpty(dir_path));
  EXPECT_TRUE(DirectoryExists(dir_path));
}

TEST_F(JumpListFileUtilTest, FilesExceedLimitInDir) {
  base::FilePath dir_path = temp_dir_path();

  // Create 2 files.
  base::FilePath file_name =
      dir_path.Append(FILE_PATH_LITERAL("TestFile1.txt"));
  ASSERT_NO_FATAL_FAILURE(CreateTextFile(file_name, kFileContent));

  file_name = dir_path.Append(FILE_PATH_LITERAL("TestFile2.txt"));
  ASSERT_NO_FATAL_FAILURE(CreateTextFile(file_name, kFileContent));

  EXPECT_TRUE(FilesExceedLimitInDir(dir_path, 1));
  EXPECT_FALSE(FilesExceedLimitInDir(dir_path, 2));

  DeleteDirectory(dir_path, kFileDeleteLimit);
}

TEST_F(JumpListFileUtilTest, DeleteNonCachedFiles) {
  base::FilePath dir_path = temp_dir_path();

  base::flat_set<base::FilePath> cached_files;

  // Create 1 file and cache its filename.
  base::FilePath file_name =
      dir_path.Append(FILE_PATH_LITERAL("TestFile1.txt"));
  ASSERT_NO_FATAL_FAILURE(CreateTextFile(file_name, kFileContent));

  cached_files.insert(file_name);

  // Create another file but not cache its filename.
  file_name = dir_path.Append(FILE_PATH_LITERAL("TestFile2.txt"));
  ASSERT_NO_FATAL_FAILURE(CreateTextFile(file_name, kFileContent));

  // The second file created will be deleted as its filename is not in the
  // cache, while the first file remains.
  DeleteNonCachedFiles(dir_path, cached_files);
  EXPECT_FALSE(base::IsDirectoryEmpty(dir_path));

  // Clear the set and delete again, and now the first file should be gone.
  cached_files.clear();
  DeleteNonCachedFiles(dir_path, cached_files);
  EXPECT_TRUE(base::IsDirectoryEmpty(dir_path));
}
