// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/installer/mini_installer/mini_file.h"

#include <utility>

#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace mini_installer {

class MiniFileTest : public ::testing::Test {
 protected:
  void SetUp() override { ASSERT_TRUE(temp_dir_.CreateUniqueTempDir()); }

  void TearDown() override { EXPECT_TRUE(temp_dir_.Delete()); }

  const base::FilePath& temp_dir() const { return temp_dir_.GetPath(); }

 private:
  base::ScopedTempDir temp_dir_;
};

// Create should create a file.
TEST_F(MiniFileTest, Create) {
  const base::FilePath file_path = temp_dir().Append(FILE_PATH_LITERAL("HUM"));

  MiniFile file;
  ASSERT_TRUE(file.Create(file_path.value().c_str()));
  ASSERT_TRUE(base::PathExists(file_path));
}

// Created files should be deletable by others and should vanish when closed.
TEST_F(MiniFileTest, CreateDeleteIsShared) {
  const base::FilePath file_path = temp_dir().Append(FILE_PATH_LITERAL("HUM"));

  MiniFile file;
  ASSERT_TRUE(file.Create(file_path.value().c_str()));
  // DeleteFile uses POSIX semantics, so the file appears to vanish immediately.
  ASSERT_TRUE(base::DeleteFile(file_path));
  file.Close();
  ASSERT_FALSE(base::PathExists(file_path));
}

// DeleteOnClose should work as advertised.
TEST_F(MiniFileTest, DeleteOnClose) {
  const base::FilePath file_path = temp_dir().Append(FILE_PATH_LITERAL("HUM"));

  MiniFile file;
  ASSERT_TRUE(file.Create(file_path.value().c_str()));
  ASSERT_TRUE(file.DeleteOnClose());

  // The file can no longer be opened now that it has been marked for deletion.
  // Attempts to do so will fail with ERROR_ACCESS_DENIED. Under the covers, the
  // NT status code is STATUS_DELETE_PENDING. Since base::PathExists will return
  // false in this case, confirm the file's existence by trying to open it.
  base::File the_file(file_path, base::File::FLAG_OPEN | base::File::FLAG_READ |
                                     base::File::FLAG_WIN_SHARE_DELETE);
  ASSERT_FALSE(the_file.IsValid());
  ASSERT_EQ(the_file.error_details(), base::File::FILE_ERROR_ACCESS_DENIED);

  file.Close();
  ASSERT_FALSE(base::PathExists(file_path));
}

// Close should really close.
TEST_F(MiniFileTest, Close) {
  const base::FilePath file_path = temp_dir().Append(FILE_PATH_LITERAL("HUM"));

  MiniFile file;
  ASSERT_TRUE(file.Create(file_path.value().c_str()));
  file.Close();
  EXPECT_FALSE(file.IsValid());
  EXPECT_EQ(*file.path(), 0);
  ASSERT_TRUE(base::PathExists(file_path));
  base::File f(file_path, base::File::FLAG_OPEN | base::File::FLAG_READ |
                              base::File::FLAG_WIN_EXCLUSIVE_READ |
                              base::File::FLAG_WIN_EXCLUSIVE_WRITE);
  ASSERT_TRUE(f.IsValid());
}

// DuplicateHandle should work as advertized.
TEST_F(MiniFileTest, DuplicateHandle) {
  const base::FilePath file_path = temp_dir().Append(FILE_PATH_LITERAL("HUM"));

  MiniFile file;
  ASSERT_TRUE(file.Create(file_path.value().c_str()));
  HANDLE dup = file.DuplicateHandle();
  ASSERT_NE(dup, INVALID_HANDLE_VALUE);

  // Check that the two handles reference the same file.
  BY_HANDLE_FILE_INFORMATION info1 = {};
  ASSERT_NE(::GetFileInformationByHandle(file.GetHandleUnsafe(), &info1),
            FALSE);
  BY_HANDLE_FILE_INFORMATION info2 = {};
  ASSERT_NE(::GetFileInformationByHandle(dup, &info2), FALSE);
  EXPECT_EQ(info1.dwVolumeSerialNumber, info2.dwVolumeSerialNumber);
  EXPECT_EQ(info1.nFileIndexHigh, info2.nFileIndexHigh);
  EXPECT_EQ(info1.nFileIndexLow, info2.nFileIndexLow);

  ::CloseHandle(std::exchange(dup, INVALID_HANDLE_VALUE));
}

TEST_F(MiniFileTest, Path) {
  const base::FilePath file_path = temp_dir().Append(FILE_PATH_LITERAL("HUM"));

  MiniFile file;
  EXPECT_EQ(*file.path(), 0);

  ASSERT_TRUE(file.Create(file_path.value().c_str()));
  EXPECT_STREQ(file.path(), file_path.value().c_str());

  file.Close();
  EXPECT_EQ(*file.path(), 0);
}

TEST_F(MiniFileTest, GetHandleUnsafe) {
  const base::FilePath file_path = temp_dir().Append(FILE_PATH_LITERAL("HUM"));

  MiniFile file;
  EXPECT_EQ(file.GetHandleUnsafe(), INVALID_HANDLE_VALUE);

  ASSERT_TRUE(file.Create(file_path.value().c_str()));
  EXPECT_NE(file.GetHandleUnsafe(), INVALID_HANDLE_VALUE);

  file.Close();
  EXPECT_EQ(file.GetHandleUnsafe(), INVALID_HANDLE_VALUE);
}

}  // namespace mini_installer
