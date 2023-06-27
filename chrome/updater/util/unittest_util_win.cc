// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/updater/util/unittest_util_win.h"

#include <windows.h>

#include <string>
#include <utility>

#include "base/base_paths_win.h"
#include "base/command_line.h"
#include "base/files/file_enumerator.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/path_service.h"
#include "base/strings/strcat.h"
#include "base/win/registry.h"
#include "chrome/updater/test_scope.h"
#include "chrome/updater/updater_scope.h"
#include "chrome/updater/util/unittest_util.h"
#include "chrome/updater/util/win_util.h"
#include "chrome/updater/win/test/test_executables.h"
#include "chrome/updater/win/win_constants.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace updater {

namespace {

void CreateAppCommandRegistryHelper(UpdaterScope scope,
                                    const std::wstring& app_id,
                                    const std::wstring& cmd_id,
                                    const std::wstring& cmd_line,
                                    bool auto_run_on_os_upgrade) {
  CreateAppClientKey(scope, app_id);
  base::win::RegKey command_key;
  EXPECT_EQ(command_key.Create(UpdaterScopeToHKeyRoot(scope),
                               GetAppCommandKey(app_id, cmd_id).c_str(),
                               Wow6432(KEY_WRITE)),
            ERROR_SUCCESS);
  EXPECT_EQ(command_key.WriteValue(kRegValueCommandLine, cmd_line.c_str()),
            ERROR_SUCCESS);
  if (auto_run_on_os_upgrade) {
    EXPECT_EQ(command_key.WriteValue(kRegValueAutoRunOnOSUpgrade, 1U),
              ERROR_SUCCESS);
  }
}

}  // namespace

base::win::RegKey CreateAppClientKey(UpdaterScope scope,
                                     const std::wstring& app_id) {
  base::win::RegKey client_key;
  EXPECT_EQ(
      client_key.Create(UpdaterScopeToHKeyRoot(scope),
                        GetAppClientsKey(app_id).c_str(), Wow6432(KEY_WRITE)),
      ERROR_SUCCESS);
  return client_key;
}

void DeleteAppClientKey(UpdaterScope scope, const std::wstring& app_id) {
  base::win::RegKey(UpdaterScopeToHKeyRoot(scope), L"", Wow6432(DELETE))
      .DeleteKey(GetAppClientsKey(app_id).c_str());
}

base::win::RegKey CreateAppClientStateKey(UpdaterScope scope,
                                          const std::wstring& app_id) {
  base::win::RegKey client_state_key;
  EXPECT_EQ(client_state_key.Create(UpdaterScopeToHKeyRoot(scope),
                                    GetAppClientStateKey(app_id).c_str(),
                                    Wow6432(KEY_WRITE)),
            ERROR_SUCCESS);
  return client_state_key;
}

void DeleteAppClientStateKey(UpdaterScope scope, const std::wstring& app_id) {
  base::win::RegKey(UpdaterScopeToHKeyRoot(scope), L"", Wow6432(DELETE))
      .DeleteKey(GetAppClientStateKey(app_id).c_str());
}

void CreateLaunchCmdElevatedRegistry(const std::wstring& app_id,
                                     const std::wstring& name,
                                     const std::wstring& pv,
                                     const std::wstring& command_id,
                                     const std::wstring& command_line) {
  base::win::RegKey client_key =
      CreateAppClientKey(UpdaterScope::kSystem, app_id);
  EXPECT_EQ(client_key.WriteValue(kRegValueName, name.c_str()), ERROR_SUCCESS);
  EXPECT_EQ(client_key.WriteValue(kRegValuePV, pv.c_str()), ERROR_SUCCESS);
  EXPECT_EQ(client_key.WriteValue(command_id.c_str(), command_line.c_str()),
            ERROR_SUCCESS);
}

void CreateAppCommandRegistry(UpdaterScope scope,
                              const std::wstring& app_id,
                              const std::wstring& cmd_id,
                              const std::wstring& cmd_line) {
  CreateAppCommandRegistryHelper(scope, app_id, cmd_id, cmd_line, false);
}

