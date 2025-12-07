// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/updater/util/posix_util.h"

#include <sys/stat.h>
#include <unistd.h>

#include <string>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace updater {
namespace {

std::string Read(const base::FilePath& path) {
  std::string contents;
  EXPECT_TRUE(base::ReadFileToString(path, &contents));
  return contents;
}

int Stat(const base::FilePath& path) {
  struct stat status = {};
  EXPECT_EQ(lstat(path.value().c_str(), &status), 0);
  return static_cast<int>(status.st_mode & 0777);
}

}  // namespace

TEST(UtilTest, CopyDir) {
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  const base::FilePath from = temp_dir.GetPath().Append("from");
  ASSERT_TRUE(base::CreateDirectory(from.Append("d1")));
  ASSERT_TRUE(base::WriteFile(from.Append("d1").Append("f1"), "c1"));
  ASSERT_TRUE(base::WriteFile(from.Append("f2"), "c2"));
  ASSERT_EQ(0, symlink("./d1/f1", from.Append("l1").value().c_str()));

  base::FilePath to = temp_dir.GetPath().Append("to");
  ASSERT_TRUE(base::CreateDirectory(to));
  CopyDir(from, to, true);
  to = to.Append("from");
  ASSERT_TRUE(base::PathExists(to));
  EXPECT_EQ(Read(to.Append("d1").Append("f1")), "c1");
  EXPECT_EQ(Read(to.Append("f2")), "c2");
  EXPECT_EQ(Read(to.Append("l1")), "c1");
  char link_contents[1000] = {};
  ASSERT_EQ(readlink(to.Append("l1").value().c_str(), link_contents,
                     std::size(link_contents)),
            7);
  EXPECT_EQ(std::string(link_contents), "./d1/f1");
}

TEST(UtilTest, CopyDirPermissions) {
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  const base::FilePath from = temp_dir.GetPath().Append("from");
  ASSERT_TRUE(base::CreateDirectory(from.Append("d1")));
  ASSERT_TRUE(base::WriteFile(from.Append("d1").Append("f1"), "c1"));
  ASSERT_TRUE(base::WriteFile(from.Append("f2"), "c2"));
  ASSERT_TRUE(base::SetPosixFilePermissions(from.Append("f2"), 0700));
  ASSERT_EQ(0, symlink("./d1/f1", from.Append("l1").value().c_str()));

  {  // World-readable.
    base::FilePath to = temp_dir.GetPath().Append("to_1");
    ASSERT_TRUE(base::CreateDirectory(to));
    CopyDir(from, to, true);
    to = to.Append("from");
    EXPECT_EQ(Stat(to), 0755);
    EXPECT_EQ(Stat(to.Append("d1")), 0755);
    EXPECT_EQ(Stat(to.Append("d1").Append("f1")), 0644);
    EXPECT_EQ(Stat(to.Append("f2")), 0755);
  }

  {  // Not world-readable.
    base::FilePath to = temp_dir.GetPath().Append("to_2");
    ASSERT_TRUE(base::CreateDirectory(to));
    CopyDir(from, to, false);
    to = to.Append("from");
    EXPECT_EQ(Stat(to), 0700);
    EXPECT_EQ(Stat(to.Append("d1")), 0700);
    EXPECT_EQ(Stat(to.Append("d1").Append("f1")), 0600);
    EXPECT_EQ(Stat(to.Append("f2")), 0700);
  }
}

}  // namespace updater
