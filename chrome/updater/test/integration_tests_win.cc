// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <shlobj.h>
#include <wrl/client.h>
#include <wrl/implements.h>

#include <regstr.h>

#include <iostream>
#include <memory>
#include <string>
#include <vector>

#include "base/base_paths.h"
#include "base/command_line.h"
#include "base/containers/contains.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/functional/callback_helpers.h"
#include "base/json/json_writer.h"
#include "base/logging.h"
#include "base/path_service.h"
#include "base/process/launch.h"
#include "base/process/process.h"
#include "base/ranges/algorithm.h"
#include "base/strings/strcat.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/sys_string_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/synchronization/waitable_event.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/test/bind.h"
#include "base/test/test_timeouts.h"
#include "base/threading/platform_thread.h"
#include "base/time/time.h"
#include "base/timer/elapsed_timer.h"
#include "base/values.h"
#include "base/version.h"
#include "base/win/registry.h"
#include "base/win/scoped_bstr.h"
#include "base/win/scoped_variant.h"
#include "base/win/win_util.h"
#include "build/branding_buildflags.h"
#include "build/build_config.h"
#include "chrome/updater/app/server/win/com_classes.h"
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
#include "chrome/updater/util/unittest_util.h"
#include "chrome/updater/util/unittest_util_win.h"
#include "chrome/updater/util/util.h"
#include "chrome/updater/util/win_util.h"
#include "chrome/updater/win/setup/setup_util.h"
#include "chrome/updater/win/task_scheduler.h"
#include "chrome/updater/win/test/test_executables.h"
#include "chrome/updater/win/test/test_strings.h"
#include "chrome/updater/win/ui/l10n_util.h"
#include "chrome/updater/win/ui/resources/updater_installer_strings.h"
#include "chrome/updater/win/win_constants.h"
#include "components/crx_file/crx_verifier.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "url/gurl.h"

