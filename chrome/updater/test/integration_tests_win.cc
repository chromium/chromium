// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <wrl/client.h>
#include <string>

#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/path_service.h"
#include "base/strings/strcat.h"
#include "base/strings/string16.h"
#include "base/strings/utf_string_conversions.h"
#include "base/synchronization/waitable_event.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/version.h"
#include "base/win/registry.h"
#include "chrome/updater/app/server/win/updater_idl.h"
#include "chrome/updater/app/server/win/updater_internal_idl.h"
#include "chrome/updater/app/server/win/updater_legacy_idl.h"
#include "chrome/updater/constants.h"
#include "chrome/updater/test/integration_tests.h"
#include "chrome/updater/updater_branding.h"
#include "chrome/updater/updater_version.h"
#include "chrome/updater/util.h"
#include "chrome/updater/win/constants.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace updater {
namespace test {
namespace {

constexpr base::char16 kDidRun[] = L"dr";

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

base::string16 GetAppClientStateKey(const std::string& id) {
  return base::ASCIIToUTF16(base::StrCat({CLIENT_STATE_KEY, id}));
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

void EnterTestMode(const GURL& url) {
  base::win::RegKey key(HKEY_CURRENT_USER, L"", KEY_SET_VALUE);
  ASSERT_EQ(key.Create(HKEY_CURRENT_USER, UPDATE_DEV_KEY, KEY_WRITE),
            ERROR_SUCCESS);
  ASSERT_EQ(key.WriteValue(base::UTF8ToUTF16(kDevOverrideKeyUrl).c_str(),
                           base::UTF8ToUTF16(url.spec()).c_str()),
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
  if (::testing::Test::HasFailure())
    PrintLog();
  // Copy logs from GetDataDirPath() before updater uninstalls itself
  // and deletes the path.
  CopyLog(GetDataDirPath());

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

void SetActive(const std::string& id) {
  // TODO(crbug/1159498): Standardize registry access.
  base::win::RegKey key;
  ASSERT_EQ(key.Open(HKEY_CURRENT_USER, GetAppClientStateKey(id).c_str(),
                     KEY_WRITE | KEY_WOW64_32KEY),
            ERROR_SUCCESS);
  EXPECT_EQ(key.WriteValue(kDidRun, L"1"), ERROR_SUCCESS);
}

void ExpectActive(const std::string& id) {
  // TODO(crbug/1159498): Standardize registry access.
  base::win::RegKey key;
  ASSERT_EQ(key.Open(HKEY_CURRENT_USER, GetAppClientStateKey(id).c_str(),
                     KEY_READ | KEY_WOW64_32KEY),
            ERROR_SUCCESS);
  base::string16 value;
  ASSERT_EQ(key.ReadValue(kDidRun, &value), ERROR_SUCCESS);
  EXPECT_EQ(value, L"1");
}

void ExpectNotActive(const std::string& id) {
  // TODO(crbug/1159498): Standardize registry access.
  base::win::RegKey key;
  if (key.Open(HKEY_CURRENT_USER, GetAppClientStateKey(id).c_str(),
               KEY_READ | KEY_WOW64_32KEY) == ERROR_SUCCESS) {
    base::string16 value;
    if (key.ReadValue(kDidRun, &value) == ERROR_SUCCESS)
      EXPECT_EQ(value, L"0");
  }
}

// Tests if the typelibs and some of the public, internal, and
// legacy interfaces are available. Failure to query these interfaces indicates
// an issue with typelib registration.
void ExpectInterfacesRegistered() {
  // IUpdater.
  Microsoft::WRL::ComPtr<IUnknown> updater_server;
  EXPECT_HRESULT_SUCCEEDED(::CoCreateInstance(__uuidof(UpdaterClass), nullptr,
                                              CLSCTX_LOCAL_SERVER,
                                              IID_PPV_ARGS(&updater_server)));
  Microsoft::WRL::ComPtr<IUpdater> updater;
  EXPECT_HRESULT_SUCCEEDED(updater_server.As(&updater));

  // IUpdaterInternal.
  Microsoft::WRL::ComPtr<IUnknown> updater_internal_server;
  EXPECT_HRESULT_SUCCEEDED(::CoCreateInstance(
      __uuidof(UpdaterInternalClass), nullptr, CLSCTX_LOCAL_SERVER,
      IID_PPV_ARGS(&updater_internal_server)));
  Microsoft::WRL::ComPtr<IUpdaterInternal> updater_internal;
  EXPECT_HRESULT_SUCCEEDED(updater_internal_server.As(&updater_internal));

  // IGoogleUpdate3Web and IAppBundleWeb.
  Microsoft::WRL::ComPtr<IUnknown> updater_legacy_server;
  EXPECT_HRESULT_SUCCEEDED(::CoCreateInstance(
      __uuidof(GoogleUpdate3WebUserClass), nullptr, CLSCTX_LOCAL_SERVER,
      IID_PPV_ARGS(&updater_legacy_server)));
  Microsoft::WRL::ComPtr<IGoogleUpdate3Web> google_update;
  EXPECT_HRESULT_SUCCEEDED(updater_legacy_server.As(&google_update));
  Microsoft::WRL::ComPtr<IAppBundleWeb> app_bundle;
  Microsoft::WRL::ComPtr<IDispatch> dispatch;
  EXPECT_HRESULT_SUCCEEDED(google_update->createAppBundleWeb(&dispatch));
  EXPECT_HRESULT_SUCCEEDED(dispatch.As(&app_bundle));
}

}  // namespace test
}  // namespace updater
