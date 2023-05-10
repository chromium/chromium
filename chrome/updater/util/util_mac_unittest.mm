// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/updater/util/util.h"

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "testing/gtest/include/gtest/gtest.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace updater {

namespace {
constexpr char kTestFilePath[] = "test_file.a";
constexpr int kPermissionsMask = base::FILE_PERMISSION_USER_MASK |
                                 base::FILE_PERMISSION_READ_BY_GROUP |
                                 base::FILE_PERMISSION_EXECUTE_BY_GROUP |
                                 base::FILE_PERMISSION_READ_BY_OTHERS |
                                 base::FILE_PERMISSION_EXECUTE_BY_OTHERS;
}  // namespace

TEST(UtilTest, ConfirmFilePermissionsTest) {
  base::ScopedTempDir temp_dir_;
  ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());

  base::FilePath test_file_path =
      temp_dir_.GetPath().AppendASCII(kTestFilePath);
  ASSERT_TRUE(base::CreateTemporaryFile(&test_file_path));

  EXPECT_TRUE(
      updater::ConfirmFilePermissions(temp_dir_.GetPath(), kPermissionsMask));
}

}  // namespace updater
