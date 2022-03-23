// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <wrl/client.h>

#include <regstr.h>

#include <iostream>
#include <memory>
#include <string>
#include <vector>

#include "base/command_line.h"
#include "base/containers/flat_map.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/json/json_writer.h"
#include "base/logging.h"
#include "base/path_service.h"
#include "base/process/launch.h"
#include "base/process/process.h"
#include "base/strings/strcat.h"
#include "base/strings/stringprintf.h"
#include "base/strings/sys_string_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/synchronization/waitable_event.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/test/bind.h"
#include "base/time/time.h"
#include "base/timer/elapsed_timer.h"
#include "base/values.h"
#include "base/version.h"
#include "base/win/registry.h"
#include "base/win/scoped_bstr.h"
#include "build/branding_buildflags.h"
#include "build/build_config.h"
#include "chrome/updater/app/server/win/updater_idl.h"
#include "chrome/updater/app/server/win/updater_internal_idl.h"
#include "chrome/updater/app/server/win/updater_legacy_idl.h"
#include "chrome/updater/constants.h"
#include "chrome/updater/external_constants_builder.h"
#include "chrome/updater/persisted_data.h"
#include "chrome/updater/prefs.h"
#include "chrome/updater/test/integration_tests_impl.h"
#include "chrome/updater/updater_branding.h"
#include "chrome/updater/updater_scope.h"
#include "chrome/updater/updater_version.h"
#include "chrome/updater/util.h"
#include "chrome/updater/win/setup/setup_util.h"
#include "chrome/updater/win/task_scheduler.h"
#include "chrome/updater/win/win_constants.h"
#include "chrome/updater/win/win_util.h"
#include "components/crx_file/crx_verifier.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "url/gurl.h"

