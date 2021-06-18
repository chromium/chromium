// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <wrl/client.h>

#include <string>
#include <vector>

#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
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
#include "chrome/updater/prefs.h"
#include "chrome/updater/test/integration_tests_impl.h"
#include "chrome/updater/test/test_app/constants.h"
#include "chrome/updater/test/test_app/test_app_version.h"
#include "chrome/updater/updater_branding.h"
#include "chrome/updater/updater_scope.h"
#include "chrome/updater/updater_version.h"
#include "chrome/updater/util.h"
#include "chrome/updater/win/setup/setup_util.h"
#include "chrome/updater/win/win_constants.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
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

absl::optional<base::FilePath> GetProductPath() {
  base::FilePath app_data_dir;
  if (!base::PathService::Get(base::DIR_LOCAL_APP_DATA, &app_data_dir))
    return absl::nullopt;
  return app_data_dir.AppendASCII(COMPANY_SHORTNAME_STRING)
      .AppendASCII(PRODUCT_FULLNAME_STRING)
      .AppendASCII(kUpdaterVersion);
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

absl::optional<base::FilePath> GetInstalledExecutablePath(UpdaterScope scope) {
  absl::optional<base::FilePath> path = GetProductPath();
  if (!path)
    return absl::nullopt;
  return path->AppendASCII("updater.exe");
}

absl::optional<base::FilePath> GetFakeUpdaterInstallFolderPath(
    UpdaterScope scope,
    const base::Version& version) {
  absl::optional<base::FilePath> path = GetProductPath();
  if (!path)
    return absl::nullopt;
  return path->AppendASCII(version.GetString());
}

absl::optional<base::FilePath> GetDataDirPath(UpdaterScope scope) {
  base::FilePath app_data_dir;
  if (!base::PathService::Get(base::DIR_LOCAL_APP_DATA, &app_data_dir))
    return absl::nullopt;
  return app_data_dir.AppendASCII(COMPANY_SHORTNAME_STRING)
      .AppendASCII(PRODUCT_FULLNAME_STRING);
}

bool DeleteService() {
  SC_HANDLE scm = ::OpenSCManager(
      nullptr, nullptr, SC_MANAGER_CONNECT | SC_MANAGER_CREATE_SERVICE);
  if (!scm)
    return false;

  SC_HANDLE service = ::OpenService(scm, kWindowsServiceName, DELETE);
  bool is_service_deleted = !service;
  if (!is_service_deleted) {
    is_service_deleted =
        ::DeleteService(service)
            ? true
            : ::GetLastError() == ERROR_SERVICE_MARKED_FOR_DELETE;

    ::CloseServiceHandle(service);
  }

  ::CloseServiceHandle(scm);

  base::win::RegKey(HKEY_LOCAL_MACHINE, base::ASCIIToWide(UPDATER_KEY).c_str(),
                    KEY_WRITE)
      .DeleteValue(kWindowsServiceName);

  return is_service_deleted;
}

void Clean(UpdaterScope scope) {
  const HKEY root =
      scope == UpdaterScope::kSystem ? HKEY_LOCAL_MACHINE : HKEY_CURRENT_USER;
  for (const char* key : {CLIENT_STATE_KEY, CLIENTS_KEY, UPDATER_KEY}) {
    EXPECT_TRUE(DeleteRegKey(root, KEY_WOW64_32KEY, base::ASCIIToWide(key)));
  }
  for (const wchar_t* key : {kRegKeyCompanyCloudManagement,
                             kRegKeyCompanyEnrollment, UPDATER_POLICIES_KEY}) {
    EXPECT_TRUE(DeleteRegKey(HKEY_LOCAL_MACHINE, 0, key));
  }

  for (const CLSID& clsid :
       JoinVectors(GetSideBySideServers(scope), GetActiveServers(scope))) {
    EXPECT_TRUE(DeleteRegKey(root, 0, GetComServerClsidRegistryPath(clsid)));
    if (scope == UpdaterScope::kSystem)
      EXPECT_TRUE(DeleteRegKey(root, 0, GetComServerAppidRegistryPath(clsid)));
  }

  for (const IID& iid :
       JoinVectors(GetSideBySideInterfaces(), GetActiveInterfaces())) {
    EXPECT_TRUE(DeleteRegKey(root, 0, GetComIidRegistryPath(iid)));
    EXPECT_TRUE(DeleteRegKey(root, 0, GetComTypeLibRegistryPath(iid)));
  }

  if (scope == UpdaterScope::kSystem) {
    EXPECT_TRUE(DeleteService());
  }

  // TODO(crbug.com/1062288): Delete the Wake task.
  absl::optional<base::FilePath> path = GetProductPath();
  EXPECT_TRUE(path);
  if (path)
    EXPECT_TRUE(base::DeletePathRecursively(*path));
  path = GetDataDirPath(scope);
  EXPECT_TRUE(path);
  if (path)
    EXPECT_TRUE(base::DeletePathRecursively(*path));
}

bool IsServiceGone() {
  SC_HANDLE scm = ::OpenSCManager(
      nullptr, nullptr, SC_MANAGER_CONNECT | SC_MANAGER_CREATE_SERVICE);
  if (!scm)
    return false;

  SC_HANDLE service = ::OpenService(
      scm, kWindowsServiceName, SERVICE_QUERY_CONFIG | SERVICE_CHANGE_CONFIG);
  bool is_service_gone = !service;
  if (!is_service_gone) {
    if (!::ChangeServiceConfig(service, SERVICE_NO_CHANGE, SERVICE_NO_CHANGE,
                               SERVICE_NO_CHANGE, nullptr, nullptr, nullptr,
                               nullptr, nullptr, nullptr,
                               L"Test Service Display Name")) {
      is_service_gone = ::GetLastError() == ERROR_SERVICE_MARKED_FOR_DELETE;
    }

    ::CloseServiceHandle(service);
  }

  ::CloseServiceHandle(scm);

  return is_service_gone &&
         !base::win::RegKey(HKEY_LOCAL_MACHINE,
                            base::ASCIIToWide(UPDATER_KEY).c_str(), KEY_READ)
              .HasValue(kWindowsServiceName);
}

void ExpectClean(UpdaterScope scope) {
  const HKEY root =
      scope == UpdaterScope::kSystem ? HKEY_LOCAL_MACHINE : HKEY_CURRENT_USER;
  for (const char* key : {CLIENT_STATE_KEY, CLIENTS_KEY, UPDATER_KEY}) {
    EXPECT_FALSE(RegKeyExists(root, KEY_WOW64_32KEY, base::ASCIIToWide(key)));
  }
  for (const wchar_t* key : {kRegKeyCompanyCloudManagement,
                             kRegKeyCompanyEnrollment, UPDATER_POLICIES_KEY}) {
    EXPECT_FALSE(RegKeyExists(HKEY_LOCAL_MACHINE, 0, key));
  }

  for (const CLSID& clsid :
       JoinVectors(GetSideBySideServers(scope), GetActiveServers(scope))) {
    EXPECT_FALSE(RegKeyExists(root, 0, GetComServerClsidRegistryPath(clsid)));
    if (scope == UpdaterScope::kSystem)
      EXPECT_FALSE(RegKeyExists(root, 0, GetComServerAppidRegistryPath(clsid)));
  }

  for (const IID& iid :
       JoinVectors(GetSideBySideInterfaces(), GetActiveInterfaces())) {
    EXPECT_FALSE(RegKeyExists(root, 0, GetComIidRegistryPath(iid)));
    EXPECT_FALSE(RegKeyExists(root, 0, GetComTypeLibRegistryPath(iid)));
  }

  if (scope == UpdaterScope::kSystem)
    EXPECT_TRUE(IsServiceGone());

  // TODO(crbug.com/1062288): Assert there are no Wake tasks.

  // Files must not exist on the file system.
  absl::optional<base::FilePath> path = GetProductPath();
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
  absl::optional<base::FilePath> path = GetProductPath();
  EXPECT_TRUE(path);
  if (path)
    EXPECT_TRUE(base::PathExists(*path));
}

void ExpectCandidateUninstalled(UpdaterScope scope) {
  // TODO(crbug.com/1062288): Assert there are no side-by-side COM interfaces.
  // TODO(crbug.com/1062288): Assert there are no Wake tasks.

  // Files must not exist on the file system.
  absl::optional<base::FilePath> path = GetProductPath();
  EXPECT_TRUE(path);
  if (path)
    EXPECT_FALSE(base::PathExists(*path));
}

void ExpectActiveUpdater(UpdaterScope scope) {
  // TODO(crbug.com/1062288): Assert that COM interfaces point to this version.

  // Files must exist on the file system.
  absl::optional<base::FilePath> path = GetProductPath();
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

void WaitForServerExit(UpdaterScope scope) {
  // CreateGlobalPrefs will block until it can acquire the prefs lock.
  CreateGlobalPrefs(scope);
}

// Tests if the typelibs and some of the public, internal, and
// legacy interfaces are available. Failure to query these interfaces indicates
// an issue with typelib registration.
void ExpectInterfacesRegistered(UpdaterScope scope) {
  {  // IUpdater, IGoogleUpdate3Web and IAppBundleWeb.
    // The block is necessary so that updater_server goes out of scope and
    // releases the prefs lock before updater_internal_server tries to acquire
    // it to mode-check.
    Microsoft::WRL::ComPtr<IUnknown> updater_server;
    ASSERT_HRESULT_SUCCEEDED(::CoCreateInstance(
        scope == UpdaterScope::kSystem ? __uuidof(UpdaterSystemClass)
                                       : __uuidof(UpdaterUserClass),
        nullptr, CLSCTX_LOCAL_SERVER, IID_PPV_ARGS(&updater_server)));
    Microsoft::WRL::ComPtr<IUpdater> updater;
    EXPECT_HRESULT_SUCCEEDED(updater_server.As(&updater));

    Microsoft::WRL::ComPtr<IUnknown> updater_legacy_server;
    EXPECT_HRESULT_SUCCEEDED(::CoCreateInstance(
        scope == UpdaterScope::kSystem ? __uuidof(GoogleUpdate3WebSystemClass)
                                       : __uuidof(GoogleUpdate3WebUserClass),
        nullptr, CLSCTX_LOCAL_SERVER, IID_PPV_ARGS(&updater_legacy_server)));
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
      scope == UpdaterScope::kSystem ? __uuidof(UpdaterInternalSystemClass)
                                     : __uuidof(UpdaterInternalUserClass),
      nullptr, CLSCTX_LOCAL_SERVER, IID_PPV_ARGS(&updater_internal_server)));
  Microsoft::WRL::ComPtr<IUpdaterInternal> updater_internal;
  EXPECT_HRESULT_SUCCEEDED(updater_internal_server.As(&updater_internal));
}

}  // namespace test
}  // namespace updater
