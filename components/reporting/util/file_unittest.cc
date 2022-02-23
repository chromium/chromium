// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/reporting/util/file.h"

#include "base/files/file.h"
#include "base/files/scoped_temp_dir.h"
#include "base/test/test_file_util.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace reporting {
namespace {

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
  base::FileEnumerator dir_enum(dir_path, /*recursive=*/false,
                                base::FileEnumerator::FILES,
                                FILE_PATH_LITERAL("*"));
  ASSERT_TRUE(DeleteFilesWarnIfFailed(dir_enum))
      << "Failed to delete " << file_path.MaybeAsASCII();
  ASSERT_FALSE(base::PathExists(file_path))
      << "Deletion succeeds but " << file_path.MaybeAsASCII()
      << " still exists.";
}

}  // namespace
}  // namespace reporting
