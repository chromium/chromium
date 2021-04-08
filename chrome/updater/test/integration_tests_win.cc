// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <wrl/client.h>

#include <string>
#include <vector>

#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/optional.h"
#include "base/path_service.h"
#include "base/strings/strcat.h"
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
#include "chrome/updater/external_constants_builder.h"
#include "chrome/updater/test/integration_tests_impl.h"
#include "chrome/updater/test/test_app/constants.h"
#include "chrome/updater/test/test_app/test_app_version.h"
#include "chrome/updater/updater_branding.h"
#include "chrome/updater/updater_scope.h"
#include "chrome/updater/updater_version.h"
#include "chrome/updater/util.h"
#include "chrome/updater/win/constants.h"
#include "chrome/updater/win/setup/setup_util.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace updater {
namespace test {
namespace {

constexpr wchar_t kDidRun[] = L"dr";

base::FilePath GetInstallerPath() {
  base::FilePath test_executable;
  if (!base::PathService::Get(base::FILE_EXE, &test_executable))
    return base::FilePath();
  return test_executable.DirName().AppendASCII("UpdaterSetup.exe");
}

base::FilePath GetTestAppExecutablePath() {
  base::FilePath test_executable;
  if (!base::PathService::Get(base::FILE_EXE, &test_executable))
    return base::FilePath();
  return test_executable.DirName().AppendASCII(TEST_APP_FULLNAME_STRING ".exe");
}

base::Optional<base::FilePath> GetProductPath() {
  base::FilePath app_data_dir;
  if (!base::PathService::Get(base::DIR_LOCAL_APP_DATA, &app_data_dir))
    return base::nullopt;
  return app_data_dir.AppendASCII(COMPANY_SHORTNAME_STRING)
      .AppendASCII(PRODUCT_FULLNAME_STRING)
      .AppendASCII(UPDATER_VERSION_STRING);
}

std::wstring GetAppClientStateKey(const std::string& id) {
  return base::ASCIIToWide(base::StrCat({CLIENT_STATE_KEY, id}));
}

bool RegKeyExists(HKEY root, REGSAM regsam, const std::wstring& path) {
  return base::win::RegKey(root, path.c_str(), KEY_QUERY_VALUE | regsam)
      .Valid();
}

bool DeleteRegKey(HKEY root, REGSAM regsam, const std::wstring& path) {
  LONG result =
      base::win::RegKey(root, L"", regsam | KEY_READ).DeleteKey(path.c_str());
  return result == ERROR_SUCCESS || result == ERROR_FILE_NOT_FOUND;
}

}  // namespace

base::Optional<base::FilePath> GetInstalledExecutablePath(UpdaterScope scope) {
  base::Optional<base::FilePath> path = GetProductPath();
  if (!path)
    return base::nullopt;
  return path->AppendASCII("updater.exe");
}

base::Optional<base::FilePath> GetFakeUpdaterInstallFolderPath(
    UpdaterScope scope,
    const base::Version& version) {
  base::Optional<base::FilePath> path = GetProductPath();
  if (!path)
    return base::nullopt;
  return path->AppendASCII(version.GetString());
}

base::Optional<base::FilePath> GetDataDirPath(UpdaterScope scope) {
  base::FilePath app_data_dir;
  if (!base::PathService::Get(base::DIR_LOCAL_APP_DATA, &app_data_dir))
    return base::nullopt;
  return app_data_dir.AppendASCII(COMPANY_SHORTNAME_STRING)
      .AppendASCII(PRODUCT_FULLNAME_STRING);
}

void Clean(UpdaterScope scope) {
  // TODO(crbug.com/1096654): Add support for system scope.
  const HKEY root = HKEY_CURRENT_USER;
  for (const char* key : {CLIENT_STATE_KEY, CLIENTS_KEY, UPDATER_KEY}) {
    EXPECT_TRUE(DeleteRegKey(root, KEY_WOW64_32KEY, base::ASCIIToWide(key)));
  }
  for (const wchar_t* key : {COMPANY_POLICIES_KEY, UPDATER_POLICIES_KEY}) {
    EXPECT_TRUE(DeleteRegKey(HKEY_LOCAL_MACHINE, 0, key));
  }
  for (const CLSID& clsid : GetSideBySideServers()) {
    EXPECT_TRUE(DeleteRegKey(root, 0, GetComServerClsidRegistryPath(clsid)));
  }
  for (const CLSID& clsid : GetActiveServers()) {
    EXPECT_TRUE(DeleteRegKey(root, 0, GetComServerClsidRegistryPath(clsid)));
  }
  for (const GUID& guid : GetSideBySideInterfaces()) {
    EXPECT_TRUE(DeleteRegKey(root, 0, GetComIidRegistryPath(guid)));
    EXPECT_TRUE(DeleteRegKey(root, 0, GetComTypeLibRegistryPath(guid)));
  }
  for (const GUID& guid : GetActiveInterfaces()) {
    EXPECT_TRUE(DeleteRegKey(root, 0, GetComIidRegistryPath(guid)));
    EXPECT_TRUE(DeleteRegKey(root, 0, GetComTypeLibRegistryPath(guid)));
  }
  // TODO(crbug.com/1062288): Delete the COM service items.
  // TODO(crbug.com/1062288): Delete the Wake task.
  base::Optional<base::FilePath> path = GetProductPath();
  EXPECT_TRUE(path);
  if (path)
    EXPECT_TRUE(base::DeletePathRecursively(*path));
  path = GetDataDirPath(scope);
  EXPECT_TRUE(path);
  if (path)
    EXPECT_TRUE(base::DeletePathRecursively(*path));
}

void ExpectClean(UpdaterScope scope) {
  // TODO(crbug.com/1096654): Add support for system scope.
  const HKEY root = HKEY_CURRENT_USER;
  for (const char* key : {CLIENT_STATE_KEY, CLIENTS_KEY, UPDATER_KEY}) {
    EXPECT_FALSE(RegKeyExists(root, KEY_WOW64_32KEY, base::ASCIIToWide(key)));
  }
  for (const wchar_t* key : {COMPANY_POLICIES_KEY, UPDATER_POLICIES_KEY}) {
    EXPECT_FALSE(RegKeyExists(HKEY_LOCAL_MACHINE, 0, key));
  }
  for (const CLSID& clsid : GetSideBySideServers()) {
    EXPECT_FALSE(RegKeyExists(root, 0, GetComServerClsidRegistryPath(clsid)));
  }
  for (const CLSID& clsid : GetActiveServers()) {
    EXPECT_FALSE(RegKeyExists(root, 0, GetComServerClsidRegistryPath(clsid)));
  }
  for (const GUID& guid : GetSideBySideInterfaces()) {
    EXPECT_FALSE(RegKeyExists(root, 0, GetComIidRegistryPath(guid)));
    EXPECT_FALSE(RegKeyExists(root, 0, GetComTypeLibRegistryPath(guid)));
  }
  for (const GUID& guid : GetActiveInterfaces()) {
    EXPECT_FALSE(RegKeyExists(root, 0, GetComIidRegistryPath(guid)));
    EXPECT_FALSE(RegKeyExists(root, 0, GetComTypeLibRegistryPath(guid)));
  }
  // TODO(crbug.com/1062288): Assert there are no COM service items.
  // TODO(crbug.com/1062288): Assert there are no Wake tasks.

  // Files must not exist on the file system.
  base::Optional<base::FilePath> path = GetProductPath();
  EXPECT_TRUE(path);
  if (path)
    EXPECT_FALSE(base::PathExists(*path));
  path = GetDataDirPath(scope);
  EXPECT_TRUE(path);
  if (path)
    EXPECT_FALSE(base::PathExists(*path));
}

void EnterTestMode(const GURL& url) {
  ASSERT_TRUE(ExternalConstantsBuilder()
                  .SetUpdateURL(std::vector<std::string>{url.spec()})
                  .SetUseCUP(false)
                  .SetInitialDelay(0.1)
                  .Overwrite());
}

void ExpectInstalled(UpdaterScope scope) {
  // TODO(crbug.com/1062288): Assert there are Client / ClientState registry
  // keys.
  // TODO(crbug.com/1062288): Assert there are COM server items.
  // TODO(crbug.com/1062288): Assert there are COM service items. (Maybe.)
  // TODO(crbug.com/1062288): Assert there are COM interfaces.
  // TODO(crbug.com/1062288): Assert there are Wake tasks.

  // Files must exist on the file system.
  base::Optional<base::FilePath> path = GetProductPath();
  EXPECT_TRUE(path);
  if (path)
    EXPECT_TRUE(base::PathExists(*path));
}

void ExpectCandidateUninstalled(UpdaterScope scope) {
  // TODO(crbug.com/1062288): Assert there are no side-by-side COM interfaces.
  // TODO(crbug.com/1062288): Assert there are no Wake tasks.

  // Files must not exist on the file system.
  base::Optional<base::FilePath> path = GetProductPath();
  EXPECT_TRUE(path);
  if (path)
    EXPECT_FALSE(base::PathExists(*path));
}

void ExpectActiveUpdater(UpdaterScope scope) {
  // TODO(crbug.com/1062288): Assert that COM interfaces point to this version.

  // Files must exist on the file system.
  base::Optional<base::FilePath> path = GetProductPath();
  EXPECT_TRUE(path);
  if (path)
    EXPECT_TRUE(base::PathExists(*path));
}

void RegisterTestApp(UpdaterScope scope) {
  const base::FilePath path = GetTestAppExecutablePath();
  ASSERT_FALSE(path.empty());
  base::CommandLine command_line(path);
  command_line.AppendSwitch(kRegisterUpdaterSwitch);
  int exit_code = -1;
  ASSERT_TRUE(Run(scope, command_line, &exit_code));
  EXPECT_EQ(exit_code, 0);
}

void Install(UpdaterScope scope) {
  const base::FilePath path = GetInstallerPath();
  ASSERT_FALSE(path.empty());
  base::CommandLine command_line(path);
  command_line.AppendSwitch(kInstallSwitch);
  int exit_code = -1;
  ASSERT_TRUE(Run(scope, command_line, &exit_code));
  EXPECT_EQ(0, exit_code);
}

void Uninstall(UpdaterScope scope) {
  if (::testing::Test::HasFailure())
    PrintLog(scope);
  // Copy logs from GetDataDirPath() before updater uninstalls itself
  // and deletes the path.
  base::Optional<base::FilePath> data_dir_path = GetDataDirPath(scope);
  EXPECT_TRUE(data_dir_path);
  if (data_dir_path)
    CopyLog(*data_dir_path);

  // Note: updater.exe --uninstall is run from the build dir, not the install
  // dir, because it is useful for tests to be able to run it to clean the
  // system even if installation has failed or the installed binaries have
  // already been removed.
  base::FilePath path = GetInstallerPath().DirName().AppendASCII("updater.exe");
  ASSERT_FALSE(path.empty());
  base::CommandLine command_line(path);
  command_line.AppendSwitch("uninstall");
  int exit_code = -1;
  ASSERT_TRUE(Run(scope, command_line, &exit_code));
  EXPECT_EQ(0, exit_code);

  // Uninstallation involves a race with the uninstall.cmd script and the
  // process exit. Sleep to allow the script to complete its work.
  SleepFor(5);
}

void SetActive(UpdaterScope scope, const std::string& id) {
  // TODO(crbug/1159498): Standardize registry access.
  base::win::RegKey key;
  ASSERT_EQ(key.Create(HKEY_CURRENT_USER, GetAppClientStateKey(id).c_str(),
                       KEY_WRITE | KEY_WOW64_32KEY),
            ERROR_SUCCESS);
  EXPECT_EQ(key.WriteValue(kDidRun, L"1"), ERROR_SUCCESS);
}

void ExpectActive(UpdaterScope scope, const std::string& id) {
  // TODO(crbug/1159498): Standardize registry access.
  base::win::RegKey key;
  ASSERT_EQ(key.Open(HKEY_CURRENT_USER, GetAppClientStateKey(id).c_str(),
                     KEY_READ | KEY_WOW64_32KEY),
            ERROR_SUCCESS);
  std::wstring value;
  ASSERT_EQ(key.ReadValue(kDidRun, &value), ERROR_SUCCESS);
  EXPECT_EQ(value, L"1");
}

void ExpectNotActive(UpdaterScope scope, const std::string& id) {
  // TODO(crbug/1159498): Standardize registry access.
  base::win::RegKey key;
  if (key.Open(HKEY_CURRENT_USER, GetAppClientStateKey(id).c_str(),
               KEY_READ | KEY_WOW64_32KEY) == ERROR_SUCCESS) {
    std::wstring value;
    if (key.ReadValue(kDidRun, &value) == ERROR_SUCCESS)
      EXPECT_EQ(value, L"0");
  }
}

// Tests if the typelibs and some of the public, internal, and
// legacy interfaces are available. Failure to query these interfaces indicates
// an issue with typelib registration.
void ExpectInterfacesRegistered() {
  {  // IUpdater, IGoogleUpdate3Web and IAppBundleWeb.
    // The block is necessary so that updater_server goes out of scope and
    // releases the prefs lock before updater_internal_server tries to acquire
    // it to mode-check.
    Microsoft::WRL::ComPtr<IUnknown> updater_server;
    EXPECT_HRESULT_SUCCEEDED(::CoCreateInstance(__uuidof(UpdaterClass), nullptr,
                                                CLSCTX_LOCAL_SERVER,
                                                IID_PPV_ARGS(&updater_server)));
    Microsoft::WRL::ComPtr<IUpdater> updater;
    EXPECT_HRESULT_SUCCEEDED(updater_server.As(&updater));

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

  // IUpdaterInternal.
  Microsoft::WRL::ComPtr<IUnknown> updater_internal_server;
  EXPECT_HRESULT_SUCCEEDED(::CoCreateInstance(
      __uuidof(UpdaterInternalClass), nullptr, CLSCTX_LOCAL_SERVER,
      IID_PPV_ARGS(&updater_internal_server)));
  Microsoft::WRL::ComPtr<IUpdaterInternal> updater_internal;
  EXPECT_HRESULT_SUCCEEDED(updater_internal_server.As(&updater_internal));
}

}  // namespace test
}  // namespace updater
