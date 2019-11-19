// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/common/passwords_directory_util_ios.h"

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/task/post_task.h"
#include "base/test/task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

// Tests that DeletePasswordsDirectory() actually deletes the directory.
TEST(PasswordsDirectoryUtilTest, Deletion) {
  base::test::TaskEnvironment environment;
  base::FilePath dir;
  ASSERT_TRUE(password_manager::GetPasswordsDirectory(&dir));
  ASSERT_TRUE(CreateDirectory(dir));
  base::FilePath file = dir.Append(FILE_PATH_LITERAL("TestPasswords.csv"));
  EXPECT_EQ(0, WriteFile(file, "", 0));

  // Verify that the file was created in the passwords directory.
  ASSERT_TRUE(base::PathExists(file));

  // Delete download directory.
  password_manager::DeletePasswordsDirectory();

  environment.RunUntilIdle();

  // Verify passwords directory deletion.
  EXPECT_FALSE(base::PathExists(dir));
}
