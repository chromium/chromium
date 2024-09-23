// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/enterprise_companion/test/test_utils.h"

#include <optional>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/no_destructor.h"
#include "base/strings/strcat.h"
#include "chrome/enterprise_companion/installer_paths.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace enterprise_companion {

namespace {

class TestMethodsMac : public TestMethods {
 public:
  TestMethodsMac() = default;
  ~TestMethodsMac() override = default;

  void Clean() override {
    TestMethods::Clean();
    ASSERT_TRUE(base::DeleteFile(GetKSAdminPath()));
  }

  void ExpectInstalled() override {
    TestMethods::ExpectInstalled();
    std::optional<base::FilePath> install_dir = GetInstallDirectory();
    ASSERT_TRUE(install_dir);
    int exe_mode = 0;
    ASSERT_TRUE(base::GetPosixFilePermissions(
        install_dir->AppendASCII(kExecutableName), &exe_mode));
    EXPECT_EQ(exe_mode, base::FILE_PERMISSION_USER_MASK |
                            base::FILE_PERMISSION_READ_BY_GROUP |
                            base::FILE_PERMISSION_EXECUTE_BY_GROUP |
                            base::FILE_PERMISSION_READ_BY_OTHERS |
                            base::FILE_PERMISSION_EXECUTE_BY_OTHERS);
  }

  void Install() override {
    InstallFakeKSAdmin(/*should_succeed=*/true);
    TestMethods::Install();
  }

  void InstallIfNeeded() override {
    InstallFakeKSAdmin(/*should_succeed=*/true);
    TestMethods::InstallIfNeeded();
  }
};

}  // namespace

TestMethods& GetTestMethods() {
  static base::NoDestructor<TestMethodsMac> test_methods;
  return *test_methods;
}

void InstallFakeKSAdmin(bool should_succeed) {
  base::FilePath ksadmin_path = GetKSAdminPath();
  ASSERT_TRUE(base::CreateDirectory(ksadmin_path.DirName()));
  ASSERT_TRUE(base::WriteFile(
      ksadmin_path,
      base::StrCat({"#!/bin/bash\nexit ", should_succeed ? "0" : "1"})));
  ASSERT_TRUE(base::SetPosixFilePermissions(ksadmin_path,
                                            base::FILE_PERMISSION_USER_MASK));
}

}  // namespace enterprise_companion
