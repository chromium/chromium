// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <wrl/client.h>

#include <string>
#include <vector>

#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/path_service.h"
#include "base/process/launch.h"
#include "base/process/process.h"
#include "base/strings/strcat.h"
#include "base/strings/utf_string_conversions.h"
#include "base/synchronization/waitable_event.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/test/bind.h"
#include "base/version.h"
#include "base/win/registry.h"
#include "chrome/updater/app/server/win/updater_idl.h"
#include "chrome/updater/app/server/win/updater_internal_idl.h"
#include "chrome/updater/app/server/win/updater_legacy_idl.h"
#include "chrome/updater/constants.h"
#include "chrome/updater/external_constants_builder.h"
#include "chrome/updater/prefs.h"
#include "chrome/updater/test/integration_tests_impl.h"
#include "chrome/updater/updater_branding.h"
#include "chrome/updater/updater_scope.h"
#include "chrome/updater/updater_version.h"
#include "chrome/updater/util.h"
#include "chrome/updater/win/setup/setup_util.h"
#include "chrome/updater/win/win_constants.h"
#include "chrome/updater/win/win_util.h"
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
  return test_executable.DirName().AppendASCII("UpdaterSetup_test.exe");
}

// Returns the root directory where the updater product is installed. This
// is the parent directory where the versioned directories of the
// updater instances are.
absl::optional<base::FilePath> GetProductPath(UpdaterScope scope) {
  base::FilePath app_data_dir;
  if (!base::PathService::Get(scope == UpdaterScope::kSystem
                                  ? base::DIR_PROGRAM_FILES
                                  : base::DIR_LOCAL_APP_DATA,
                              &app_data_dir)) {
    return absl::nullopt;
  }
  return app_data_dir.AppendASCII(COMPANY_SHORTNAME_STRING)
      .AppendASCII(PRODUCT_FULLNAME_STRING);
}

// Returns the versioned directory of this updater instances.
absl::optional<base::FilePath> GetProductVersionPath(UpdaterScope scope) {
  absl::optional<base::FilePath> product_path = GetProductPath(scope);
  return product_path ? product_path->AppendASCII(kUpdaterVersion)
                      : product_path;
}

std::wstring GetAppClientStateKey(const std::string& id) {
  return base::StrCat({CLIENT_STATE_KEY, base::ASCIIToWide(id)});
}

bool RegKeyExists(HKEY root, const std::wstring& path) {
  return base::win::RegKey(root, path.c_str(), Wow6432(KEY_QUERY_VALUE))
      .Valid();
}

bool RegKeyExistsCOM(HKEY root, const std::wstring& path) {
  return base::win::RegKey(root, path.c_str(), KEY_QUERY_VALUE).Valid();
}

bool DeleteRegKey(HKEY root, const std::wstring& path) {
  LONG result =
      base::win::RegKey(root, L"", Wow6432(KEY_READ)).DeleteKey(path.c_str());
  return result == ERROR_SUCCESS || result == ERROR_FILE_NOT_FOUND;
}

bool DeleteRegKeyCOM(HKEY root, const std::wstring& path) {
  LONG result = base::win::RegKey(root, L"", KEY_READ).DeleteKey(path.c_str());
  return result == ERROR_SUCCESS || result == ERROR_FILE_NOT_FOUND;
}

bool DeleteRegValue(HKEY root,
                    const std::wstring& path,
                    const std::wstring& value) {
  if (!base::win::RegKey(root, path.c_str(), Wow6432(KEY_QUERY_VALUE))
           .Valid()) {
    return true;
  }

  LONG result = base::win::RegKey(root, path.c_str(), Wow6432(KEY_WRITE))
                    .DeleteValue(value.c_str());
  return result == ERROR_SUCCESS || result == ERROR_FILE_NOT_FOUND;
}

bool DeleteService(const std::wstring& service_name) {
  SC_HANDLE scm = ::OpenSCManager(
      nullptr, nullptr, SC_MANAGER_CONNECT | SC_MANAGER_CREATE_SERVICE);
  if (!scm)
    return false;

  SC_HANDLE service = ::OpenService(scm, service_name.c_str(), DELETE);
  bool is_service_deleted = !service;
  if (!is_service_deleted) {
    is_service_deleted =
        ::DeleteService(service)
            ? true
            : ::GetLastError() == ERROR_SERVICE_MARKED_FOR_DELETE;

    ::CloseServiceHandle(service);
  }

  DeleteRegValue(HKEY_LOCAL_MACHINE, UPDATER_KEY, service_name);

  ::CloseServiceHandle(scm);

  return is_service_deleted;
}

