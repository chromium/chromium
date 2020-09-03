// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/path_service.h"
#include "base/process/launch.h"
#include "base/process/process.h"
#include "base/strings/utf_string_conversions.h"
#include "base/synchronization/waitable_event.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/win/registry.h"
#include "chrome/updater/constants.h"
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

bool Run(base::CommandLine command_line, int* exit_code) {
  auto process = base::LaunchProcess(command_line, {});
  if (!process.IsValid())
    return false;
  if (!process.WaitForExitWithTimeout(base::TimeDelta::FromSeconds(60),
                                      exit_code))
    return false;
  base::WaitableEvent sleep(base::WaitableEvent::ResetPolicy::MANUAL,
                            base::WaitableEvent::InitialState::NOT_SIGNALED);
  // The process will exit before it is done uninstalling: sleep for five
  // seconds to allow uninstall to complete.
  base::ThreadPool::PostDelayedTask(
      FROM_HERE, {base::MayBlock()},
      base::BindOnce(&base::WaitableEvent::Signal, base::Unretained(&sleep)),
      base::TimeDelta::FromSeconds(5));
  sleep.Wait();
  return true;
}

base::FilePath GetProductPath() {
  base::FilePath app_data_dir;
  if (!base::PathService::Get(base::DIR_LOCAL_APP_DATA, &app_data_dir))
    return base::FilePath();
  return app_data_dir.AppendASCII(COMPANY_SHORTNAME_STRING)
      .AppendASCII(PRODUCT_FULLNAME_STRING)
      .AppendASCII(UPDATER_VERSION_STRING);
}

base::FilePath GetExecutablePath() {
  return GetProductPath().AppendASCII("updater.exe");
}

base::FilePath GetDataDirPath() {
  base::FilePath app_data_dir;
  if (!base::PathService::Get(base::DIR_LOCAL_APP_DATA, &app_data_dir))
    return base::FilePath();
  return app_data_dir.AppendASCII(COMPANY_SHORTNAME_STRING)
      .AppendASCII(PRODUCT_FULLNAME_STRING);
}

}  // namespace

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
                           static_cast<DWORD>(0)),
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

void ExpectActive() {
  // TODO(crbug.com/1062288): Assert that COM interfaces point to this version.

  // Files must exist on the file system.
  EXPECT_TRUE(base::PathExists(GetProductPath()));
}

void RunWake(int expected_exit_code) {
  const base::FilePath path = GetExecutablePath();
  ASSERT_FALSE(path.empty());
  base::CommandLine command_line(path);
  command_line.AppendSwitch(kWakeSwitch);
  int exit_code = -1;
  ASSERT_TRUE(Run(command_line, &exit_code));
  EXPECT_EQ(exit_code, expected_exit_code);
}

void Install() {
  int exit_code = -1;
  ASSERT_TRUE(Run(base::CommandLine(GetInstallerPath()), &exit_code));
  EXPECT_EQ(0, exit_code);
}

void Uninstall() {
  base::FilePath path = GetExecutablePath();
  ASSERT_FALSE(path.empty());
  base::CommandLine command_line(path);
  command_line.AppendSwitch("uninstall");
  int exit_code = -1;
  ASSERT_TRUE(Run(command_line, &exit_code));
  EXPECT_EQ(0, exit_code);
}

}  // namespace test

}  // namespace updater