namespace updater {
namespace test {
namespace {

constexpr wchar_t kDidRun[] = L"dr";

enum class CheckInstallationStatus {
  kCheckIsNotInstalled = 0,
  kCheckIsInstalled = 1,
};

enum class CheckInstallationVersions {
  kCheckSxSOnly = 0,
  kCheckActiveAndSxS = 1,
};

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

std::wstring GetAppClientsKey(const std::wstring& id) {
  return base::StrCat({CLIENTS_KEY, id});
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

// Checks the installation states (installed or uninstalled) and versions (SxS
// only, or both active and SxS). The installation state includes
// Client/ClientState registry, COM server registration, COM service
// registration, COM interfaces, wake tasks, and files on the file system.
void CheckInstallation(UpdaterScope scope,
                       CheckInstallationStatus check_installation_status,
                       CheckInstallationVersions check_installation_versions) {
  const bool is_installed =
      check_installation_status == CheckInstallationStatus::kCheckIsInstalled;
  const bool is_active_and_sxs = check_installation_versions ==
                                 CheckInstallationVersions::kCheckActiveAndSxS;

  const HKEY root = UpdaterScopeToHKeyRoot(scope);

  if (is_active_and_sxs) {
    for (const wchar_t* key : {CLIENT_STATE_KEY, CLIENTS_KEY, UPDATER_KEY}) {
      EXPECT_EQ(is_installed, RegKeyExists(root, key));
    }

    EXPECT_EQ(is_installed, base::PathExists(*GetGoogleUpdateExePath(scope)));

    if (is_installed) {
      std::wstring pv;
      EXPECT_EQ(ERROR_SUCCESS,
                base::win::RegKey(
                    root,
                    base::StrCat({CLIENTS_KEY,
                                  L"{430FD4D0-B729-4F61-AA34-91526481799D}"})
                        .c_str(),
                    Wow6432(KEY_READ))
                    .ReadValue(kRegValuePV, &pv));
      EXPECT_STREQ(kUpdaterVersionUtf16, pv.c_str());

      std::wstring uninstall_cmd_line_string;
      EXPECT_EQ(ERROR_SUCCESS,
                base::win::RegKey(root, UPDATER_KEY, Wow6432(KEY_READ))
                    .ReadValue(kRegValueUninstallCmdLine,
                               &uninstall_cmd_line_string));
      EXPECT_TRUE(base::CommandLine::FromString(uninstall_cmd_line_string)
                      .HasSwitch(kUninstallIfUnusedSwitch));

      if (scope == UpdaterScope::kUser) {
        std::wstring run_updater_wake_command;
        EXPECT_EQ(ERROR_SUCCESS,
                  base::win::RegKey(root, REGSTR_PATH_RUN, KEY_READ)
                      .ReadValue(GetTaskNamePrefix(scope).c_str(),
                                 &run_updater_wake_command));
        EXPECT_TRUE(base::CommandLine::FromString(run_updater_wake_command)
                        .HasSwitch(kWakeSwitch));
      }
    } else {
      for (const wchar_t* key :
           {kRegKeyCompanyCloudManagement, kRegKeyCompanyEnrollment,
            UPDATER_POLICIES_KEY}) {
        EXPECT_FALSE(RegKeyExists(HKEY_LOCAL_MACHINE, key));
      }

      EXPECT_FALSE(RegKeyExists(root, UPDATER_KEY));

      if (scope == UpdaterScope::kUser) {
        EXPECT_FALSE(base::win::RegKey(root, REGSTR_PATH_RUN, KEY_READ)
                         .HasValue(GetTaskNamePrefix(scope).c_str()));
      }
    }
  }

  for (const CLSID& clsid :
       JoinVectors(GetSideBySideServers(scope), is_active_and_sxs
                                                    ? GetActiveServers(scope)
                                                    : std::vector<CLSID>())) {
    EXPECT_EQ(is_installed,
              RegKeyExistsCOM(root, GetComServerClsidRegistryPath(clsid)));
    if (scope == UpdaterScope::kSystem) {
      EXPECT_EQ(is_installed,
                RegKeyExistsCOM(root, GetComServerAppidRegistryPath(clsid)));
    }
  }

  for (const IID& iid : JoinVectors(
           GetSideBySideInterfaces(),
           is_active_and_sxs ? GetActiveInterfaces() : std::vector<IID>())) {
    EXPECT_EQ(is_installed, RegKeyExistsCOM(root, GetComIidRegistryPath(iid)));
    EXPECT_EQ(is_installed,
              RegKeyExistsCOM(root, GetComTypeLibRegistryPath(iid)));
  }

  if (scope == UpdaterScope::kSystem) {
    for (const bool is_internal_service : {false, true}) {
      if (!is_active_and_sxs && !is_internal_service)
        continue;

      EXPECT_EQ(is_installed,
                !IsServiceGone(GetServiceName(is_internal_service)));
    }
  }

  if (is_installed) {
    std::unique_ptr<TaskScheduler> task_scheduler =
        TaskScheduler::CreateInstance();
    const std::wstring task_name =
        task_scheduler->FindFirstTaskName(GetTaskNamePrefix(scope));
    EXPECT_TRUE(!task_name.empty());
    EXPECT_TRUE(task_scheduler->IsTaskRegistered(task_name.c_str()));

    TaskScheduler::TaskInfo task_info;
    ASSERT_TRUE(task_scheduler->GetTaskInfo(task_name.c_str(), &task_info));
    ASSERT_EQ(task_info.exec_actions.size(), 1u);
    EXPECT_STREQ(
        task_info.exec_actions[0].arguments.c_str(),
        base::StrCat(
            {L"--wake ", scope == UpdaterScope::kSystem ? L"--system " : L"",
             L"--enable-logging "
             L"--vmodule=*/chrome/updater/*=2,*/components/winhttp/*=2"})
            .c_str());
  }

  const absl::optional<base::FilePath> product_version_path =
      GetProductVersionPath(scope);
  const absl::optional<base::FilePath> data_dir_path = GetDataDirPath(scope);

  for (const absl::optional<base::FilePath>& path :
       {product_version_path, data_dir_path}) {
    if (!is_active_and_sxs && path == data_dir_path)
      continue;

    EXPECT_TRUE(path);
    EXPECT_TRUE(WaitFor(base::BindLambdaForTesting(
        [&]() { return is_installed == base::PathExists(*path); })));
  }
}

// Returns true is any updater process is found running in any session in the
// system, regardless of its path.
bool IsUpdaterRunning() {
  return IsProcessRunning(kUpdaterProcessName);
}

void SleepFor(int seconds) {
  VLOG(2) << "Sleeping " << seconds << " seconds...";
  base::WaitableEvent().TimedWait(base::Seconds(seconds));
  VLOG(2) << "Sleep complete.";
}

}  // namespace

base::FilePath GetSetupExecutablePath() {
  base::FilePath test_executable;
  if (!base::PathService::Get(base::FILE_EXE, &test_executable))
    return base::FilePath();
  return test_executable.DirName().AppendASCII("UpdaterSetup_test.exe");
}

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
  const HKEY root = UpdaterScopeToHKeyRoot(scope);
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