bool IsServiceGone(const std::wstring& service_name) {
  SC_HANDLE scm = ::OpenSCManager(
      nullptr, nullptr, SC_MANAGER_CONNECT | SC_MANAGER_CREATE_SERVICE);
  if (!scm)
    return false;

  SC_HANDLE service = ::OpenService(
      scm, service_name.c_str(), SERVICE_QUERY_CONFIG | SERVICE_CHANGE_CONFIG);
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
         !base::win::RegKey(HKEY_LOCAL_MACHINE, UPDATER_KEY, Wow6432(KEY_READ))
              .HasValue(service_name.c_str());
}

}  // namespace

absl::optional<base::FilePath> GetInstalledExecutablePath(UpdaterScope scope) {
  absl::optional<base::FilePath> path = GetProductVersionPath(scope);
  if (!path)
    return absl::nullopt;
  return path->AppendASCII("updater.exe");
}

absl::optional<base::FilePath> GetFakeUpdaterInstallFolderPath(
    UpdaterScope scope,
    const base::Version& version) {
  absl::optional<base::FilePath> path = GetProductVersionPath(scope);
  if (!path)
    return absl::nullopt;
  return path->AppendASCII(version.GetString());
}

absl::optional<base::FilePath> GetDataDirPath(UpdaterScope scope) {
  return GetProductPath(scope);
}

void Clean(UpdaterScope scope) {
  const HKEY root =
      scope == UpdaterScope::kSystem ? HKEY_LOCAL_MACHINE : HKEY_CURRENT_USER;
  for (const wchar_t* key : {CLIENT_STATE_KEY, CLIENTS_KEY, UPDATER_KEY}) {
    EXPECT_TRUE(DeleteRegKey(root, key));
  }
  for (const wchar_t* key : {kRegKeyCompanyCloudManagement,
                             kRegKeyCompanyEnrollment, UPDATER_POLICIES_KEY}) {
    EXPECT_TRUE(DeleteRegKey(HKEY_LOCAL_MACHINE, key));
  }

  for (const CLSID& clsid :
       JoinVectors(GetSideBySideServers(scope), GetActiveServers(scope))) {
    EXPECT_TRUE(DeleteRegKeyCOM(root, GetComServerClsidRegistryPath(clsid)));
    if (scope == UpdaterScope::kSystem)
      EXPECT_TRUE(DeleteRegKeyCOM(root, GetComServerAppidRegistryPath(clsid)));
  }

  for (const IID& iid :
       JoinVectors(GetSideBySideInterfaces(), GetActiveInterfaces())) {
    EXPECT_TRUE(DeleteRegKeyCOM(root, GetComIidRegistryPath(iid)));
    EXPECT_TRUE(DeleteRegKeyCOM(root, GetComTypeLibRegistryPath(iid)));
  }

  if (scope == UpdaterScope::kSystem) {
    for (const bool is_internal_service : {true, false}) {
      EXPECT_TRUE(DeleteService(GetServiceName(is_internal_service)));
    }
  }

  // TODO(crbug.com/1062288): Delete the Wake task.
  absl::optional<base::FilePath> path = GetProductPath(scope);
  EXPECT_TRUE(path);
  if (path)
    EXPECT_TRUE(base::DeletePathRecursively(*path));
  path = GetDataDirPath(scope);
  EXPECT_TRUE(path);
  if (path)
    EXPECT_TRUE(base::DeletePathRecursively(*path));
}

void ExpectClean(UpdaterScope scope) {
  const HKEY root =
      scope == UpdaterScope::kSystem ? HKEY_LOCAL_MACHINE : HKEY_CURRENT_USER;
  for (const wchar_t* key : {CLIENT_STATE_KEY, CLIENTS_KEY, UPDATER_KEY}) {
    EXPECT_FALSE(RegKeyExists(root, key));
  }
  for (const wchar_t* key : {kRegKeyCompanyCloudManagement,
                             kRegKeyCompanyEnrollment, UPDATER_POLICIES_KEY}) {
    EXPECT_FALSE(RegKeyExists(HKEY_LOCAL_MACHINE, key));
  }

  for (const CLSID& clsid :
       JoinVectors(GetSideBySideServers(scope), GetActiveServers(scope))) {
    EXPECT_FALSE(RegKeyExistsCOM(root, GetComServerClsidRegistryPath(clsid)));
    if (scope == UpdaterScope::kSystem)
      EXPECT_FALSE(RegKeyExistsCOM(root, GetComServerAppidRegistryPath(clsid)));
  }

  for (const IID& iid :
       JoinVectors(GetSideBySideInterfaces(), GetActiveInterfaces())) {
    EXPECT_FALSE(RegKeyExistsCOM(root, GetComIidRegistryPath(iid)));
    EXPECT_FALSE(RegKeyExistsCOM(root, GetComTypeLibRegistryPath(iid)));
  }

  if (scope == UpdaterScope::kSystem) {
    for (const bool is_internal_service : {true, false}) {
      EXPECT_TRUE(IsServiceGone(GetServiceName(is_internal_service)));
    }
  }

  // TODO(crbug.com/1062288): Assert there are no Wake tasks.

  // Files must not exist on the file system.
  absl::optional<base::FilePath> path = GetProductVersionPath(scope);
  EXPECT_TRUE(path);
  if (path) {
    EXPECT_TRUE(WaitFor(base::BindLambdaForTesting(
        [&]() { return !base::PathExists(*path); })));
  }
  path = GetDataDirPath(scope);
  EXPECT_TRUE(path);
  if (path) {
    EXPECT_TRUE(WaitFor(base::BindLambdaForTesting(
        [&]() { return !base::PathExists(*path); })));
  }
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
  absl::optional<base::FilePath> path = GetProductVersionPath(scope);
  EXPECT_TRUE(path);
  if (path)
    EXPECT_TRUE(base::PathExists(*path));
}

