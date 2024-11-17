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
  // The install directory.
  base::FilePath install_dir_;

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
  ExpectUpdaterRegistration();
}

TEST_F(InstallerTest, Overinstall) {
  SetUpdaterRegistration(L"0.0.0.1", L"Prehistoric Enterprise Companion");
  ASSERT_TRUE(base::CreateDirectory(install_dir_));
  ASSERT_TRUE(base::WriteFile(install_dir_.AppendASCII(kExecutableName), ""));

  RunInstaller(true);

  ASSERT_TRUE(base::PathExists(install_dir_.AppendASCII(kExecutableName)));
  std::optional<int64_t> exe_size =
      base::GetFileSize(install_dir_.AppendASCII(kExecutableName));
  ASSERT_TRUE(exe_size.has_value());
  EXPECT_GT(exe_size.value(), 0);

  ExpectUpdaterRegistration();
}

// Uses `DIR_PROGRAM_FILES6432` as the fake installation directory. Fake because
// the production install always goes into `DIR_PROGRAM_FILESX86`.
TEST_F(InstallerTest, OverinstallFakeLocationDifferentArch) {
  base::FilePath program_files_dir;
  ASSERT_TRUE(
      base::PathService::Get(base::DIR_PROGRAM_FILES6432, &program_files_dir));
  base::FilePath program_files_x86_dir;
  ASSERT_TRUE(base::PathService::Get(base::DIR_PROGRAM_FILESX86,
                                     &program_files_x86_dir));
  if (program_files_dir == program_files_x86_dir) {
    GTEST_SKIP() << "Test not applicable on x86 hosts.";
  }
  const base::FilePath incorrect_install_dir =
      program_files_dir.AppendASCII(COMPANY_SHORTNAME_STRING)
          .AppendASCII(PRODUCT_FULLNAME_STRING);
  SetUpdaterRegistration(L"0.0.0.1", L"Fake Enterprise Companion");
  ASSERT_TRUE(base::CreateDirectory(incorrect_install_dir));
  ASSERT_TRUE(
      base::WriteFile(incorrect_install_dir.AppendASCII(kExecutableName), ""));

  RunInstaller(true);

  ASSERT_TRUE(base::PathExists(install_dir_.AppendASCII(kExecutableName)));
  ASSERT_TRUE(base::PathExists(incorrect_install_dir));

  ASSERT_TRUE(base::DeletePathRecursively(incorrect_install_dir));

  ExpectUpdaterRegistration();
}

}  // namespace enterprise_companion
