// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_UPDATER_TEST_UNIT_TEST_UTIL_WIN_H_
#define CHROME_UPDATER_TEST_UNIT_TEST_UTIL_WIN_H_

#include <string>

#include "base/command_line.h"
#include "base/files/scoped_temp_dir.h"
#include "base/win/atl.h"
#include "base/win/registry.h"
#include "chrome/updater/updater_scope.h"
#include "chrome/updater/test/unit_test_util.h"

namespace updater::test {

// Creates the key `{HKLM\HKCU}\Software\{CompanyName}\Update\Clients\{app_id}`.
// `{HKLM\HKCU}` is determined by `scope`.
base::win::RegKey CreateAppClientKey(UpdaterScope scope,
                                     const std::wstring& app_id);

// Deletes the key `{HKLM\HKCU}\Software\{CompanyName}\Update\Clients\{app_id}`.
// `{HKLM\HKCU}` is determined by `scope`.
void DeleteAppClientKey(UpdaterScope scope, const std::wstring& app_id);

// Creates `{HKRoot}\Software\{CompanyName}\Update\ClientState\{app_id}`.
base::win::RegKey CreateAppClientStateKey(UpdaterScope scope,
                                          const std::wstring& app_id);

// Deletes `{HKRoot}\Software\{CompanyName}\Update\ClientState\{app_id}`.
void DeleteAppClientStateKey(UpdaterScope scope, const std::wstring& app_id);

// Creates the key
// `HKLM\Software\{CompanyName}\Update\Clients\{app_id}`,
// and adds the provided REG_SZ entries under it.
void CreateLaunchCmdElevatedRegistry(const std::wstring& app_id,
                                     const std::wstring& name,
                                     const std::wstring& pv,
                                     const std::wstring& command_id,
                                     const std::wstring& command_line);

// Creates the key
// `{HKRoot}\Software\{CompanyName}\Update\Clients\{app_id}\Commands\{cmd_id}`,
// and adds a `CommandLine` REG_SZ entry with the value `cmd_line`. `{HKRoot}`
// is determined by `scope`.
void CreateAppCommandRegistry(UpdaterScope scope,
                              const std::wstring& app_id,
                              const std::wstring& cmd_id,
                              const std::wstring& cmd_line);

// Similar to `CreateAppCommandRegistry`, and then marks the AppCommand to run
// on OS upgrades.
void CreateAppCommandOSUpgradeRegistry(UpdaterScope scope,
                                       const std::wstring& app_id,
                                       const std::wstring& cmd_id,
                                       const std::wstring& cmd_line);

// Returns the path to "cmd.exe" in `cmd_exe_command_line` based on the current
// test scope:
// * "%systemroot%\system32\cmd.exe" for user `scope`.
// * "%programfiles%\`temp_parent_dir`\cmd.exe" for system `scope`.
// `temp_parent_dir` is owned by the caller.
void SetupCmdExe(UpdaterScope scope,
                 base::CommandLine& cmd_exe_command_line,
                 base::ScopedTempDir& temp_parent_dir);

// Creates a service for test purposes.
[[nodiscard]] bool CreateService(const std::wstring& service_name,
                                 const std::wstring& display_name,
                                 const std::wstring& command_line);

// Creates an event accessible to all authenticated users on the machine.
test::EventHolder CreateEveryoneWaitableEventForTest();

int RunVPythonCommand(const base::CommandLine& command_line);

}  // namespace updater::test

#endif  // CHROME_UPDATER_TEST_UNIT_TEST_UTIL_WIN_H_