  if (scope == UpdaterScope::kUser) {
    base::win::RegKey(root, REGSTR_PATH_RUN, KEY_READ)
        .DeleteValue(GetTaskNamePrefix(scope).c_str());
  }

  if (scope == UpdaterScope::kSystem) {
    for (const bool is_internal_service : {true, false}) {
      EXPECT_TRUE(DeleteService(GetServiceName(is_internal_service)));
    }
  }

  std::unique_ptr<TaskScheduler> task_scheduler =
      TaskScheduler::CreateInstance();
  const std::wstring task_name =
      task_scheduler->FindFirstTaskName(GetTaskNamePrefix(scope));
  if (!task_name.empty())
    task_scheduler->DeleteTask(task_name.c_str());
  EXPECT_TRUE(
      task_scheduler->FindFirstTaskName(GetTaskNamePrefix(scope)).empty());

  absl::optional<base::FilePath> path = GetProductPath(scope);
  EXPECT_TRUE(path);
  if (path)
    EXPECT_TRUE(base::DeletePathRecursively(*path));
  path = GetDataDirPath(scope);
  EXPECT_TRUE(path);
  if (path)
    EXPECT_TRUE(base::DeletePathRecursively(*path));

  const absl::optional<base::FilePath> target_path =
      GetGoogleUpdateExePath(scope);
  if (target_path)
    base::DeleteFile(*target_path);
}

void EnterTestMode(const GURL& url) {
  ASSERT_TRUE(ExternalConstantsBuilder()
                  .SetUpdateURL(std::vector<std::string>{url.spec()})
                  .SetUseCUP(false)
                  .SetInitialDelay(0.1)
                  .SetCrxVerifierFormat(crx_file::VerifierFormat::CRX3)
                  .Overwrite());
}

void ExpectInstalled(UpdaterScope scope) {
  CheckInstallation(scope, CheckInstallationStatus::kCheckIsInstalled,
                    CheckInstallationVersions::kCheckSxSOnly);
}

void ExpectClean(UpdaterScope scope) {
  CheckInstallation(scope, CheckInstallationStatus::kCheckIsNotInstalled,
                    CheckInstallationVersions::kCheckActiveAndSxS);
}

void ExpectCandidateUninstalled(UpdaterScope scope) {
  CheckInstallation(scope, CheckInstallationStatus::kCheckIsNotInstalled,
                    CheckInstallationVersions::kCheckSxSOnly);
}

void ExpectActiveUpdater(UpdaterScope scope) {
  CheckInstallation(scope, CheckInstallationStatus::kCheckIsInstalled,
                    CheckInstallationVersions::kCheckActiveAndSxS);
}

void Uninstall(UpdaterScope scope) {
  // Note: updater_test.exe --uninstall is run from the build dir, not the
  // install dir, because it is useful for tests to be able to run it to clean
  // the system even if installation has failed or the installed binaries have
  // already been removed.
  base::FilePath path =
      GetSetupExecutablePath().DirName().AppendASCII("updater_test.exe");
  ASSERT_FALSE(path.empty());
  base::CommandLine command_line(path);
  command_line.AppendSwitch("uninstall");
  int exit_code = -1;
  ASSERT_TRUE(Run(scope, command_line, &exit_code));
  EXPECT_EQ(0, exit_code);

  // Uninstallation involves a race with the uninstall.cmd script and the
  // process exit. Sleep to allow the script to complete its work.
  // TODO(crbug.com/1217765): Figure out a way to replace this.
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

// Waits for all updater processes to end, including the server process holding
// the prefs lock.
void WaitForUpdaterExit(UpdaterScope /*scope*/) {
  WaitFor(base::BindRepeating([]() { return !IsUpdaterRunning(); }));
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

#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
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
#endif  // BUILDFLAG(GOOGLE_CHROME_BRANDING)
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

HRESULT InitializeBundle(UpdaterScope scope,
                         Microsoft::WRL::ComPtr<IAppBundleWeb>& bundle_web) {
  Microsoft::WRL::ComPtr<IGoogleUpdate3Web> update3web;
  EXPECT_HRESULT_SUCCEEDED(::CoCreateInstance(
      scope == UpdaterScope::kSystem ? __uuidof(GoogleUpdate3WebSystemClass)
                                     : __uuidof(GoogleUpdate3WebUserClass),
      nullptr, CLSCTX_LOCAL_SERVER, IID_PPV_ARGS(&update3web)));

  Microsoft::WRL::ComPtr<IAppBundleWeb> bundle;
  Microsoft::WRL::ComPtr<IDispatch> dispatch;
  EXPECT_HRESULT_SUCCEEDED(update3web->createAppBundleWeb(&dispatch));
  EXPECT_HRESULT_SUCCEEDED(dispatch.As(&bundle));

  EXPECT_HRESULT_SUCCEEDED(bundle->initialize());

  bundle_web = bundle;
  return S_OK;
}

HRESULT DoLoopUntilDone(Microsoft::WRL::ComPtr<IAppBundleWeb> bundle,
                        int expected_final_state,
                        HRESULT expected_error_code) {
  bool done = false;
  static const base::TimeDelta kExpirationTimeout = base::Minutes(1);
  base::ElapsedTimer timer;

  EXPECT_TRUE(timer.Elapsed() < kExpirationTimeout);

  LONG state_value = 0;
  LONG error_code = 0;
  while (!done && (timer.Elapsed() < kExpirationTimeout)) {
    EXPECT_TRUE(bundle);

    Microsoft::WRL::ComPtr<IDispatch> app_dispatch;
    EXPECT_HRESULT_SUCCEEDED(bundle->get_appWeb(0, &app_dispatch));
    Microsoft::WRL::ComPtr<IAppWeb> app;
    EXPECT_HRESULT_SUCCEEDED(app_dispatch.As(&app));

    Microsoft::WRL::ComPtr<IDispatch> state_dispatch;
    EXPECT_HRESULT_SUCCEEDED(app->get_currentState(&state_dispatch));
    Microsoft::WRL::ComPtr<ICurrentState> state;
    EXPECT_HRESULT_SUCCEEDED(state_dispatch.As(&state));

    std::wstring stateDescription;
    std::wstring extraData;

    EXPECT_HRESULT_SUCCEEDED(state->get_stateValue(&state_value));

    switch (state_value) {
      case STATE_INIT:
        stateDescription = L"Initializating...";
        break;

      case STATE_WAITING_TO_CHECK_FOR_UPDATE:
      case STATE_CHECKING_FOR_UPDATE: {
        stateDescription = L"Checking for update...";
        break;
      }

      case STATE_UPDATE_AVAILABLE: {
        stateDescription = L"Update available!";
        EXPECT_HRESULT_SUCCEEDED(bundle->download());
        break;
      }

      case STATE_WAITING_TO_DOWNLOAD:
      case STATE_RETRYING_DOWNLOAD:
        stateDescription = L"Contacting server...";
        break;

      case STATE_DOWNLOADING: {
        stateDescription = L"Downloading...";

        ULONG bytes_downloaded = 0;
        state->get_bytesDownloaded(&bytes_downloaded);

        ULONG total_bytes_to_download = 0;
        state->get_totalBytesToDownload(&total_bytes_to_download);

        LONG download_time_remaining_ms = 0;
        state->get_downloadTimeRemainingMs(&download_time_remaining_ms);

        extraData = base::StringPrintf(
            L"[Bytes downloaded: %d][Bytes total: %d][Time remaining: %d]",
            bytes_downloaded, total_bytes_to_download,
            download_time_remaining_ms);
        break;
      }

      case STATE_DOWNLOAD_COMPLETE:
      case STATE_EXTRACTING:
      case STATE_APPLYING_DIFFERENTIAL_PATCH:
      case STATE_READY_TO_INSTALL: {
        stateDescription = L"Download completed!";
        ULONG bytes_downloaded = 0;
        state->get_bytesDownloaded(&bytes_downloaded);

        ULONG total_bytes_to_download = 0;
        state->get_totalBytesToDownload(&total_bytes_to_download);

        extraData =
            base::StringPrintf(L"[Bytes downloaded: %d][Bytes total: %d]",
                               bytes_downloaded, total_bytes_to_download);

        EXPECT_HRESULT_SUCCEEDED(bundle->install());

        break;
      }

      case STATE_WAITING_TO_INSTALL:
      case STATE_INSTALLING: {
        stateDescription = L"Installing...";

        LONG install_progress = 0;
        state->get_installProgress(&install_progress);
        LONG install_time_remaining_ms = 0;
        state->get_installTimeRemainingMs(&install_time_remaining_ms);

        extraData =
            base::StringPrintf(L"[Install Progress: %d][Time remaining: %d]",
                               install_progress, install_time_remaining_ms);
        break;
      }

      case STATE_INSTALL_COMPLETE:
        stateDescription = L"Done!";
        done = true;
        break;

      case STATE_PAUSED:
        stateDescription = L"Paused...";
        break;

      case STATE_NO_UPDATE:
        stateDescription = L"No update available!";
        done = true;
        break;

      case STATE_ERROR: {
        stateDescription = L"Error!";

        EXPECT_HRESULT_SUCCEEDED(state->get_errorCode(&error_code));

        base::win::ScopedBstr completion_message;
        EXPECT_HRESULT_SUCCEEDED(
            state->get_completionMessage(completion_message.Receive()));

        LONG installer_result_code = 0;
        EXPECT_HRESULT_SUCCEEDED(
            state->get_installerResultCode(&installer_result_code));

        extraData = base::StringPrintf(
            L"[errorCode: %d][completionMessage: %ls][installerResultCode: %d]",
            error_code, completion_message.Get(), installer_result_code);
        done = true;
        break;
      }

      default:
        stateDescription = L"Unhandled state...";
        break;
    }

    // TODO(1245992): Remove this logging once we get some confidence that the
    // code is working correctly and no further debugging is needed.
    LOG(ERROR) << base::StringPrintf(L"[State: %d][%ls][%ls]\n", state_value,
                                     stateDescription.c_str(),
                                     extraData.c_str());
    ::Sleep(100);
  }

  EXPECT_TRUE(done);
  EXPECT_EQ(expected_final_state, state_value);
  EXPECT_EQ(expected_error_code, error_code);

  return S_OK;
}

HRESULT DoUpdate(UpdaterScope scope,
                 const base::win::ScopedBstr& appid,
                 int expected_final_state,
                 HRESULT expected_error_code) {
  Microsoft::WRL::ComPtr<IAppBundleWeb> bundle;
  EXPECT_HRESULT_SUCCEEDED(InitializeBundle(scope, bundle));
  EXPECT_HRESULT_SUCCEEDED(bundle->createInstalledApp(appid.Get()));
  EXPECT_HRESULT_SUCCEEDED(bundle->checkForUpdate());
  return DoLoopUntilDone(bundle, expected_final_state, expected_error_code);
}

void ExpectLegacyUpdate3WebSucceeds(UpdaterScope scope,
                                    const std::string& app_id,
                                    int expected_final_state,
                                    int expected_error_code) {
  EXPECT_HRESULT_SUCCEEDED(
      DoUpdate(scope, base::win::ScopedBstr(base::UTF8ToWide(app_id).c_str()),
               expected_final_state, expected_error_code));
}

void SetFcLaunchCmd(const std::wstring& id) {
  base::win::RegKey key;
  ASSERT_EQ(key.Create(HKEY_LOCAL_MACHINE, GetAppClientsKey(id).c_str(),
                       Wow6432(KEY_WRITE)),
            ERROR_SUCCESS);
  EXPECT_EQ(key.WriteValue(L"fc", L"fc /?"), ERROR_SUCCESS);
}

void DeleteFcLaunchCmd(const std::wstring& id) {
  base::win::RegKey key;
  ASSERT_EQ(key.Create(HKEY_LOCAL_MACHINE, GetAppClientsKey(id).c_str(),
                       Wow6432(KEY_WRITE)),
            ERROR_SUCCESS);
  EXPECT_EQ(key.DeleteValue(L"fc"), ERROR_SUCCESS);
}

void ExpectLegacyProcessLauncherSucceeds(UpdaterScope scope) {
  // ProcessLauncher is only implemented for kSystem at the moment.
  if (scope != UpdaterScope::kSystem)
    return;

  Microsoft::WRL::ComPtr<IProcessLauncher> process_launcher;
  EXPECT_HRESULT_SUCCEEDED(::CoCreateInstance(__uuidof(ProcessLauncherClass),
                                              nullptr, CLSCTX_LOCAL_SERVER,
                                              IID_PPV_ARGS(&process_launcher)));
  EXPECT_TRUE(process_launcher);

  const wchar_t* const kAppId1 = L"{831EF4D0-B729-4F61-AA34-91526481799D}";
  ULONG_PTR proc_handle = 0;
  DWORD caller_proc_id = ::GetCurrentProcessId();

  // Succeeds when the command is present in the registry.
  SetFcLaunchCmd(kAppId1);
  EXPECT_HRESULT_SUCCEEDED(process_launcher->LaunchCmdElevated(
      kAppId1, _T("fc"), caller_proc_id, &proc_handle));
  EXPECT_NE(static_cast<ULONG_PTR>(0), proc_handle);

  HANDLE handle = reinterpret_cast<HANDLE>(proc_handle);
  EXPECT_NE(WAIT_FAILED, ::WaitForSingleObject(handle, 10000));
  EXPECT_TRUE(::CloseHandle(handle));
  DeleteFcLaunchCmd(kAppId1);

  // Returns HRESULT_FROM_WIN32(ERROR_NOT_FOUND) when the command is missing in
  // the registry.
  EXPECT_EQ(HRESULT_FROM_WIN32(ERROR_NOT_FOUND),
            process_launcher->LaunchCmdElevated(kAppId1, _T("fc"),
                                                caller_proc_id, &proc_handle));
  EXPECT_EQ(static_cast<ULONG_PTR>(0), proc_handle);
}

int RunVPythonCommand(const base::CommandLine& command_line) {
  base::CommandLine python_command = command_line;
  python_command.PrependWrapper(FILE_PATH_LITERAL("vpython3.bat"));

  int exit_code = -1;
  base::Process process = base::LaunchProcess(python_command, {});
  EXPECT_TRUE(process.IsValid());
  EXPECT_TRUE(process.WaitForExitWithTimeout(base::Seconds(60), &exit_code));
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

void InvokeTestServiceFunction(
    const std::string& function_name,
    const base::flat_map<std::string, base::Value>& arguments) {
  std::string arguments_json_string;
  EXPECT_TRUE(
      base::JSONWriter::Write(base::Value(arguments), &arguments_json_string));

  base::FilePath path(base::CommandLine::ForCurrentProcess()->GetProgram());
  path = path.DirName();
  path = MakeAbsoluteFilePath(path);
  path = path.Append(FILE_PATH_LITERAL("test_service"))
             .Append(FILE_PATH_LITERAL("service_client.py"));
  EXPECT_TRUE(base::PathExists(path));

  base::CommandLine command(path);
  command.AppendSwitchASCII("--function", function_name);
  command.AppendSwitchASCII("--args", arguments_json_string);
  EXPECT_EQ(RunVPythonCommand(command), 0);
}

void SetupRealUpdaterLowerVersion(UpdaterScope scope) {
  base::FilePath source_path;
  ASSERT_TRUE(base::PathService::Get(base::DIR_SOURCE_ROOT, &source_path));
  base::FilePath old_updater_path =
      source_path.Append(FILE_PATH_LITERAL("third_party"))
          .Append(FILE_PATH_LITERAL("updater"));
#if BUILDFLAG(CHROMIUM_BRANDING)
#if defined(ARCH_CPU_X86_64)
  old_updater_path =
      old_updater_path.Append(FILE_PATH_LITERAL("chromium_win_x86_64"));
#elif defined(ARCH_CPU_X86)
  old_updater_path =
      old_updater_path.Append(FILE_PATH_LITERAL("chromium_win_x86"));
#endif
#elif BUILDFLAG(GOOGLE_CHROME_BRANDING)
#if defined(ARCH_CPU_X86_64)
  old_updater_path =
      old_updater_path.Append(FILE_PATH_LITERAL("chrome_win_x86_64"));
#elif defined(ARCH_CPU_X86)
  old_updater_path =
      old_updater_path.Append(FILE_PATH_LITERAL("chrome_win_x86"));
#endif
#endif
  base::CommandLine command_line(
      old_updater_path.Append(FILE_PATH_LITERAL("UpdaterSetup_test.exe")));
  command_line.AppendSwitch(kInstallSwitch);
  int exit_code = -1;
  ASSERT_TRUE(Run(scope, command_line, &exit_code));
  ASSERT_EQ(exit_code, 0);
}

void RunUninstallCmdLine(UpdaterScope scope) {
  std::wstring uninstall_cmd_line_string;
  EXPECT_EQ(ERROR_SUCCESS, base::win::RegKey(UpdaterScopeToHKeyRoot(scope),
                                             UPDATER_KEY, Wow6432(KEY_READ))
                               .ReadValue(kRegValueUninstallCmdLine,
                                          &uninstall_cmd_line_string));
  base::CommandLine command_line =
      base::CommandLine::FromString(uninstall_cmd_line_string);

  base::ScopedAllowBaseSyncPrimitivesForTesting allow_wait_process;

  base::Process process = base::LaunchProcess(command_line, {});
  EXPECT_TRUE(process.IsValid());

  int exit_code = 0;
  EXPECT_TRUE(process.WaitForExitWithTimeout(base::Seconds(45), &exit_code));
  EXPECT_EQ(0, exit_code);
}

void SetupFakeLegacyUpdaterData(UpdaterScope scope) {
  const HKEY root = UpdaterScopeToHKeyRoot(scope);

  base::win::RegKey key;
  ASSERT_EQ(
      key.Create(root, GetAppClientsKey(kLegacyGoogleUpdaterAppID).c_str(),
                 Wow6432(KEY_WRITE)),
      ERROR_SUCCESS);
  ASSERT_EQ(key.WriteValue(kRegValuePV, L"1.1.1.1"), ERROR_SUCCESS);
  ASSERT_EQ(key.WriteValue(kRegValueBrandCode, L"GOOG"), ERROR_SUCCESS);
  ASSERT_EQ(key.WriteValue(kRegValueAP, L"TestAP"), ERROR_SUCCESS);
  key.Close();

  ASSERT_EQ(
      key.Create(
          root,
          GetAppClientsKey(L"{8A69D345-D564-463C-AFF1-A69D9E530F96}").c_str(),
          Wow6432(KEY_WRITE)),
      ERROR_SUCCESS);
  ASSERT_EQ(key.WriteValue(kRegValuePV, L"99.0.0.1"), ERROR_SUCCESS);
  ASSERT_EQ(key.WriteValue(kRegValueBrandCode, L"GGLS"), ERROR_SUCCESS);
  ASSERT_EQ(key.WriteValue(kRegValueAP, L"TestAP"), ERROR_SUCCESS);
  key.Close();

  ASSERT_EQ(
      key.Create(
          root,
          GetAppClientsKey(L"{fc54d8f9-b6fd-4274-92eb-c4335cd8761e}").c_str(),
          Wow6432(KEY_WRITE)),
      ERROR_SUCCESS);
  ASSERT_EQ(key.WriteValue(kRegValueBrandCode, L"GGLS"), ERROR_SUCCESS);
  ASSERT_EQ(key.WriteValue(kRegValueAP, L"TestAP"), ERROR_SUCCESS);
  key.Close();
}

void ExpectLegacyUpdaterDataMigrated(UpdaterScope scope) {
  scoped_refptr<GlobalPrefs> global_prefs = CreateGlobalPrefs(scope);
  auto persisted_data =
      base::MakeRefCounted<PersistedData>(global_prefs->GetPrefService());

  // Legacy updater itself should not be migrated.
  const std::string kLegacyUpdaterAppId =
      base::SysWideToUTF8(kLegacyGoogleUpdaterAppID);
  EXPECT_FALSE(
      persisted_data->GetProductVersion(kLegacyUpdaterAppId).IsValid());
  EXPECT_TRUE(persisted_data->GetAP(kLegacyUpdaterAppId).empty());
  EXPECT_TRUE(persisted_data->GetBrandCode(kLegacyUpdaterAppId).empty());
  EXPECT_TRUE(persisted_data->GetFingerprint(kLegacyUpdaterAppId).empty());

  // App without 'pv' should not be migrated.
  const std::string kNoPVAppId("{fc54d8f9-b6fd-4274-92eb-c4335cd8761e}");
  EXPECT_FALSE(persisted_data->GetProductVersion(kNoPVAppId).IsValid());
  EXPECT_TRUE(persisted_data->GetAP(kNoPVAppId).empty());
  EXPECT_TRUE(persisted_data->GetBrandCode(kNoPVAppId).empty());
  EXPECT_TRUE(persisted_data->GetFingerprint(kNoPVAppId).empty());

  const std::string kChromeAppId = "{8A69D345-D564-463C-AFF1-A69D9E530F96}";
  EXPECT_EQ(persisted_data->GetProductVersion(kChromeAppId),
            base::Version("99.0.0.1"));
  EXPECT_EQ(persisted_data->GetAP(kChromeAppId), "TestAP");
  EXPECT_EQ(persisted_data->GetBrandCode(kChromeAppId), "GGLS");
  EXPECT_TRUE(persisted_data->GetFingerprint(kChromeAppId).empty());
}

void InstallApp(UpdaterScope scope, const std::string& app_id) {
  base::win::RegKey key;
  ASSERT_EQ(
      key.Create(
          UpdaterScopeToHKeyRoot(scope),
          base::StrCat({CLIENTS_KEY, base::SysUTF8ToWide(app_id)}).c_str(),
          Wow6432(KEY_WRITE)),
      ERROR_SUCCESS);
  RegisterApp(scope, app_id);
}

void UninstallApp(UpdaterScope scope, const std::string& app_id) {
  base::win::RegKey key;
  ASSERT_EQ(
      key.Open(UpdaterScopeToHKeyRoot(scope), CLIENTS_KEY, Wow6432(KEY_WRITE)),
      ERROR_SUCCESS);
  ASSERT_EQ(key.DeleteKey(base::SysUTF8ToWide(app_id).c_str()), ERROR_SUCCESS);
}

}  // namespace test
}  // namespace updater