void CreateAppCommandOSUpgradeRegistry(UpdaterScope scope,
                                       const std::wstring& app_id,
                                       const std::wstring& cmd_id,
                                       const std::wstring& cmd_line) {
  CreateAppCommandRegistryHelper(scope, app_id, cmd_id, cmd_line, true);
}

void SetupCmdExe(UpdaterScope scope,
                 base::CommandLine& cmd_exe_command_line,
                 base::ScopedTempDir& temp_programfiles_dir) {
  constexpr wchar_t kCmdExe[] = L"cmd.exe";

  base::FilePath system_path;
  ASSERT_TRUE(base::PathService::Get(base::DIR_SYSTEM, &system_path));

  const base::FilePath cmd_exe_system_path = system_path.Append(kCmdExe);
  if (!IsSystemInstall(scope)) {
    cmd_exe_command_line = base::CommandLine(cmd_exe_system_path);
    return;
  }

  base::FilePath programfiles_path;
  ASSERT_TRUE(
      base::PathService::Get(base::DIR_PROGRAM_FILES, &programfiles_path));
  ASSERT_TRUE(
      temp_programfiles_dir.CreateUniqueTempDirUnderPath(programfiles_path));
  base::FilePath cmd_exe_path;
  cmd_exe_path = temp_programfiles_dir.GetPath().Append(kCmdExe);

  ASSERT_TRUE(base::CopyFile(cmd_exe_system_path, cmd_exe_path));
  cmd_exe_command_line = base::CommandLine(cmd_exe_path);
}

[[nodiscard]] bool CreateService(const std::wstring& service_name,
                                 const std::wstring& display_name,
                                 const std::wstring& command_line) {
  ScopedScHandle scm(::OpenSCManager(
      nullptr, nullptr, SC_MANAGER_CONNECT | SC_MANAGER_CREATE_SERVICE));
  if (!scm.IsValid()) {
    return false;
  }

  ScopedScHandle service(::CreateService(
      scm.Get(), service_name.c_str(), display_name.c_str(),
      DELETE | SERVICE_QUERY_CONFIG | SERVICE_CHANGE_CONFIG,
      SERVICE_WIN32_OWN_PROCESS, SERVICE_DEMAND_START, SERVICE_ERROR_NORMAL,
      command_line.c_str(), nullptr, nullptr, nullptr, nullptr, nullptr));
  return service.IsValid() || ::GetLastError() == ERROR_SERVICE_EXISTS;
}

void SetupMockUpdater(const base::FilePath& mock_updater_path) {
  const base::FilePath exe_dir(mock_updater_path.DirName());

  const base::FilePath test_exe(
      GetTestProcessCommandLine(GetTestScope(), test::GetTestName())  // IN-TEST
          .GetProgram());

  for (const base::FilePath& dir :
       {exe_dir, exe_dir.Append(L"1.2.3.4"), exe_dir.Append(L"Download"),
        exe_dir.Append(L"Install")}) {
    ASSERT_TRUE(base::CreateDirectory(dir));

    for (const std::wstring& exe_name :
         {mock_updater_path.BaseName().value(), {L"mock.exe"}}) {
      const base::FilePath exe(dir.Append(exe_name));
      ASSERT_TRUE(base::CopyFile(test_exe, exe));
    }
  }
}

void ExpectOnlyMockUpdater(const base::FilePath& mock_updater_path) {
  const base::FilePath exe_dir(mock_updater_path.DirName());
  ASSERT_TRUE(base::PathExists(mock_updater_path));
  int count_mock_updater_path = 0;

  ForEachItemInPath(
      exe_dir, false,
      base::FileEnumerator::FILES | base::FileEnumerator::DIRECTORIES,
      base::BindRepeating(
          [](const base::FilePath& mock_updater_path,
             int& count_mock_updater_path, const base::FilePath& item) {
            if (item == mock_updater_path) {
              ++count_mock_updater_path;
            } else {
              ADD_FAILURE() << "Unexpected file/directory found: " << item;
            }
          },
          mock_updater_path, std::ref(count_mock_updater_path)));

  EXPECT_EQ(count_mock_updater_path, 1);
}

}  // namespace updater
