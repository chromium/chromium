// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/files/file_path.h"
#include "base/path_service.h"
#include "chromecast/base/path_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromecast {
namespace {
const char kTestRelPath[] = "rel/path";
const char kTestAbsPath[] = "/abs/path/to/dir";

std::string GetTestString(int base_dir_key) {
  base::FilePath basedir;
  EXPECT_TRUE(base::PathService::Get(base_dir_key, &basedir));
  return basedir.value() + "/" + kTestRelPath;
}

}  // namespace

TEST(PathUtilsTest, GetHomePath) {
  // Test with relative path.
  std::string expected = GetTestString(base::DIR_HOME);
  base::FilePath actual = GetHomePath(base::FilePath(kTestRelPath));
  EXPECT_EQ(expected, actual.value());

  // Test with absolute path.
  actual = GetHomePath(base::FilePath(kTestAbsPath));
  EXPECT_EQ(kTestAbsPath, actual.value());
}

TEST(PathUtilsTest, GetBinPath) {
  // Test with relative path.
  std::string expected = GetTestString(base::DIR_EXE);
  base::FilePath actual = GetBinPath(base::FilePath(kTestRelPath));
  EXPECT_EQ(expected, actual.value());

  // Test with absolute path.
  actual = GetBinPath(base::FilePath(kTestAbsPath));
  EXPECT_EQ(kTestAbsPath, actual.value());
}

TEST(PathUtilsTest, GetTmpPath) {
  // Test with relative path.
  std::string expected = GetTestString(base::DIR_TEMP);
  base::FilePath actual = GetTmpPath(base::FilePath(kTestRelPath));
  EXPECT_EQ(expected, actual.value());

  // Test with absolute path.
  actual = GetTmpPath(base::FilePath(kTestAbsPath));
  EXPECT_EQ(kTestAbsPath, actual.value());
}

}  // chromecast