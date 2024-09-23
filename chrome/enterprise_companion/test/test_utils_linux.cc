// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/enterprise_companion/test/test_utils.h"

#include <optional>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/no_destructor.h"
#include "chrome/enterprise_companion/installer_paths.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace enterprise_companion {

namespace {

class TestMethodsLinux : public TestMethods {
 public:
  TestMethodsLinux() = default;
  ~TestMethodsLinux() override = default;

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
};

}  // namespace

TestMethods& GetTestMethods() {
  static base::NoDestructor<TestMethodsLinux> test_methods;
  return *test_methods;
}

}  // namespace enterprise_companion
