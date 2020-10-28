// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/path_service.h"
#include "base/strings/utf_string_conversions.h"
#include "base/synchronization/waitable_event.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/version.h"
#include "base/win/registry.h"
#include "chrome/updater/constants.h"
#include "chrome/updater/test/integration_tests.h"
#include "chrome/updater/updater_version.h"
#include "chrome/updater/util.h"
#include "chrome/updater/win/constants.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace updater {
namespace test {
namespace {

base::FilePath GetInstallerPath() {
  base::FilePath test_executable;
  if (!base::PathService::Get(base::FILE_EXE, &test_executable))
    return base::FilePath();
  return test_executable.DirName().AppendASCII("UpdaterSetup.exe");
}

base::FilePath GetProductPath() {
  base::FilePath app_data_dir;
  if (!base::PathService::Get(base::DIR_LOCAL_APP_DATA, &app_data_dir))
    return base::FilePath();
  return app_data_dir.AppendASCII(COMPANY_SHORTNAME_STRING)
      .AppendASCII(PRODUCT_FULLNAME_STRING)
      .AppendASCII(UPDATER_VERSION_STRING);
}

}  // namespace

base::FilePath GetInstalledExecutablePath() {
  return GetProductPath().AppendASCII("updater.exe");
}

base::FilePath GetFakeUpdaterInstallFolderPath(const base::Version& version) {
  return GetProductPath().AppendASCII(version.GetString());
}

base::FilePath GetDataDirPath() {
  base::FilePath app_data_dir;
  if (!base::PathService::Get(base::DIR_LOCAL_APP_DATA, &app_data_dir))
    return base::FilePath();
  return app_data_dir.AppendASCII(COMPANY_SHORTNAME_STRING)
      .AppendASCII(PRODUCT_FULLNAME_STRING);
}

void Clean() {
  // TODO(crbug.com/1062288): Delete the Client / ClientState registry keys.
  base::win::RegKey(HKEY_CURRENT_USER, L"", KEY_SET_VALUE)
      .DeleteKey(UPDATE_DEV_KEY);
  // TODO(crbug.com/1062288): Delete the COM server items.
  // TODO(crbug.com/1062288): Delete the COM service items.
  // TODO(crbug.com/1062288): Delete the COM interfaces.
  // TODO(crbug.com/1062288): Delete the Wake task.
  EXPECT_TRUE(base::DeletePathRecursively(GetProductPath()));
  EXPECT_TRUE(base::DeletePathRecursively(GetDataDirPath()));
}

void ExpectClean() {
  // TODO(crbug.com/1062288): Assert there are no Client / ClientState registry
  // keys.
  // TODO(crbug.com/1062288): Assert there is no UpdateDev registry key.
  // TODO(crbug.com/1062288): Assert there are no COM server items.
  // TODO(crbug.com/1062288): Assert there are no COM service items.
  // TODO(crbug.com/1062288): Assert there are no COM interfaces.
  // TODO(crbug.com/1062288): Assert there are no Wake tasks.

  // Files must not exist on the file system.
  EXPECT_FALSE(base::PathExists(GetProductPath()));
  EXPECT_FALSE(base::PathExists(GetDataDirPath()));
}

void EnterTestMode() {
  // TODO(crbug.com/1119857): Point this to an actual fake server.
  base::win::RegKey key(HKEY_CURRENT_USER, L"", KEY_SET_VALUE);
  ASSERT_EQ(key.Create(HKEY_CURRENT_USER, UPDATE_DEV_KEY, KEY_WRITE),
            ERROR_SUCCESS);
  ASSERT_EQ(key.WriteValue(base::UTF8ToUTF16(kDevOverrideKeyUrl).c_str(),
                           L"http://localhost:8367"),
            ERROR_SUCCESS);
  ASSERT_EQ(key.WriteValue(base::UTF8ToUTF16(kDevOverrideKeyUseCUP).c_str(),
                           DWORD{0}),
            ERROR_SUCCESS);
}

void ExpectInstalled() {
  // TODO(crbug.com/1062288): Assert there are Client / ClientState registry
  // keys.
  // TODO(crbug.com/1062288): Assert there are COM server items.
  // TODO(crbug.com/1062288): Assert there are COM service items. (Maybe.)
  // TODO(crbug.com/1062288): Assert there are COM interfaces.
  // TODO(crbug.com/1062288): Assert there are Wake tasks.

  // Files must exist on the file system.
  EXPECT_TRUE(base::PathExists(GetProductPath()));
}

void ExpectCandidateUninstalled() {
  // TODO(crbug.com/1062288): Assert there are no side-by-side COM interfaces.
  // TODO(crbug.com/1062288): Assert there are no Wake tasks.

  // Files must not exist on the file system.
  EXPECT_FALSE(base::PathExists(GetProductPath()));
}

void ExpectActive() {
  // TODO(crbug.com/1062288): Assert that COM interfaces point to this version.

  // Files must exist on the file system.
  EXPECT_TRUE(base::PathExists(GetProductPath()));
}

void Install() {
  const base::FilePath path = GetInstallerPath();
  ASSERT_FALSE(path.empty());
  base::CommandLine command_line(path);
  command_line.AppendSwitch(kInstallSwitch);
  int exit_code = -1;
  ASSERT_TRUE(Run(command_line, &exit_code));
  EXPECT_EQ(0, exit_code);
}

void Uninstall() {
  // Note: updater.exe --uninstall is run from the build dir, not the install
  // dir, because it is useful for tests to be able to run it to clean the
  // system even if installation has failed or the installed binaries have
  // already been removed.
  base::FilePath path = GetInstallerPath().DirName().AppendASCII("updater.exe");
  ASSERT_FALSE(path.empty());
  base::CommandLine command_line(path);
  command_line.AppendSwitch("uninstall");
  int exit_code = -1;
  ASSERT_TRUE(Run(command_line, &exit_code));
  EXPECT_EQ(0, exit_code);

  // Uninstallation involves a race with the uninstall.cmd script and the
  // process exit. Sleep to allow the script to complete its work.
  SleepFor(5);
}

}  // namespace test
}  // namespace updater
