// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/updater/util/util.h"

#include <sys/stat.h>
#include <unistd.h>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/posix/safe_strerror.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace updater {

namespace {

void CheckPermission(const base::FilePath& path, int mode) {
  struct stat st_buf;
  ASSERT_EQ(lstat(path.value().c_str(), &st_buf), 0)
      << path << ": " << base::safe_strerror(errno);
  EXPECT_EQ(st_buf.st_mode & 0777, mode) << path;
}

void SetPermission(const base::FilePath& path, int mode) {
  EXPECT_EQ(lchmod(path.value().c_str(), mode), 0)
      << path << ": " << base::safe_strerror(errno);
  CheckPermission(path, mode);
}

}  // namespace

TEST(UtilTest, SetFilePermissionsRecursiveTest) {
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  const base::FilePath temp = temp_dir.GetPath();

  // Create files.
  ASSERT_TRUE(base::WriteFile(temp.AppendASCII("f1"), ""));
  ASSERT_TRUE(base::WriteFile(temp.AppendASCII("f2"), ""));
  ASSERT_TRUE(base::WriteFile(temp.AppendASCII("f3"), ""));
  ASSERT_TRUE(base::WriteFile(temp.AppendASCII("f4"), ""));
  ASSERT_TRUE(base::WriteFile(temp.AppendASCII("f5"), ""));
  ASSERT_TRUE(base::WriteFile(temp.AppendASCII("f6"), ""));
  ASSERT_TRUE(base::WriteFile(temp.AppendASCII("f7"), ""));
  ASSERT_TRUE(base::CreateDirectory(temp.AppendASCII("d1")));
  ASSERT_TRUE(base::CreateDirectory(temp.AppendASCII("d2")));
  ASSERT_TRUE(base::CreateDirectory(temp.AppendASCII("d3")));
  ASSERT_TRUE(base::WriteFile(temp.AppendASCII("d2").AppendASCII("f"), ""));
  ASSERT_TRUE(base::WriteFile(temp.AppendASCII("d3").AppendASCII("f"), ""));
  ASSERT_TRUE(base::CreateSymbolicLink(temp.AppendASCII("d1"),
                                       temp.AppendASCII("l_d1")));
  ASSERT_TRUE(base::CreateSymbolicLink(temp.AppendASCII("n"),
                                       temp.AppendASCII("l_dangling")));
  ASSERT_TRUE(base::CreateSymbolicLink(temp.AppendASCII("f2"),
                                       temp.AppendASCII("l_f2")));

  SetPermission(temp, 0700);
  SetPermission(temp.AppendASCII("f1"), 0600);
  SetPermission(temp.AppendASCII("f2"), 0600);
  SetPermission(temp.AppendASCII("f3"), 0700);
  SetPermission(temp.AppendASCII("f4"), 0000);
  SetPermission(temp.AppendASCII("f5"), 0400);
  SetPermission(temp.AppendASCII("f6"), 0650);
  SetPermission(temp.AppendASCII("f7"), 0001);
  SetPermission(temp.AppendASCII("d1"), 0700);
  SetPermission(temp.AppendASCII("d2"), 0700);
  SetPermission(temp.AppendASCII("d2").AppendASCII("f"), 0600);
  SetPermission(temp.AppendASCII("d3").AppendASCII("f"), 0600);
  SetPermission(temp.AppendASCII("d3"), 0000);
  SetPermission(temp.AppendASCII("l_d1"), 0700);
  SetPermission(temp.AppendASCII("l_dangling"), 0700);
  SetPermission(temp.AppendASCII("l_f2"), 0600);

  // Update permissions and check results.
  ASSERT_TRUE(updater::SetFilePermissionsRecursive(temp));
  CheckPermission(temp, 0755);
  CheckPermission(temp.AppendASCII("f1"), 0644);
  CheckPermission(temp.AppendASCII("f2"), 0644);
  CheckPermission(temp.AppendASCII("f3"), 0755);
  CheckPermission(temp.AppendASCII("f4"), 0644);
  CheckPermission(temp.AppendASCII("f5"), 0644);
  CheckPermission(temp.AppendASCII("f6"), 0755);
  CheckPermission(temp.AppendASCII("f7"), 0755);
  CheckPermission(temp.AppendASCII("d1"), 0755);
  CheckPermission(temp.AppendASCII("d2"), 0755);
  CheckPermission(temp.AppendASCII("d2").AppendASCII("f"), 0644);
  CheckPermission(temp.AppendASCII("d3"), 0755);
  CheckPermission(temp.AppendASCII("d3").AppendASCII("f"), 0644);
  CheckPermission(temp.AppendASCII("l_d1"), 0755);
  CheckPermission(temp.AppendASCII("l_dangling"), 0755);
  CheckPermission(temp.AppendASCII("l_f2"), 0644);
}

TEST(UtilTest, SetFilePermissionsRecursiveTestFile) {
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  const base::FilePath temp = temp_dir.GetPath();
  const base::FilePath file = temp.AppendASCII("f1");
  ASSERT_TRUE(base::WriteFile(file, ""));
  SetPermission(file, 0600);
  ASSERT_TRUE(updater::SetFilePermissionsRecursive(file));
  CheckPermission(file, 0644);
}

TEST(UtilTest, SetFilePermissionsRecursiveTestLink) {
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  const base::FilePath temp = temp_dir.GetPath();
  const base::FilePath dir = temp.AppendASCII("d1");
  const base::FilePath file = dir.AppendASCII("f1");
  const base::FilePath link = temp.AppendASCII("l1");
  ASSERT_TRUE(base::CreateDirectory(dir));
  ASSERT_TRUE(base::WriteFile(file, ""));
  ASSERT_TRUE(base::CreateSymbolicLink(dir, link));
  SetPermission(dir, 0700);
  SetPermission(file, 0600);
  SetPermission(link, 0700);
  ASSERT_TRUE(updater::SetFilePermissionsRecursive(link));
  CheckPermission(dir, 0700);
  CheckPermission(file, 0600);
  CheckPermission(link, 0755);
}

}  // namespace updater
