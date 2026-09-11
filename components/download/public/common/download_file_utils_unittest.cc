// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/download/public/common/download_file_utils.h"

#include <string>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/strings/string_view_util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace download {
namespace {

class DownloadFileUtilsTest : public testing::Test {
 public:
  void SetUp() override { ASSERT_TRUE(temp_dir_.CreateUniqueTempDir()); }

 protected:
  base::ScopedTempDir temp_dir_;
};

TEST_F(DownloadFileUtilsTest, WriteFileAtomicallyWithPermissionsNewFile) {
  base::FilePath file_path =
      temp_dir_.GetPath().Append(FILE_PATH_LITERAL("new_file.txt"));
  const std::string content = "hello atomic write";

  EXPECT_TRUE(WriteFileAtomicallyWithPermissions(file_path,
                                                 base::as_byte_span(content)));

  std::string actual_content;
  ASSERT_TRUE(base::ReadFileToString(file_path, &actual_content));
  EXPECT_EQ(content, actual_content);
}

TEST_F(DownloadFileUtilsTest,
       WriteFileAtomicallyWithPermissionsOverwritesExistingFile) {
  base::FilePath file_path =
      temp_dir_.GetPath().Append(FILE_PATH_LITERAL("overwrite.txt"));
  ASSERT_TRUE(base::WriteFile(file_path, "old_content"));

  base::File::Info initial_info;
  ASSERT_TRUE(base::GetFileInfo(file_path, &initial_info));

  const std::string new_content = "new_atomic_content";
  EXPECT_TRUE(WriteFileAtomicallyWithPermissions(
      file_path, base::as_byte_span(new_content)));

  std::string actual_content;
  ASSERT_TRUE(base::ReadFileToString(file_path, &actual_content));
  EXPECT_EQ(new_content, actual_content);

  base::File::Info updated_info;
  ASSERT_TRUE(base::GetFileInfo(file_path, &updated_info));

  EXPECT_GE(updated_info.last_modified, initial_info.last_modified);
}

TEST_F(DownloadFileUtilsTest, ReplaceFileWithPermissionsSuccess) {
  base::FilePath from_path =
      temp_dir_.GetPath().Append(FILE_PATH_LITERAL("from.txt"));
  base::FilePath to_path =
      temp_dir_.GetPath().Append(FILE_PATH_LITERAL("to.txt"));

  ASSERT_TRUE(base::WriteFile(from_path, "from_data"));
  ASSERT_TRUE(base::WriteFile(to_path, "to_data"));

  EXPECT_TRUE(ReplaceFileWithPermissions(from_path, to_path));
  EXPECT_FALSE(base::PathExists(from_path));

  std::string actual_content;
  ASSERT_TRUE(base::ReadFileToString(to_path, &actual_content));
  EXPECT_EQ("from_data", actual_content);
}

}  // namespace
}  // namespace download