void ExpectCandidateUninstalled(UpdaterScope scope) {
  // TODO(crbug.com/1062288): Assert there are no side-by-side COM interfaces.
  // TODO(crbug.com/1062288): Assert there are no Wake tasks.

  // Files must not exist on the file system.
  absl::optional<base::FilePath> path = GetProductVersionPath(scope);
  EXPECT_TRUE(path);
  if (path)
    EXPECT_FALSE(base::PathExists(*path));
}

void ExpectActiveUpdater(UpdaterScope scope) {
  // TODO(crbug.com/1062288): Assert that COM interfaces point to this version.

  // Files must exist on the file system.
  absl::optional<base::FilePath> path = GetProductVersionPath(scope);
  EXPECT_TRUE(path);
  if (path)
    EXPECT_TRUE(base::PathExists(*path));
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
  // Note: updater_test.exe --uninstall is run from the build dir, not the
  // install dir, because it is useful for tests to be able to run it to clean
  // the system even if installation has failed or the installed binaries have
  // already been removed.
  base::FilePath path =
      GetInstallerPath().DirName().AppendASCII("updater_test.exe");
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

void SetActive(UpdaterScope /*scope*/, const std::string& id) {
  // TODO(crbug.com/1159498): Standardize registry access.
  base::win::RegKey key;
  ASSERT_EQ(key.Create(HKEY_CURRENT_USER, GetAppClientStateKey(id).c_str(),
                       Wow6432(KEY_WRITE)),
            ERROR_SUCCESS);
  EXPECT_EQ(key.WriteValue(kDidRun, L"1"), ERROR_SUCCESS);
}

void ExpectActive(UpdaterScope /*scope*/, const std::string& id) {
  // TODO(crbug.com/1159498): Standardize registry access.
  base::win::RegKey key;
  ASSERT_EQ(key.Open(HKEY_CURRENT_USER, GetAppClientStateKey(id).c_str(),
                     Wow6432(KEY_READ)),
            ERROR_SUCCESS);
  std::wstring value;
  ASSERT_EQ(key.ReadValue(kDidRun, &value), ERROR_SUCCESS);
  EXPECT_EQ(value, L"1");
}

void ExpectNotActive(UpdaterScope /*scope*/, const std::string& id) {
  // TODO(crbug.com/1159498): Standardize registry access.
  base::win::RegKey key;
  if (key.Open(HKEY_CURRENT_USER, GetAppClientStateKey(id).c_str(),
               Wow6432(KEY_READ)) == ERROR_SUCCESS) {
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

int RunVPythonCommand(const base::CommandLine& command_line) {
  base::CommandLine python_command = command_line;
  python_command.PrependWrapper(FILE_PATH_LITERAL("vpython.bat"));

  int exit_code = -1;
  base::Process process = base::LaunchProcess(python_command, {});
  EXPECT_TRUE(process.IsValid());
  EXPECT_TRUE(process.WaitForExitWithTimeout(base::TimeDelta::FromSeconds(60),
                                             &exit_code));
  return exit_code;
}

void RunTestServiceCommand(const std::string& sub_command) {
  base::FilePath path(base::CommandLine::ForCurrentProcess()->GetProgram());
  path = path.DirName();
  path = MakeAbsoluteFilePath(path);
  path = path.Append(FILE_PATH_LITERAL("test_service"))
             .Append(FILE_PATH_LITERAL("updater_test_service_control.py"));
  EXPECT_TRUE(base::PathExists(path));

  base::CommandLine command(path);
  command.AppendArg(sub_command);

  EXPECT_EQ(RunVPythonCommand(command), 0);
}

}  // namespace test
}  // namespace updater
