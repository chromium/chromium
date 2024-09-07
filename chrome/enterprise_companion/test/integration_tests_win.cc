// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <optional>
#include <string>

#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/path_service.h"
#include "base/process/launch.h"
#include "base/process/process.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/task_environment.h"
#include "base/win/registry.h"
#include "base/win/windows_types.h"
#include "chrome/enterprise_companion/enterprise_companion.h"
#include "chrome/enterprise_companion/enterprise_companion_branding.h"
#include "chrome/enterprise_companion/enterprise_companion_version.h"
#include "chrome/enterprise_companion/installer.h"
#include "chrome/enterprise_companion/installer_paths.h"
#include "chrome/enterprise_companion/test/test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace enterprise_companion {

namespace {

// The filename of the companion app binary under test.
constexpr char kTestExe[] = "enterprise_companion_test.exe";

}  // namespace

class InstallerTest : public ::testing::Test {
 public:
  void SetUp() override {
    std::optional<base::FilePath> install_dir = GetInstallDirectory();
    ASSERT_TRUE(install_dir);
    install_dir_ = *install_dir;
    GetTestMethods().Clean();
  }

  void TearDown() override { GetTestMethods().Clean(); }

 protected:
  base::test::TaskEnvironment environment_;
  // The install directory for the current process architecture.
  base::FilePath install_dir_;
  // If 64-on-64, the 32-bit install directory. If 32-on-64 the 64-bit install
  // directory. Otherwise nullopt.
  std::optional<base::FilePath> alt_install_dir_ =
      GetInstallDirectoryForAlternateArch();

  // Run the installer and expect success or failure.
  void RunInstaller(bool expect_success) {
    base::FilePath installer_pkg_path =
        base::PathService::CheckedGet(base::DIR_EXE).AppendASCII(kTestExe);
    ASSERT_TRUE(base::PathExists(installer_pkg_path));

    base::CommandLine command_line(installer_pkg_path);
    command_line.AppendSwitch(kInstallSwitch);

    base::Process installer_process = base::LaunchProcess(command_line, {});

    int exit_code = WaitForProcess(installer_process);
    if (expect_success) {
      EXPECT_EQ(exit_code, 0);
    } else {
      EXPECT_NE(exit_code, 0);
    }
  }

  void SetUpdaterRegistration(const std::wstring version,
                              const std::wstring name) {
    base::win::RegKey app_key(HKEY_LOCAL_MACHINE, kAppRegKey,
                              KEY_WRITE | KEY_WOW64_32KEY);

    ASSERT_EQ(app_key.WriteValue(kRegValuePV, version.c_str()), ERROR_SUCCESS);
    ASSERT_EQ(app_key.WriteValue(kRegValueName, name.c_str()), ERROR_SUCCESS);
  }
};

TEST_F(InstallerTest, FirstInstall) {
  RunInstaller(true);

  ASSERT_TRUE(base::PathExists(install_dir_.AppendASCII(kExecutableName)));
  ASSERT_FALSE(alt_install_dir_ && base::PathExists(*alt_install_dir_));

  ExpectUpdaterRegistration();
}

TEST_F(InstallerTest, OverinstallSameArch) {
  SetUpdaterRegistration(L"0.0.0.1", L"Prehistoric Enterprise Companion");
  ASSERT_TRUE(base::CreateDirectory(install_dir_));
  ASSERT_TRUE(base::WriteFile(install_dir_.AppendASCII(kExecutableName), ""));

  RunInstaller(true);

  ASSERT_TRUE(base::PathExists(install_dir_.AppendASCII(kExecutableName)));
  ASSERT_FALSE(alt_install_dir_ && base::PathExists(*alt_install_dir_));

  int64_t exe_size = 0;
  ASSERT_TRUE(
      base::GetFileSize(install_dir_.AppendASCII(kExecutableName), &exe_size));
  EXPECT_GT(exe_size, 0);

  ExpectUpdaterRegistration();
}

TEST_F(InstallerTest, OverinstallDifferentArch) {
  if (!alt_install_dir_) {
    LOG(WARNING) << "OverinstallDifferentArch not implemented for x86 hosts.";
    return;
  }

  SetUpdaterRegistration(L"0.0.0.1", L"Prehistoric Enterprise Companion");
  ASSERT_TRUE(base::CreateDirectory(*alt_install_dir_));
  ASSERT_TRUE(
      base::WriteFile(alt_install_dir_->AppendASCII(kExecutableName), ""));

  RunInstaller(true);

  ASSERT_TRUE(base::PathExists(install_dir_.AppendASCII(kExecutableName)));
  ASSERT_FALSE(alt_install_dir_ && base::PathExists(*alt_install_dir_));

  EXPECT_FALSE(base::PathExists(*alt_install_dir_));

  ExpectUpdaterRegistration();
}

}  // namespace enterprise_companion