namespace updater::test {
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

// Creates an instance of the class specified by `clsid` in a local server.
template <typename ComInterface>
HRESULT CreateLocalServer(GUID clsid,
                          Microsoft::WRL::ComPtr<ComInterface>& server) {
  // crbug.com/1259178 - there is known race condition between the COM server
  // shutdown and server start up.
  base::PlatformThread::Sleep(kCreateUpdaterInstanceDelay);
  return ::CoCreateInstance(clsid, nullptr, CLSCTX_LOCAL_SERVER,
                            IID_PPV_ARGS(&server));
}

// Returns the root directory where the updater product is installed. This
// is the parent directory where the versioned directories of the
// updater instances are.
absl::optional<base::FilePath> GetProductPath(UpdaterScope scope) {
  base::FilePath app_data_dir;
  if (!base::PathService::Get(IsSystemInstall(scope) ? base::DIR_PROGRAM_FILES
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

[[nodiscard]] bool RegKeyExists(HKEY root, const std::wstring& path) {
  return base::win::RegKey(root, path.c_str(), Wow6432(KEY_QUERY_VALUE))
      .Valid();
}

[[nodiscard]] bool RegKeyExistsCOM(HKEY root, const std::wstring& path) {
  return base::win::RegKey(root, path.c_str(), KEY_QUERY_VALUE).Valid();
}

[[nodiscard]] bool DeleteRegKey(HKEY root, const std::wstring& path) {
  LONG result =
      base::win::RegKey(root, L"", Wow6432(KEY_READ)).DeleteKey(path.c_str());
  return result == ERROR_SUCCESS || result == ERROR_FILE_NOT_FOUND;
}

[[nodiscard]] bool DeleteRegKeyCOM(HKEY root, const std::wstring& path) {
  LONG result = base::win::RegKey(root, L"", KEY_READ).DeleteKey(path.c_str());
  return result == ERROR_SUCCESS || result == ERROR_FILE_NOT_FOUND;
}

[[nodiscard]] bool DeleteRegValue(HKEY root,
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

[[nodiscard]] bool DeleteService(const std::wstring& service_name) {
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
  ::CloseServiceHandle(scm);

  if (!DeleteRegValue(HKEY_LOCAL_MACHINE, UPDATER_KEY, service_name)) {
    return false;
  }

  return is_service_deleted;
}

[[nodiscard]] bool IsServiceGone(const std::wstring& service_name) {
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
    for (const wchar_t* key : {CLIENTS_KEY, UPDATER_KEY}) {
      EXPECT_EQ(is_installed, RegKeyExists(root, key));
    }

    EXPECT_EQ(is_installed, base::PathExists(*GetGoogleUpdateExePath(scope)));

    if (is_installed) {
      EXPECT_TRUE(RegKeyExists(root, CLIENT_STATE_KEY));

      std::wstring pv;
      EXPECT_EQ(ERROR_SUCCESS,
                base::win::RegKey(
                    root,
                    GetAppClientsKey(L"{430FD4D0-B729-4F61-AA34-91526481799D}")
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
                      .HasSwitch(kWakeSwitch));

      if (!IsSystemInstall(scope)) {
        std::wstring run_updater_wake_command;
        EXPECT_EQ(ERROR_SUCCESS,
                  base::win::RegKey(root, REGSTR_PATH_RUN, KEY_READ)
                      .ReadValue(GetTaskNamePrefix(scope).c_str(),
                                 &run_updater_wake_command));
        EXPECT_TRUE(base::CommandLine::FromString(run_updater_wake_command)
                        .HasSwitch(kWakeSwitch));
      }
    } else {
      if (::IsUserAnAdmin()) {
        for (const wchar_t* key :
             {kRegKeyCompanyCloudManagement, kRegKeyCompanyEnrollment,
              UPDATER_POLICIES_KEY}) {
          EXPECT_FALSE(RegKeyExists(HKEY_LOCAL_MACHINE, key));
        }
      }

      EXPECT_FALSE(RegKeyExists(root, UPDATER_KEY));

      if (!IsSystemInstall(scope)) {
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
    if (IsSystemInstall(scope)) {
      EXPECT_EQ(is_installed,
                RegKeyExistsCOM(root, GetComServerAppidRegistryPath(clsid)));
    }
  }

  for (const IID& iid :
       JoinVectors(GetSideBySideInterfaces(scope),
                   is_active_and_sxs ? GetActiveInterfaces(scope)
                                     : std::vector<IID>())) {
    EXPECT_EQ(is_installed, RegKeyExistsCOM(root, GetComIidRegistryPath(iid)));
    EXPECT_EQ(is_installed,
              RegKeyExistsCOM(root, GetComTypeLibRegistryPath(iid)));
  }

  if (IsSystemInstall(scope)) {
    for (const bool is_internal_service : {false, true}) {
      if (!is_active_and_sxs && !is_internal_service)
        continue;

      EXPECT_EQ(is_installed,
                !IsServiceGone(GetServiceName(is_internal_service)));
    }
  }

  if (is_installed) {
    std::unique_ptr<TaskScheduler> task_scheduler =
        TaskScheduler::CreateInstance(scope);
    const std::wstring task_name =
        task_scheduler->FindFirstTaskName(GetTaskNamePrefix(scope));
    EXPECT_TRUE(!task_name.empty());
    EXPECT_TRUE(task_scheduler->IsTaskRegistered(task_name.c_str()));

    TaskScheduler::TaskInfo task_info;
    ASSERT_TRUE(task_scheduler->GetTaskInfo(task_name.c_str(), &task_info));
    ASSERT_EQ(task_info.exec_actions.size(), 1u);
    EXPECT_STREQ(
        task_info.exec_actions[0].arguments.c_str(),
        base::StrCat({L"--wake ", IsSystemInstall(scope) ? L"--system " : L"",
                      L"--enable-logging "
                      L"--vmodule=*/components/winhttp/*=2,"
                      L"*/components/update_client/*=2,"
                      L"*/chrome/updater/*=2"})
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
    EXPECT_TRUE(WaitFor(base::BindLambdaForTesting([&]() {
                          return is_installed == base::PathExists(*path);
                        }),
                        base::BindLambdaForTesting([&]() {
                          VLOG(0) << "Still waiting for " << *path
                                  << " where is_installed=" << is_installed;
                        })));
  }
}

// Returns true if any updater process is found running in any session in the
// system, regardless of its path.
bool IsUpdaterRunning() {
  return test::IsProcessRunning(GetExecutableRelativePath().value());
}

void SleepFor(const base::TimeDelta& interval) {
  VLOG(2) << "Sleeping " << interval.InSecondsF() << " seconds...";
  base::PlatformThread::Sleep(interval);
  VLOG(2) << "Sleep complete.";
}

void SetupAppCommand(UpdaterScope scope,
                     const std::wstring& app_id,
                     const std::wstring& command_id,
                     const std::wstring& parameters,
                     base::ScopedTempDir& temp_dir) {
  base::CommandLine cmd_exe_command_line(base::CommandLine::NO_PROGRAM);
  SetupCmdExe(scope, cmd_exe_command_line, temp_dir);
  CreateAppCommandRegistry(
      scope, app_id, command_id,
      base::StrCat({cmd_exe_command_line.GetCommandLineString(), parameters}));
}

base::Process LaunchOfflineInstallProcess(bool is_legacy_install,
                                          const base::FilePath& exe_path,
                                          UpdaterScope install_scope,
                                          const std::wstring& app_id,
                                          const std::wstring& offline_dir_guid,
                                          bool is_silent_install) {
  auto launch_legacy_offline_install = [&]() -> base::Process {
    auto build_legacy_switch =
        [](const std::string& switch_name) -> std::wstring {
      return base::ASCIIToWide(base::StrCat({"/", switch_name}));
    };
    std::vector<std::wstring> install_cmd_args = {
        base::CommandLine::QuoteForCommandLineToArgvW(exe_path.value()),

        build_legacy_switch(updater::kEnableLoggingSwitch),

        // This switch and its value must be connected by '=' because logging
        // switch does not support legacy format.
        base::StrCat({build_legacy_switch(updater::kLoggingModuleSwitch), L"=",
                      base::ASCIIToWide(updater::kLoggingModuleSwitchValue)}),

        IsSystemInstall(install_scope)
            ? build_legacy_switch(updater::kSystemSwitch)
            : L"",

        build_legacy_switch(updater::kHandoffSwitch),
        base::StrCat({L"\"appguid=", app_id, L"&lang=en\""}),

        build_legacy_switch(updater::kSessionIdSwitch),
        L"{E85204C6-6F2F-40BF-9E6C-4952208BB977}",

        build_legacy_switch(updater::kOfflineDirSwitch),
        base::CommandLine::QuoteForCommandLineToArgvW(offline_dir_guid),

        is_silent_install ? build_legacy_switch(updater::kSilentSwitch) : L"",
    };

    return base::LaunchProcess(base::JoinString(install_cmd_args, L" "), {});
  };

  auto launch_offline_install = [&]() -> base::Process {
    base::CommandLine install_cmd(exe_path);

    install_cmd.AppendSwitch(kEnableLoggingSwitch);
    install_cmd.AppendSwitchASCII(kLoggingModuleSwitch,
                                  kLoggingModuleSwitchValue);
    if (IsSystemInstall(install_scope))
      install_cmd.AppendSwitch(kSystemSwitch);

    install_cmd.AppendSwitchNative(
        updater::kHandoffSwitch,
        base::StrCat({L"appguid=", app_id, L"&lang=en"}));
    install_cmd.AppendSwitchASCII(updater::kSessionIdSwitch,
                                  "{E85204C6-6F2F-40BF-9E6C-4952208BB977}");
    install_cmd.AppendSwitchNative(updater::kOfflineDirSwitch,
                                   offline_dir_guid);
    if (is_silent_install)
      install_cmd.AppendSwitch(updater::kSilentSwitch);

    return base::LaunchProcess(install_cmd, {});
  };

  return is_legacy_install ? launch_legacy_offline_install()
                           : launch_offline_install();
}

class WindowEnumerator {
 public:
  WindowEnumerator(HWND parent,
                   base::RepeatingCallback<bool(HWND hwnd)> filter,
                   base::RepeatingCallback<void(HWND hwnd)> action)
      : parent_(parent), filter_(filter), action_(action) {}

  WindowEnumerator(const WindowEnumerator&) = delete;
  WindowEnumerator& operator=(const WindowEnumerator&) = delete;

  void Run() const {
    ::EnumChildWindows(parent_, &OnWindowProc, reinterpret_cast<LPARAM>(this));
  }

  static std::wstring GetWindowClass(HWND hwnd) {
    constexpr int kMaxWindowClassNameLength = 256;
    wchar_t buffer[kMaxWindowClassNameLength + 1] = {0};
    int name_len = ::GetClassName(hwnd, buffer, std::size(buffer));
    if (name_len <= 0 || name_len > kMaxWindowClassNameLength)
      return std::wstring();

    return std::wstring(&buffer[0], name_len);
  }

  static bool IsSystemDialog(HWND hwnd) {
    constexpr wchar_t kSystemDialogClass[] = L"#32770";
    return GetWindowClass(hwnd) == kSystemDialogClass;
  }

  static std::wstring GetWindowText(HWND hwnd) {
    const int num_chars = ::GetWindowTextLength(hwnd);
    if (!num_chars)
      return std::wstring();
    std::vector<wchar_t> text(num_chars + 1);
    if (!::GetWindowText(hwnd, &text.front(), text.size()))
      return std::wstring();
    return std::wstring(text.begin(), text.end());
  }

 private:
  bool OnWindow(HWND hwnd) const {
    if (filter_.Run(hwnd))
      action_.Run(hwnd);

    // Returns true to keep enumerating.
    return true;
  }

  static BOOL CALLBACK OnWindowProc(HWND hwnd, LPARAM lparam) {
    return reinterpret_cast<WindowEnumerator*>(lparam)->OnWindow(hwnd);
  }

  const HWND parent_;
  base::RepeatingCallback<bool(HWND hwnd)> filter_;
  base::RepeatingCallback<void(HWND hwnd)> action_;
};

DISPID GetDispId(Microsoft::WRL::ComPtr<IDispatch> dispatch,
                 std::wstring name) {
  DISPID id = 0;
  LPOLESTR name_ptr = &name[0];
  EXPECT_HRESULT_SUCCEEDED(dispatch->GetIDsOfNames(IID_NULL, &name_ptr, 1,
                                                   LOCALE_USER_DEFAULT, &id));
  VLOG(2) << __func__ << ": " << name << ": " << id;
  return id;
}

void CallDispatchMethod(
    Microsoft::WRL::ComPtr<IDispatch> dispatch,
    const std::wstring& method_name,
    const std::vector<base::win::ScopedVariant>& variant_params) {
  std::vector<VARIANT> params;
  params.reserve(variant_params.size());

  // IDispatch::Invoke() expects the parameters in reverse order.
  std::transform(variant_params.rbegin(), variant_params.rend(),
                 std::back_inserter(params),
                 [](const auto& param) { return param.Copy(); });

  DISPPARAMS dp = {};
  if (!params.empty()) {
    dp.rgvarg = &params[0];
    dp.cArgs = params.size();
  }

  EXPECT_HRESULT_SUCCEEDED(dispatch->Invoke(
      GetDispId(dispatch, method_name), IID_NULL, LOCALE_USER_DEFAULT,
      DISPATCH_METHOD, &dp, nullptr, nullptr, nullptr));

  base::ranges::for_each(params, [&](auto& param) { ::VariantClear(&param); });
  return;
}

base::win::ScopedVariant GetDispatchProperty(
    Microsoft::WRL::ComPtr<IDispatch> dispatch,
    const std::wstring& property_name) {
  DISPPARAMS dp = {};
  base::win::ScopedVariant result;

  EXPECT_HRESULT_SUCCEEDED(dispatch->Invoke(
      GetDispId(dispatch, property_name), IID_NULL, LOCALE_USER_DEFAULT,
      DISPATCH_PROPERTYGET, &dp, result.Receive(), nullptr, nullptr));

  return result;
}

}  // namespace

base::FilePath GetSetupExecutablePath() {
  base::FilePath out_dir;
  if (!base::PathService::Get(base::DIR_EXE, &out_dir))
    return base::FilePath();
  return out_dir.AppendASCII("UpdaterSetup_test.exe");
}

absl::optional<base::FilePath> GetInstalledExecutablePath(UpdaterScope scope) {
  absl::optional<base::FilePath> path = GetProductVersionPath(scope);
  if (!path)
    return absl::nullopt;
  return path->Append(GetExecutableRelativePath());
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
  VLOG(0) << __func__;

  CleanProcesses();

  const HKEY root = UpdaterScopeToHKeyRoot(scope);
  for (const wchar_t* key : {CLIENT_STATE_KEY, CLIENTS_KEY, UPDATER_KEY}) {
    EXPECT_TRUE(DeleteRegKey(root, key));
  }

  if (::IsUserAnAdmin()) {
    for (const wchar_t* key :
         {kRegKeyCompanyCloudManagement, kRegKeyCompanyEnrollment,
          UPDATER_POLICIES_KEY}) {
      EXPECT_TRUE(DeleteRegKey(HKEY_LOCAL_MACHINE, key));
    }
  }

  for (const CLSID& clsid :
       JoinVectors(GetSideBySideServers(scope), GetActiveServers(scope))) {
    EXPECT_TRUE(DeleteRegKeyCOM(root, GetComServerClsidRegistryPath(clsid)));
    if (IsSystemInstall(scope))
      EXPECT_TRUE(DeleteRegKeyCOM(root, GetComServerAppidRegistryPath(clsid)));
  }

  for (const IID& iid : JoinVectors(GetSideBySideInterfaces(scope),
                                    GetActiveInterfaces(scope))) {
    EXPECT_TRUE(DeleteRegKeyCOM(root, GetComIidRegistryPath(iid)));
    EXPECT_TRUE(DeleteRegKeyCOM(root, GetComTypeLibRegistryPath(iid)));
  }

  if (!IsSystemInstall(scope)) {
    base::win::RegKey(root, REGSTR_PATH_RUN, KEY_WRITE)
        .DeleteValue(GetTaskNamePrefix(scope).c_str());
  }

  if (IsSystemInstall(scope)) {
    for (const bool is_internal_service : {true, false}) {
      EXPECT_TRUE(DeleteService(GetServiceName(is_internal_service)));
    }
  }

  std::unique_ptr<TaskScheduler> task_scheduler =
      TaskScheduler::CreateInstance(scope);
  const std::wstring task_name =
      task_scheduler->FindFirstTaskName(GetTaskNamePrefix(scope));
  if (!task_name.empty())
    task_scheduler->DeleteTask(task_name.c_str());
  EXPECT_TRUE(
      task_scheduler->FindFirstTaskName(GetTaskNamePrefix(scope)).empty());

  const absl::optional<base::FilePath> target_path =
      GetGoogleUpdateExePath(scope);
  if (target_path)
    base::DeleteFile(*target_path);

  absl::optional<base::FilePath> path = GetProductPath(scope);
  ASSERT_TRUE(path);
  ASSERT_TRUE(base::DeletePathRecursively(*path)) << *path;

  // TODO(crbug.com/1401759) - this can be removed after the crbug is closed.
  VLOG(0) << __func__ << " end.";
}

void EnterTestMode(const GURL& url) {
  ASSERT_TRUE(ExternalConstantsBuilder()
                  .SetUpdateURL(std::vector<std::string>{url.spec()})
                  .SetUseCUP(false)
                  .SetInitialDelay(base::Milliseconds(100))
                  .SetCrxVerifierFormat(crx_file::VerifierFormat::CRX3)
                  .SetOverinstallTimeout(base::Seconds(11))
                  .Modify());
}

void ExpectInstalled(UpdaterScope scope) {
  CheckInstallation(scope, CheckInstallationStatus::kCheckIsInstalled,
                    CheckInstallationVersions::kCheckSxSOnly);
}

void ExpectClean(UpdaterScope scope) {
  ExpectCleanProcesses();
  CheckInstallation(scope, CheckInstallationStatus::kCheckIsNotInstalled,
                    CheckInstallationVersions::kCheckActiveAndSxS);
}

void ExpectCandidateUninstalled(UpdaterScope scope) {
  CheckInstallation(scope, CheckInstallationStatus::kCheckIsNotInstalled,
                    CheckInstallationVersions::kCheckSxSOnly);
}

void Uninstall(UpdaterScope scope) {
  // Note: "updater.exe --uninstall" is run from the build dir, not the install
  // dir, because it is useful for tests to be able to run it to clean the
  // system even if installation has failed or the installed binaries have
  // already been removed.
  base::FilePath path =
      GetSetupExecutablePath().DirName().Append(GetExecutableRelativePath());
  ASSERT_FALSE(path.empty());
  base::CommandLine command_line(path);
  command_line.AppendSwitch("uninstall");
  int exit_code = -1;
  Run(scope, command_line, &exit_code);

  // Uninstallation involves a race with the uninstall.cmd script and the
  // process exit. Sleep to allow the script to complete its work.
  // TODO(crbug.com/1217765): Figure out a way to replace this.
  SleepFor(base::Seconds(5));
  ASSERT_EQ(0, exit_code);
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
bool WaitForUpdaterExit(UpdaterScope /*scope*/) {
  return WaitFor(
      base::BindRepeating([]() { return !IsUpdaterRunning(); }),
      base::BindLambdaForTesting([]() {
        VLOG(0) << "Still waiting for updater to exit. "
                << test::PrintProcesses(GetExecutableRelativePath().value());
      }));
}

// Verify registry entries for all interfaces.
// IID entries under `Software\Classes\Interface`:
// * ProxyStubClsid32 entry should point to the OLE automation marshaler
// * TypeLib entry should be equal to the IID.
//
// TypeLib entries under `Software\Classes\TypeLib`:
// * Read the typelib path under both `win32` and `win64`.
// * Confirm that the typelib can be loaded using ::LoadTypeLib.
// * Confirm that the typeinfo for each interface can be loaded from the
// typelib.
void VerifyInterfacesRegistryEntries(UpdaterScope scope) {
  for (const auto is_internal : {true, false}) {
    for (const auto& iid : GetInterfaces(is_internal, scope)) {
      const HKEY root = UpdaterScopeToHKeyRoot(scope);
      const std::wstring iid_reg_path = GetComIidRegistryPath(iid);
      const std::wstring typelib_reg_path = GetComTypeLibRegistryPath(iid);
      const std::wstring iid_string = base::win::WStringFromGUID(iid);

      std::wstring val;
      {
        const auto& path = iid_reg_path + L"\\ProxyStubClsid32";
        EXPECT_EQ(base::win::RegKey(root, path.c_str(), KEY_READ)
                      .ReadValue(L"", &val),
                  ERROR_SUCCESS)
            << ": " << root << ": " << path << ": " << iid_string;
        EXPECT_EQ(val, L"{00020424-0000-0000-C000-000000000046}");
      }

      {
        const auto& path = iid_reg_path + L"\\TypeLib";
        EXPECT_EQ(base::win::RegKey(root, path.c_str(), KEY_READ)
                      .ReadValue(L"", &val),
                  ERROR_SUCCESS)
            << ": " << root << ": " << path << ": " << iid_string;
        EXPECT_EQ(val, iid_string);
      }

      const std::wstring typelib_reg_path_win32 =
          typelib_reg_path + L"\\1.0\\0\\win32";
      const std::wstring typelib_reg_path_win64 =
          typelib_reg_path + L"\\1.0\\0\\win64";

      for (const auto& path :
           {typelib_reg_path_win32, typelib_reg_path_win64}) {
        std::wstring typelib_path;
        EXPECT_EQ(base::win::RegKey(root, path.c_str(), KEY_READ)
                      .ReadValue(L"", &typelib_path),
                  ERROR_SUCCESS)
            << ": " << root << ": " << path << ": " << iid_string;

        Microsoft::WRL::ComPtr<ITypeLib> type_lib;
        EXPECT_HRESULT_SUCCEEDED(::LoadTypeLib(typelib_path.c_str(), &type_lib))
            << ": Typelib path: " << typelib_path;

        Microsoft::WRL::ComPtr<ITypeInfo> type_info;
        EXPECT_HRESULT_SUCCEEDED(type_lib->GetTypeInfoOfGuid(iid, &type_info))
            << ": Typelib path: " << typelib_path << ": IID: " << iid_string;
      }
    }
  }
}

// Tests if the typelibs and some of the public, internal, and
// legacy interfaces are available. Failure to query these interfaces indicates
// an issue with typelib registration.
void ExpectInterfacesRegistered(UpdaterScope scope) {
  {
    // IUpdater, IGoogleUpdate3Web and IAppBundleWeb.
    // The block is necessary so that updater_server goes out of scope and
    // releases the prefs lock before updater_internal_server tries to acquire
    // it to mode-check.
    Microsoft::WRL::ComPtr<IUnknown> updater_server;
    ASSERT_HRESULT_SUCCEEDED(
        CreateLocalServer(IsSystemInstall(scope) ? __uuidof(UpdaterSystemClass)
                                                 : __uuidof(UpdaterUserClass),
                          updater_server));
    Microsoft::WRL::ComPtr<IUpdater> updater;
    EXPECT_HRESULT_SUCCEEDED(
        updater_server.CopyTo(IsSystemInstall(scope) ? __uuidof(IUpdaterSystem)
                                                     : __uuidof(IUpdaterUser),
                              IID_PPV_ARGS_Helper(&updater)));

    for (const CLSID& clsid : [&scope]() -> std::vector<CLSID> {
           if (IsSystemInstall(scope)) {
             return {__uuidof(GoogleUpdate3WebSystemClass),
                     __uuidof(GoogleUpdate3WebServiceClass)};
           } else {
             return {__uuidof(GoogleUpdate3WebUserClass)};
           }
         }()) {
      Microsoft::WRL::ComPtr<IUnknown> updater_legacy_server;
      ASSERT_HRESULT_SUCCEEDED(CreateLocalServer(clsid, updater_legacy_server));
      Microsoft::WRL::ComPtr<IGoogleUpdate3Web> google_update;
      ASSERT_HRESULT_SUCCEEDED(updater_legacy_server.As(&google_update));
      Microsoft::WRL::ComPtr<IAppBundleWeb> app_bundle;
      Microsoft::WRL::ComPtr<IDispatch> dispatch;
      ASSERT_HRESULT_SUCCEEDED(google_update->createAppBundleWeb(&dispatch));
      EXPECT_HRESULT_SUCCEEDED(dispatch.As(&app_bundle));
    }
  }

  {
    // IUpdaterInternal.
    Microsoft::WRL::ComPtr<IUnknown> updater_internal_server;
    ASSERT_HRESULT_SUCCEEDED(CreateLocalServer(
        IsSystemInstall(scope) ? __uuidof(UpdaterInternalSystemClass)
                               : __uuidof(UpdaterInternalUserClass),
        updater_internal_server));
    Microsoft::WRL::ComPtr<IUpdaterInternal> updater_internal;
    EXPECT_HRESULT_SUCCEEDED(updater_internal_server.CopyTo(
        IsSystemInstall(scope) ? __uuidof(IUpdaterInternalSystem)
                               : __uuidof(IUpdaterInternalUser),
        IID_PPV_ARGS_Helper(&updater_internal)));
  }

  VerifyInterfacesRegistryEntries(scope);
}

void ExpectMarshalInterfaceSucceeds(UpdaterScope scope) {
  // Create proxy/stubs for the IUpdaterInternal interface.
  // Look up the ProxyStubClsid32.
  CLSID psclsid = {};
  REFIID iupdaterinternal_iid = IsSystemInstall(scope)
                                    ? __uuidof(IUpdaterInternalSystem)
                                    : __uuidof(IUpdaterInternalUser);
  EXPECT_HRESULT_SUCCEEDED(::CoGetPSClsid(iupdaterinternal_iid, &psclsid));
  EXPECT_EQ(base::ToUpperASCII(base::win::WStringFromGUID(psclsid)),
            L"{00020424-0000-0000-C000-000000000046}");

  // Get the proxy/stub factory buffer.
  Microsoft::WRL::ComPtr<IPSFactoryBuffer> psfb;
  EXPECT_HRESULT_SUCCEEDED(
      ::CoGetClassObject(psclsid, CLSCTX_INPROC, 0, IID_PPV_ARGS(&psfb)));

  // Create the interface proxy.
  Microsoft::WRL::ComPtr<IRpcProxyBuffer> proxy_buffer;
  Microsoft::WRL::ComPtr<IUpdaterInternal> object;
  EXPECT_HRESULT_SUCCEEDED(psfb->CreateProxy(nullptr, iupdaterinternal_iid,
                                             &proxy_buffer,
                                             IID_PPV_ARGS_Helper(&object)));

  // Create the interface stub.
  Microsoft::WRL::ComPtr<IRpcStubBuffer> stub_buffer;
  EXPECT_HRESULT_SUCCEEDED(
      psfb->CreateStub(iupdaterinternal_iid, nullptr, &stub_buffer));

  // Marshal and unmarshal an IUpdaterInternal object.
  Microsoft::WRL::ComPtr<IUpdaterInternal> updater_internal;
  EXPECT_HRESULT_SUCCEEDED(
      Microsoft::WRL::MakeAndInitialize<UpdaterInternalImpl>(
          &updater_internal));

  Microsoft::WRL::ComPtr<IStream> stream;
  EXPECT_HRESULT_SUCCEEDED(::CoMarshalInterThreadInterfaceInStream(
      iupdaterinternal_iid, updater_internal.Get(), &stream));

  base::WaitableEvent unmarshal_complete_event;

  base::ThreadPool::CreateCOMSTATaskRunner({base::MayBlock()})
      ->PostTask(
          FROM_HERE,
          base::BindOnce(
              [](Microsoft::WRL::ComPtr<IStream> stream,
                 REFIID iupdaterinternal_iid, base::WaitableEvent& event) {
                const base::ScopedClosureRunner signal_event(base::BindOnce(
                    [](base::WaitableEvent& event) { event.Signal(); },
                    std::ref(event)));

                Microsoft::WRL::ComPtr<IUpdaterInternal> updater_internal;
                EXPECT_HRESULT_SUCCEEDED(::CoUnmarshalInterface(
                    stream.Get(), iupdaterinternal_iid,
                    IID_PPV_ARGS_Helper(&updater_internal)));
              },
              stream, iupdaterinternal_iid,
              std::ref(unmarshal_complete_event)));

  EXPECT_TRUE(
      unmarshal_complete_event.TimedWait(TestTimeouts::action_max_timeout()));
}

void InitializeBundle(UpdaterScope scope,
                      Microsoft::WRL::ComPtr<IAppBundleWeb>& bundle_web) {
  Microsoft::WRL::ComPtr<IGoogleUpdate3Web> update3web;
  ASSERT_HRESULT_SUCCEEDED(CreateLocalServer(
      IsSystemInstall(scope) ? __uuidof(GoogleUpdate3WebSystemClass)
                             : __uuidof(GoogleUpdate3WebUserClass),
      update3web));

  Microsoft::WRL::ComPtr<IAppBundleWeb> bundle;
  Microsoft::WRL::ComPtr<IDispatch> dispatch;
  ASSERT_HRESULT_SUCCEEDED(update3web->createAppBundleWeb(&dispatch));
  ASSERT_HRESULT_SUCCEEDED(dispatch.As(&bundle));

  EXPECT_HRESULT_SUCCEEDED(bundle->initialize());

  bundle_web = bundle;
}

HRESULT DoLoopUntilDone(Microsoft::WRL::ComPtr<IAppBundleWeb> bundle,
                        int expected_final_state,
                        HRESULT expected_error_code) {
  bool done = false;
  static const base::TimeDelta kExpirationTimeout =
      2 * TestTimeouts::action_max_timeout();
  base::ElapsedTimer timer;

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

    // TODO(crbug.com/1245992): Remove this logging once the code is test
    // flakiness is eliminated and no further debugging is needed.
    LOG(ERROR) << base::StringPrintf(L"[State: %d][%ls]%ls", state_value,
                                     stateDescription.c_str(),
                                     extraData.c_str());
    base::PlatformThread::Sleep(base::Seconds(1));
  }

  EXPECT_TRUE(done)
      << "The test timed out, consider increasing kExpirationTimeout which is: "
      << kExpirationTimeout;
  EXPECT_EQ(expected_final_state, state_value);
  EXPECT_EQ(expected_error_code, error_code);

  return S_OK;
}

HRESULT DoUpdate(UpdaterScope scope,
                 const base::win::ScopedBstr& appid,
                 int expected_final_state,
                 HRESULT expected_error_code) {
  Microsoft::WRL::ComPtr<IAppBundleWeb> bundle;
  InitializeBundle(scope, bundle);
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

void SetupLaunchCommandElevated(const std::wstring& app_id,
                                const std::wstring& name,
                                const std::wstring& pv,
                                const std::wstring& command_id,
                                const std::wstring& command_parameters,
                                base::ScopedTempDir& temp_dir) {
  base::CommandLine cmd_exe_command_line(base::CommandLine::NO_PROGRAM);
  SetupCmdExe(UpdaterScope::kSystem, cmd_exe_command_line, temp_dir);
  CreateLaunchCmdElevatedRegistry(
      app_id, name, pv, command_id,
      base::StrCat({cmd_exe_command_line.GetCommandLineString(),
                    command_parameters.c_str()}));
}

void DeleteLaunchCommandElevated(const std::wstring& app_id,
                                 const std::wstring& command_id) {
  EXPECT_EQ(CreateAppClientKey(UpdaterScope::kSystem, app_id)
                .DeleteValue(command_id.c_str()),
            ERROR_SUCCESS);
}

HRESULT ProcessLaunchCmdElevated(
    Microsoft::WRL::ComPtr<IProcessLauncher> process_launcher,
    const std::wstring& appid,
    const std::wstring& commandid,
    const int expected_exit_code) {
  ULONG_PTR proc_handle = 0;
  HRESULT hr = process_launcher->LaunchCmdElevated(
      appid.c_str(), commandid.c_str(), ::GetCurrentProcessId(), &proc_handle);
  if (FAILED(hr))
    return hr;

  EXPECT_NE(static_cast<ULONG_PTR>(0), proc_handle);

  const base::Process process(reinterpret_cast<HANDLE>(proc_handle));
  int exit_code = 0;
  EXPECT_TRUE(process.WaitForExitWithTimeout(TestTimeouts::action_max_timeout(),
                                             &exit_code));
  EXPECT_EQ(exit_code, expected_exit_code);

  return hr;
}

void ExpectLegacyProcessLauncherSucceeds(UpdaterScope scope) {
  // ProcessLauncher is only implemented for kSystem at the moment.
  if (!IsSystemInstall(scope))
    return;

  Microsoft::WRL::ComPtr<IProcessLauncher> process_launcher;
  ASSERT_HRESULT_SUCCEEDED(
      CreateLocalServer(__uuidof(ProcessLauncherClass), process_launcher));

  constexpr wchar_t kAppId1[] = L"{831EF4D0-B729-4F61-AA34-91526481799D}";
  constexpr wchar_t kCommandId[] = L"cmd";

  // Succeeds when the command is present in the registry.
  base::ScopedTempDir temp_dir;
  SetupLaunchCommandElevated(kAppId1, L"" BROWSER_PRODUCT_NAME_STRING,
                             L"1.0.0.0", kCommandId, L" /c \"exit 5420\"",
                             temp_dir);

  // Succeeds when the command is present in the registry.
  ASSERT_HRESULT_SUCCEEDED(
      ProcessLaunchCmdElevated(process_launcher, kAppId1, kCommandId, 5420));

  DeleteLaunchCommandElevated(kAppId1, kCommandId);
  EXPECT_EQ(
      HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND),
      ProcessLaunchCmdElevated(process_launcher, kAppId1, kCommandId, 5420));

  base::ScopedTempDir app_command_temp_dir;
  SetupAppCommand(scope, kAppId1, kCommandId, L" /c \"exit 11555\"",
                  app_command_temp_dir);
  ASSERT_HRESULT_SUCCEEDED(
      ProcessLaunchCmdElevated(process_launcher, kAppId1, kCommandId, 11555));

  DeleteAppClientKey(scope, kAppId1);
}

void ExpectLegacyAppCommandWebSucceeds(UpdaterScope scope,
                                       const std::string& app_id,
                                       const std::string& command_id,
                                       const base::Value::List& parameters,
                                       int expected_exit_code) {
  const size_t kMaxParameters = 9;
  ASSERT_LE(parameters.size(), kMaxParameters);

  base::ScopedTempDir temp_dir;
  const std::wstring appid = base::UTF8ToWide(app_id);
  const std::wstring commandid = base::UTF8ToWide(command_id);

  SetupAppCommand(scope, appid, commandid, L" /c \"exit %1\"", temp_dir);

  Microsoft::WRL::ComPtr<IAppBundleWeb> bundle;
  InitializeBundle(scope, bundle);
  ASSERT_HRESULT_SUCCEEDED(
      bundle->createInstalledApp(base::win::ScopedBstr(appid).Get()));

  Microsoft::WRL::ComPtr<IDispatch> app_dispatch;
  ASSERT_HRESULT_SUCCEEDED(bundle->get_appWeb(0, &app_dispatch));
  Microsoft::WRL::ComPtr<IAppWeb> app;
  ASSERT_HRESULT_SUCCEEDED(app_dispatch.As(&app));

  Microsoft::WRL::ComPtr<IDispatch> command_dispatch;
  ASSERT_HRESULT_SUCCEEDED(app->get_command(
      base::win::ScopedBstr(commandid).Get(), &command_dispatch));
  Microsoft::WRL::ComPtr<IAppCommandWeb> app_command_web;
  ASSERT_HRESULT_SUCCEEDED(command_dispatch.As(&app_command_web));

  std::vector<base::win::ScopedVariant> variant_params;
  variant_params.reserve(kMaxParameters);
  base::ranges::transform(parameters, std::back_inserter(variant_params),
                          [](const auto& param) {
                            return base::win::ScopedVariant(
                                base::UTF8ToWide(param.GetString()).c_str());
                          });
  for (size_t i = parameters.size(); i < kMaxParameters; ++i)
    variant_params.emplace_back(base::win::ScopedVariant::kEmptyVariant);

  ASSERT_HRESULT_SUCCEEDED(app_command_web->execute(
      variant_params[0], variant_params[1], variant_params[2],
      variant_params[3], variant_params[4], variant_params[5],
      variant_params[6], variant_params[7], variant_params[8]));

  EXPECT_TRUE(WaitFor(base::BindLambdaForTesting([&]() {
    UINT status = 0;
    EXPECT_HRESULT_SUCCEEDED(app_command_web->get_status(&status));
    return status == COMMAND_STATUS_COMPLETE;
  })));

  DWORD exit_code = 0;
  EXPECT_HRESULT_SUCCEEDED(app_command_web->get_exitCode(&exit_code));
  EXPECT_EQ(exit_code, static_cast<DWORD>(expected_exit_code));

  // Now also run the AppCommand using the IDispatch methods.
  command_dispatch.Reset();
  ASSERT_HRESULT_SUCCEEDED(app->get_command(
      base::win::ScopedBstr(commandid).Get(), &command_dispatch));

  CallDispatchMethod(command_dispatch, L"execute", variant_params);

  EXPECT_TRUE(WaitFor(base::BindLambdaForTesting([&]() {
    base::win::ScopedVariant status =
        GetDispatchProperty(command_dispatch, L"status");
    return V_UINT(status.ptr()) == COMMAND_STATUS_COMPLETE;
  })));

  base::win::ScopedVariant command_exit_code =
      GetDispatchProperty(command_dispatch, L"exitCode");
  EXPECT_EQ(V_UI4(command_exit_code.ptr()),
            static_cast<DWORD>(expected_exit_code));

  DeleteAppClientKey(scope, appid);
}

namespace {

void ExpectPolicyStatusValues(
    Microsoft::WRL::ComPtr<IPolicyStatusValue> policy_status_value,
    const std::wstring& expected_source,
    const std::wstring& expected_value,
    VARIANT_BOOL expected_has_conflict) {
  base::win::ScopedBstr source;
  base::win::ScopedBstr value;
  VARIANT_BOOL has_conflict = VARIANT_FALSE;

  ASSERT_NE(policy_status_value.Get(), nullptr);
  EXPECT_HRESULT_SUCCEEDED(policy_status_value->get_source(source.Receive()));
  EXPECT_EQ(source.Get(), expected_source);
  EXPECT_HRESULT_SUCCEEDED(policy_status_value->get_value(value.Receive()));
  EXPECT_EQ(value.Get(), expected_value);
  EXPECT_HRESULT_SUCCEEDED(policy_status_value->get_hasConflict(&has_conflict));
  EXPECT_EQ(has_conflict, expected_has_conflict);
}

}  // namespace

void ExpectLegacyPolicyStatusSucceeds(UpdaterScope scope) {
  Microsoft::WRL::ComPtr<IUnknown> policy_status_server;
  ASSERT_HRESULT_SUCCEEDED(CreateLocalServer(
      IsSystemInstall(scope) ? __uuidof(PolicyStatusSystemClass)
                             : __uuidof(PolicyStatusUserClass),
      policy_status_server));
  Microsoft::WRL::ComPtr<IPolicyStatus2> policy_status2;
  ASSERT_HRESULT_SUCCEEDED(policy_status_server.As(&policy_status2));

  base::win::ScopedBstr updater_version;
  ASSERT_HRESULT_SUCCEEDED(
      policy_status2->get_updaterVersion(updater_version.Receive()));
  EXPECT_STREQ(updater_version.Get(), kUpdaterVersionUtf16);

  DATE last_checked = 0;
  EXPECT_HRESULT_SUCCEEDED(policy_status2->get_lastCheckedTime(&last_checked));
  EXPECT_GT(static_cast<int>(last_checked), 0);

  Microsoft::WRL::ComPtr<IPolicyStatusValue> policy_status_value;
  ASSERT_HRESULT_SUCCEEDED(
      policy_status2->get_lastCheckPeriodMinutes(&policy_status_value));
  ExpectPolicyStatusValues(policy_status_value, L"default", L"270",
                           VARIANT_FALSE);

  const base::win::ScopedBstr test_app(L"test1");
  policy_status_value.Reset();
  ASSERT_HRESULT_SUCCEEDED(policy_status2->get_effectivePolicyForAppInstalls(
      test_app.Get(), &policy_status_value));
  ExpectPolicyStatusValues(policy_status_value, L"default", L"1",
                           VARIANT_FALSE);

  policy_status_value.Reset();
  ASSERT_HRESULT_SUCCEEDED(policy_status2->get_effectivePolicyForAppUpdates(
      test_app.Get(), &policy_status_value));
  ExpectPolicyStatusValues(policy_status_value, L"default", L"1",
                           VARIANT_FALSE);

  policy_status_value.Reset();
  ASSERT_HRESULT_SUCCEEDED(policy_status2->get_isRollbackToTargetVersionAllowed(
      test_app.Get(), &policy_status_value));
  ExpectPolicyStatusValues(policy_status_value, L"default", L"false",
                           VARIANT_FALSE);

  ASSERT_HRESULT_SUCCEEDED(policy_status2->refreshPolicies());
}

int RunVPythonCommand(const base::CommandLine& command_line) {
  base::CommandLine python_command = command_line;
  python_command.PrependWrapper(FILE_PATH_LITERAL("vpython3.bat"));

  int exit_code = -1;
  base::Process process = base::LaunchProcess(python_command, {});
  EXPECT_TRUE(process.IsValid());
  EXPECT_TRUE(process.WaitForExitWithTimeout(TestTimeouts::action_timeout(),
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

void InvokeTestServiceFunction(const std::string& function_name,
                               const base::Value::Dict& arguments) {
  std::string arguments_json_string;
  EXPECT_TRUE(base::JSONWriter::Write(arguments, &arguments_json_string));

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
  base::FilePath exe_path;
  ASSERT_TRUE(base::PathService::Get(base::DIR_EXE, &exe_path));
  base::FilePath old_updater_path =
      exe_path.Append(FILE_PATH_LITERAL("old_updater"));
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
  Run(scope, command_line, &exit_code);
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
  EXPECT_TRUE(process.WaitForExitWithTimeout(TestTimeouts::action_timeout(),
                                             &exit_code));
  EXPECT_EQ(0, exit_code);
}

void RunHandoff(UpdaterScope scope, const std::string& app_id) {
  const absl::optional<base::FilePath> installed_executable_path =
      GetInstalledExecutablePath(scope);
  ASSERT_TRUE(installed_executable_path);
  ASSERT_TRUE(base::PathExists(*installed_executable_path));

  base::ScopedAllowBaseSyncPrimitivesForTesting allow_wait_process;
  const std::wstring command_line(base::StrCat(
      {base::CommandLine::QuoteForCommandLineToArgvW(
           installed_executable_path->value()),
       L" /handoff \"appguid=", base::ASCIIToWide(app_id), L"&needsadmin=",
       IsSystemInstall(scope) ? L"Prefers" : L"False", L"\" /silent"}));
  VLOG(0) << " RunHandoff: " << command_line;
  const base::Process process = base::LaunchProcess(command_line, {});
  ASSERT_TRUE(process.IsValid());

  int exit_code = 0;
  ASSERT_TRUE(process.WaitForExitWithTimeout(TestTimeouts::action_max_timeout(),
                                             &exit_code));
  ASSERT_EQ(exit_code, 0);
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
  auto persisted_data = base::MakeRefCounted<PersistedData>(
      scope, global_prefs->GetPrefService());

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

  EXPECT_EQ(persisted_data->GetProductVersion(kChromeAppId),
            base::Version("99.0.0.1"));
  EXPECT_EQ(persisted_data->GetAP(kChromeAppId), "TestAP");
  EXPECT_EQ(persisted_data->GetBrandCode(kChromeAppId), "GGLS");
  EXPECT_TRUE(persisted_data->GetFingerprint(kChromeAppId).empty());
}

void InstallApp(UpdaterScope scope, const std::string& app_id) {
  base::win::RegKey key;
  ASSERT_EQ(key.Create(UpdaterScopeToHKeyRoot(scope),
                       GetAppClientsKey(app_id).c_str(), Wow6432(KEY_WRITE)),
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

void RunOfflineInstall(UpdaterScope scope,
                       bool is_legacy_install,
                       bool is_silent_install) {
  constexpr wchar_t kTestAppID[] = L"{CDABE316-39CD-43BA-8440-6D1E0547AEE6}";
  constexpr char kManifestFormat[] =
      "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
      "<response protocol=\"3.0\">\n"
      "  <app appid=\"%ls\" status=\"ok\">\n"
      "    <updatecheck status=\"ok\">\n"
      "      <manifest version=\"1.2.3.4\">\n"
      "        <packages>\n"
      "          <package hash_sha256=\"sha256hash_foobar\"\n"
      "            name=\"cmd.exe\" required=\"true\" size=\"%lld\"/>\n"
      "        </packages>\n"
      "        <actions>\n"
      "          <action event=\"install\"\n"
      "            run=\"cmd.exe\"\n"
      "            arguments=\"/c &quot;%ls&quot; \"/>\n"
      "        </actions>\n"
      "      </manifest>\n"
      "    </updatecheck>\n"
      "    <data index=\"verboselogging\" name=\"install\" status=\"ok\">\n"
      "      {\"distribution\": { \"verbose_logging\": true}}\n"
      "    </data>\n"
      "  </app>\n"
      "</response>\n";

  const std::wstring manifest_filename(L"OfflineManifest.gup");
  const std::wstring cmd_exe_arbitrarily_named(L"arbitrarily_named_cmd.exe");
  const std::string script_name("test_installer.bat");
  const std::wstring offline_dir_guid(
      L"{7B3A5597-DDEA-409B-B900-4C3D2A94A75C}");
  const HKEY root = UpdaterScopeToHKeyRoot(scope);
  const std::wstring app_client_state_key = GetAppClientStateKey(kTestAppID);

  EXPECT_TRUE(DeleteRegKey(root, app_client_state_key));

  const absl::optional<base::FilePath> updater_exe =
      GetInstalledExecutablePath(scope);
  ASSERT_TRUE(updater_exe.has_value());

  const base::FilePath exe_dir(updater_exe->DirName());
  const base::FilePath offline_dir(
      exe_dir.Append(L"Offline").Append(offline_dir_guid));
  const base::FilePath offline_app_dir(offline_dir.Append(kTestAppID));
  const base::FilePath offline_app_scripts_dir(
      offline_app_dir.Append(L"Scripts"));
  ASSERT_TRUE(base::CreateDirectory(offline_app_scripts_dir));

  // Create a batch file as the installer script, which creates some registry
  // values as the installation artifacts.
  const base::FilePath batch_script_path(
      offline_app_scripts_dir.AppendASCII(script_name));

  // Create a unique name for a shared event to be waited for in this process
  // and signaled in the offline installer process to confirm the installer
  // was run.
  test::EventHolder event_holder(test::CreateWaitableEventForTest());

  EXPECT_TRUE(base::WriteFile(
      batch_script_path,
      [](UpdaterScope scope, const std::string& app_client_state_key,
         const std::wstring& event_name) -> std::string {
        const std::string reg_hive = IsSystemInstall(scope) ? "HKLM" : "HKCU";

        base::CommandLine post_install_cmd(
            GetTestProcessCommandLine(scope, GetTestName()));
        post_install_cmd.AppendSwitchNative(kTestEventToSignal, event_name);
        std::vector<std::string> commands;
        const struct {
          const char* value_name;
          const char* type;
          const std::string value;
        } reg_items[5] = {
            {"InstallerResult", "REG_DWORD", "0"},
            {"InstallerError", "REG_DWORD", "0"},
            {"InstallerExtraCode1", "REG_DWORD", "0"},
            {"InstallerResultUIString", "REG_SZ", "CoolApp"},
            {"InstallerSuccessLaunchCmdLine", "REG_SZ",
             base::WideToASCII(post_install_cmd.GetCommandLineString())},
        };
        for (const auto& reg_item : reg_items) {
          commands.push_back(base::StringPrintf(
              "REG.exe ADD \"%s\\%s\" /v %s /t %s /d \"%s\" /f /reg:32",
              reg_hive.c_str(), app_client_state_key.c_str(),
              reg_item.value_name, reg_item.type, reg_item.value.c_str()));
        }
        return base::JoinString(commands, "\n");
      }(scope, base::WideToASCII(app_client_state_key), event_holder.name)));

  // The updater only allows `.exe` or `.msi` to run from the offline directory.
  // Setup `cmd.exe` as the wrapper installer.
  base::FilePath cmd_exe_path;
  ASSERT_TRUE(base::PathService::Get(base::DIR_SYSTEM, &cmd_exe_path));
  cmd_exe_path = cmd_exe_path.Append(L"cmd.exe");
  ASSERT_TRUE(base::CopyFile(
      cmd_exe_path, offline_app_dir.Append(cmd_exe_arbitrarily_named)));

  base::FilePath manifest_path = offline_dir.Append(manifest_filename);
  int64_t exe_size = 0;
  EXPECT_TRUE(base::GetFileSize(cmd_exe_path, &exe_size));
  const std::string manifest = base::StringPrintf(
      kManifestFormat, kTestAppID, exe_size, batch_script_path.value().c_str());
  EXPECT_TRUE(base::WriteFile(manifest_path, manifest));

  // Trigger offline install.
  ASSERT_TRUE(LaunchOfflineInstallProcess(
                  is_legacy_install, updater_exe.value(), scope, kTestAppID,
                  offline_dir_guid, is_silent_install)
                  .IsValid());

  if (is_silent_install) {
    EXPECT_TRUE(WaitForUpdaterExit(scope));
  } else {
    // Dismiss the installation completion dialog, then wait for the process
    // exit.
    EXPECT_TRUE(WaitFor(
        base::BindRepeating([]() {
          // Enumerate the top-level dialogs to find the setup dialog.
          WindowEnumerator(
              ::GetDesktopWindow(), base::BindRepeating([](HWND hwnd) {
                return WindowEnumerator::IsSystemDialog(hwnd) &&
                       base::Contains(WindowEnumerator::GetWindowText(hwnd),
                                      GetLocalizedStringF(
                                          IDS_INSTALLER_DISPLAY_NAME_BASE,
                                          GetLocalizedString(
                                              IDS_FRIENDLY_COMPANY_NAME_BASE)));
              }),
              base::BindRepeating([](HWND hwnd) {
                // Enumerates the dialog items to search for installation
                // complete message. Once found, close the dialog.
                WindowEnumerator(
                    hwnd, base::BindRepeating([](HWND hwnd) {
                      return base::Contains(
                          WindowEnumerator::GetWindowText(hwnd),
                          GetLocalizedString(
                              IDS_BUNDLE_INSTALLED_SUCCESSFULLY_BASE));
                    }),
                    base::BindRepeating([](HWND hwnd) {
                      ::PostMessage(::GetParent(hwnd), WM_CLOSE, 0, 0);
                    }))
                    .Run();
              }))
              .Run();

          return !IsUpdaterRunning();
        }),
        base::BindLambdaForTesting(
            []() { VLOG(0) << "Still waiting for the process exit."; })));
  }

  // App installer should have created the expected reg value.
  base::win::RegKey key;
  std::wstring value;
  EXPECT_EQ(
      key.Open(root, app_client_state_key.c_str(), Wow6432(KEY_QUERY_VALUE)),
      ERROR_SUCCESS);
  EXPECT_EQ(key.ReadValue(kRegValueInstallerResultUIString, &value),
            ERROR_SUCCESS);
  EXPECT_EQ(value, L"CoolApp");

  if (!is_silent_install) {
    // Silent install does not run post-install command. For other cases the
    // event should have been signaled by the post-install command via the
    // installer result API.
    EXPECT_TRUE(
        event_holder.event.TimedWait(TestTimeouts::action_max_timeout()));
  }

  EXPECT_TRUE(DeleteRegKey(root, app_client_state_key));
}

}  // namespace updater::test
