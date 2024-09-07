// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <optional>

#include "base/base_paths.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/path_service.h"
#include "base/process/launch.h"
#include "base/process/process.h"
#include "base/strings/strcat.h"
#include "base/test/task_environment.h"
#include "chrome/enterprise_companion/installer_paths.h"
#include "chrome/enterprise_companion/test/test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace enterprise_companion {

namespace {

constexpr char kInstallerPkg[] = "enterprise_companion_installer_unsigned.pkg";

}  // namespace

class InstallerPkgTest : public ::testing::Test {
 public:
  void SetUp() override {
    std::optional<base::FilePath> install_dir = GetInstallDirectory();
    ASSERT_TRUE(install_dir);
    install_dir_ = *install_dir;
    ASSERT_TRUE(base::DeletePathRecursively(install_dir_));
    DeleteKSAdmin();
  }

  void TearDown() override {
    ASSERT_TRUE(base::DeletePathRecursively(install_dir_));
    DeleteKSAdmin();
  }

 protected:
  base::test::TaskEnvironment environment_;
  base::FilePath install_dir_;

  // Deletes ksadmin, if it exists.
  void DeleteKSAdmin() {
    base::FilePath ksadmin_path = GetKSAdminPath();
    ASSERT_TRUE(base::DeleteFile(ksadmin_path));
  }

  // Run the PKG installer and expect success or failure.
  void RunInstaller(bool expect_success) {
    base::FilePath installer_pkg_path =
        base::PathService::CheckedGet(base::DIR_EXE).AppendASCII(kInstallerPkg);
    ASSERT_TRUE(base::PathExists(installer_pkg_path));

    base::Process installer_process =
        base::LaunchProcess({"installer", "-pkg",
                             installer_pkg_path.AsUTF8Unsafe(), "-target", "/"},
                            base::LaunchOptionsForTest());

    int exit_code = WaitForProcess(installer_process);
    if (expect_success) {
      EXPECT_EQ(exit_code, 0);
    } else {
      EXPECT_NE(exit_code, 0);
    }
  }
};

TEST_F(InstallerPkgTest, Installs) {
  InstallFakeKSAdmin(/*should_succeed=*/true);

  RunInstaller(/*expect_success=*/true);

  GetTestMethods().ExpectInstalled();
}

TEST_F(InstallerPkgTest, FirstInstallFailsIfKSAdminMissing) {
  DeleteKSAdmin();

  RunInstaller(/*expect_success=*/false);

  EXPECT_FALSE(base::PathExists(install_dir_.AppendASCII(kExecutableName)));
}

TEST_F(InstallerPkgTest, FirstInstallFailsIfKSAdminFails) {
  InstallFakeKSAdmin(/*should_succeed=*/false);

  RunInstaller(/*expect_success=*/false);

  EXPECT_FALSE(base::PathExists(install_dir_.AppendASCII(kExecutableName)));
}

}  // namespace enterprise_companion
