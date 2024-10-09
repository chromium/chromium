// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/updater/test/integration_tests_win.h"

#include <regstr.h>
#include <shlobj.h>
#include <wrl/client.h>
#include <wrl/implements.h>

#include <functional>
#include <iostream>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "base/base_paths.h"
#include "base/command_line.h"
#include "base/containers/adapters.h"
#include "base/containers/contains.h"
#include "base/files/file_enumerator.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/format_macros.h"
#include "base/functional/callback_helpers.h"
#include "base/json/json_writer.h"
#include "base/logging.h"
#include "base/memory/scoped_refptr.h"
#include "base/path_service.h"
#include "base/process/launch.h"
#include "base/process/process.h"
#include "base/ranges/algorithm.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/sys_string_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/synchronization/waitable_event.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/test/bind.h"
#include "base/test/gmock_expected_support.h"
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
#include "base/win/window_enumerator.h"
#include "build/branding_buildflags.h"
#include "build/build_config.h"
#include "chrome/updater/activity.h"
#include "chrome/updater/app/server/win/com_classes.h"
#include "chrome/updater/app/server/win/updater_idl.h"
#include "chrome/updater/app/server/win/updater_internal_idl.h"
#include "chrome/updater/app/server/win/updater_legacy_idl.h"
#include "chrome/updater/constants.h"
#include "chrome/updater/external_constants_builder.h"
#include "chrome/updater/persisted_data.h"
#include "chrome/updater/prefs.h"
#include "chrome/updater/registration_data.h"
#include "chrome/updater/test/integration_tests_impl.h"
#include "chrome/updater/test/unit_test_util.h"
#include "chrome/updater/test/unit_test_util_win.h"
#include "chrome/updater/updater_branding.h"
#include "chrome/updater/updater_scope.h"
#include "chrome/updater/updater_version.h"
#include "chrome/updater/util/util.h"
#include "chrome/updater/util/win_util.h"
#include "chrome/updater/win/setup/setup_util.h"
#include "chrome/updater/win/task_scheduler.h"
#include "chrome/updater/win/test/test_executables.h"
#include "chrome/updater/win/test/test_strings.h"
#include "chrome/updater/win/ui/l10n_util.h"
#include "chrome/updater/win/ui/resources/resources.grh"
#include "chrome/updater/win/ui/resources/updater_installer_strings.h"
#include "chrome/updater/win/ui/ui_util.h"
#include "chrome/updater/win/win_constants.h"
#include "components/crx_file/crx_verifier.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
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
  return ::CoCreateInstance(clsid, nullptr, CLSCTX_LOCAL_SERVER,
                            IID_PPV_ARGS(&server));
}

[[nodiscard]] bool RegKeyExists(HKEY root, const std::wstring& path) {
  return base::win::RegKey(root, path.c_str(), Wow6432(KEY_QUERY_VALUE))
      .Valid();
}

[[nodiscard]] bool RegKeyExists64(HKEY root, const std::wstring& path) {
  return base::win::RegKey(root, path.c_str(),
                           KEY_WOW64_64KEY | KEY_QUERY_VALUE)
      .Valid();
}

[[nodiscard]] bool RegKeyExistsCOM(HKEY root, const std::wstring& path) {
  return base::win::RegKey(root, path.c_str(), KEY_QUERY_VALUE).Valid();
}

[[nodiscard]] std::wstring ReadRegValue(HKEY root,
                                        const std::wstring& path,
                                        const std::wstring& value,
                                        REGSAM wow64_access) {
  std::wstring result;
  base::win::RegKey(root, path.c_str(), wow64_access | KEY_QUERY_VALUE)
      .ReadValue(value.c_str(), &result);
  return result;
}

[[nodiscard]] bool DeleteRegKey(HKEY root, const std::wstring& path) {
  LONG result =
      base::win::RegKey(root, L"", Wow6432(DELETE)).DeleteKey(path.c_str());
  return result == ERROR_SUCCESS || result == ERROR_FILE_NOT_FOUND;
}

[[nodiscard]] bool DeleteRegKey64(HKEY root, const std::wstring& path) {
  LONG result = base::win::RegKey(root, L"", KEY_WOW64_64KEY | DELETE)
                    .DeleteKey(path.c_str());
  return result == ERROR_SUCCESS || result == ERROR_FILE_NOT_FOUND;
}

[[nodiscard]] bool DeleteRegKeyCOM(HKEY root, const std::wstring& path) {
  LONG result = base::win::RegKey(root, L"", DELETE).DeleteKey(path.c_str());
  return result == ERROR_SUCCESS || result == ERROR_FILE_NOT_FOUND;
}

[[nodiscard]] bool IsServiceGone(const std::wstring& service_name) {
  ScopedScHandle scm(::OpenSCManager(
      nullptr, nullptr, SC_MANAGER_CONNECT | SC_MANAGER_CREATE_SERVICE));
  if (!scm.IsValid()) {
    return false;
  }

  ScopedScHandle service(
      ::OpenService(scm.Get(), service_name.c_str(),
                    SERVICE_QUERY_CONFIG | SERVICE_CHANGE_CONFIG));
  bool is_service_gone = !service.IsValid();
  if (!is_service_gone) {
    if (!::ChangeServiceConfig(service.Get(), SERVICE_NO_CHANGE,
                               SERVICE_NO_CHANGE, SERVICE_NO_CHANGE, nullptr,
                               nullptr, nullptr, nullptr, nullptr, nullptr,
                               nullptr)) {
      is_service_gone = ::GetLastError() == ERROR_SERVICE_MARKED_FOR_DELETE;
    }
  }

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
    EXPECT_EQ(is_installed, base::PathExists(*GetGoogleUpdateExePath(scope)));
    EXPECT_EQ(is_installed,
              RegKeyExists(UpdaterScopeToHKeyRoot(scope),
                           base::StrCat({CLIENT_STATE_KEY,
                                         base::UTF8ToWide(kUpdaterAppId)})));

    if (is_installed) {
      for (const wchar_t* key : {CLIENTS_KEY, CLIENT_STATE_KEY, UPDATER_KEY}) {
        EXPECT_TRUE(RegKeyExists(root, key)) << key;
      }

      std::wstring pv;
      EXPECT_EQ(ERROR_SUCCESS,
                base::win::RegKey(
                    root, GetAppClientsKey(kLegacyGoogleUpdateAppID).c_str(),
                    Wow6432(KEY_READ))
                    .ReadValue(kRegValuePV, &pv));
      EXPECT_STREQ(kUpdaterVersionUtf16, pv.c_str());
      EXPECT_EQ(
          ERROR_SUCCESS,
          base::win::RegKey(
              root, GetAppClientStateKey(kLegacyGoogleUpdateAppID).c_str(),
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

      EXPECT_EQ(ERROR_SUCCESS,
                base::win::RegKey(root, UPDATER_KEY, Wow6432(KEY_READ))
                    .HasValue(kRegValueVersion));

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
             {kRegKeyCompanyCloudManagement, UPDATER_POLICIES_KEY}) {
          EXPECT_FALSE(RegKeyExists(HKEY_LOCAL_MACHINE, key));
        }
      }
      if (!IsSystemInstall(scope)) {
        ForEachRegistryRunValueWithPrefix(
            base::ASCIIToWide(PRODUCT_FULLNAME_STRING),
            [](const std::wstring& run_name) {
              ADD_FAILURE() << "Unexpected Run key found: " << run_name;
            });
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

    const std::wstring progid(GetProgIdForClsid(clsid));
    if (!progid.empty()) {
      EXPECT_EQ(is_installed,
                RegKeyExistsCOM(root, GetComProgIdRegistryPath(progid)));
    }
  }

  for (const auto& [iid, expected_interface_name] : JoinVectors(
           GetSideBySideInterfaces(scope),
           is_active_and_sxs ? GetActiveInterfaces(scope)
                             : std::vector<std::pair<IID, std::wstring>>())) {
    EXPECT_EQ(is_installed, RegKeyExists(root, GetComIidRegistryPath(iid)));
    EXPECT_EQ(is_installed, RegKeyExists64(root, GetComIidRegistryPath(iid)));
    if (is_installed) {
      for (const auto& key_flag : {KEY_WOW64_32KEY, KEY_WOW64_64KEY}) {
        EXPECT_EQ(ReadRegValue(root, GetComIidRegistryPath(iid), L"", key_flag),
                  expected_interface_name);
      }
    }
    EXPECT_EQ(is_installed,
              RegKeyExistsCOM(root, GetComTypeLibRegistryPath(iid)));
  }

  if (IsSystemInstall(scope)) {
    for (const bool is_internal_service : {false, true}) {
      if (!is_active_and_sxs && !is_internal_service) {
        continue;
      }

      const std::wstring service_name(GetServiceName(is_internal_service));
      EXPECT_EQ(is_installed,
                !IsServiceGone(GetServiceName(is_internal_service)))
          << ": " << service_name << ": " << is_internal_service;

      int count_entries = 0;
      ForEachServiceWithPrefix(
          base::StrCat({base::ASCIIToWide(PRODUCT_FULLNAME_STRING),
                        is_internal_service ? kWindowsInternalServiceName
                                            : kWindowsServiceName}),
          GetLocalizedString(
              is_internal_service
                  ? IDS_INTERNAL_UPDATER_SERVICE_DISPLAY_NAME_BASE
                  : IDS_UPDATER_SERVICE_DISPLAY_NAME_BASE),
          [&](const std::wstring& service_name) {
            ++count_entries;

            if (is_installed) {
              base::win::RegKey key;
              ASSERT_EQ(key.Open(HKEY_LOCAL_MACHINE,
                                 base::StrCat(
                                     {L"SYSTEM\\CurrentControlSet\\Services\\",
                                      service_name})
                                     .c_str(),
                                 Wow6432(KEY_READ)),
                        ERROR_SUCCESS);
              std::wstring description;
              EXPECT_EQ(key.ReadValue(L"Description", &description),
                        ERROR_SUCCESS);
              EXPECT_EQ(description, GetLocalizedString(
                                         IDS_UPDATER_SERVICE_DESCRIPTION_BASE));
            } else {
              ADD_FAILURE() << "Unexpected service found: " << service_name;
            }
          });
      EXPECT_EQ(count_entries, is_installed);
    }
  }

  scoped_refptr<TaskScheduler> task_scheduler =
      TaskScheduler::CreateInstance(scope);
  if (is_installed) {
    const std::wstring task_name =
        task_scheduler->FindFirstTaskName(GetTaskNamePrefix(scope));
    EXPECT_TRUE(!task_name.empty());
    EXPECT_TRUE(task_scheduler->IsTaskRegistered(task_name));

    TaskScheduler::TaskInfo task_info;
    ASSERT_TRUE(task_scheduler->GetTaskInfo(task_name, task_info));
    ASSERT_EQ(task_info.exec_actions.size(), 1u);
    EXPECT_EQ(
        task_info.exec_actions[0].arguments,
        base::StrCat({L"--wake", IsSystemInstall(scope) ? L" --system" : L""}));

    EXPECT_EQ(task_info.trigger_types,
              TaskScheduler::TriggerType::TRIGGER_TYPE_HOURLY |
                  TaskScheduler::TriggerType::TRIGGER_TYPE_LOGON);
  } else {
    task_scheduler->ForEachTaskWithPrefix(
        base::ASCIIToWide(PRODUCT_FULLNAME_STRING),
        [](const std::wstring& task_name) {
          ADD_FAILURE() << "Unexpected task found: " << task_name;
        });
  }

  const std::optional<base::FilePath> path =
      GetVersionedInstallDirectory(scope, base::Version(kUpdaterVersion));
  ASSERT_TRUE(path);
  EXPECT_TRUE(WaitFor([&] { return is_installed == base::PathExists(*path); },
                      [&] {
                        VLOG(0) << "Still waiting for " << *path
                                << " where is_installed=" << is_installed;
                      }))
      << base::JoinString(
             [&path] {
               std::vector<base::FilePath::StringType> files;
               base::FileEnumerator(*path, true,
                                    base::FileEnumerator::FILES |
                                        base::FileEnumerator::DIRECTORIES)
                   .ForEach([&files](const base::FilePath& name) {
                     files.push_back(name.value());
                   });

               return files;
             }(),
             FILE_PATH_LITERAL(","));
}

void SleepFor(base::TimeDelta interval) {
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
  auto launch_legacy_offline_install = [&] {
    auto build_legacy_switch =
        [](const std::string& switch_name) -> std::wstring {
      return base::ASCIIToWide(base::StrCat({"/", switch_name}));
    };
    std::vector<std::wstring> install_cmd_args = {
        base::CommandLine::QuoteForCommandLineToArgvW(exe_path.value()),

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

  auto launch_offline_install = [&] {
    base::CommandLine install_cmd(exe_path);
    if (IsSystemInstall(install_scope)) {
      install_cmd.AppendSwitch(kSystemSwitch);
    }

    install_cmd.AppendSwitchNative(
        updater::kHandoffSwitch,
        base::StrCat({L"appguid=", app_id, L"&lang=en"}));
    install_cmd.AppendSwitchASCII(updater::kSessionIdSwitch,
                                  "{E85204C6-6F2F-40BF-9E6C-4952208BB977}");
    install_cmd.AppendSwitchNative(updater::kOfflineDirSwitch,
                                   offline_dir_guid);
    if (is_silent_install) {
      install_cmd.AppendSwitch(updater::kSilentSwitch);
    }

    return base::LaunchProcess(install_cmd, {});
  };

  return is_legacy_install ? launch_legacy_offline_install()
                           : launch_offline_install();
}

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
  base::ranges::transform(base::Reversed(variant_params),
                          std::back_inserter(params),
                          &base::win::ScopedVariant::Copy);

  DISPPARAMS dp = {};
  if (!params.empty()) {
    dp.rgvarg = &params[0];
    dp.cArgs = params.size();
  }

  EXPECT_HRESULT_SUCCEEDED(dispatch->Invoke(
      GetDispId(dispatch, method_name), IID_NULL, LOCALE_USER_DEFAULT,
      DISPATCH_METHOD, &dp, nullptr, nullptr, nullptr));

  base::ranges::for_each(params, [](auto& param) { ::VariantClear(&param); });
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

std::wstring GetAppVersionWebString(
    Microsoft::WRL::ComPtr<IDispatch> version_web_dispatch) {
  Microsoft::WRL::ComPtr<IAppVersionWeb> version_web;
  EXPECT_HRESULT_SUCCEEDED(version_web_dispatch.As(&version_web));

  base::win::ScopedBstr version;
  EXPECT_HRESULT_SUCCEEDED(version_web->get_version(version.Receive()));

  return version.Get();
}

bool BuildTestAppInstaller(const base::FilePath& installer_script,
                           const base::FilePath& output_installer) {
  base::FilePath exe_path;
  if (!base::PathService::Get(base::DIR_EXE, &exe_path)) {
    return false;
  }
  const base::FilePath installer_dir = exe_path.AppendASCII("test_installer");

  base::CommandLine command(
      installer_dir.AppendASCII("embed_install_scripts.py"));
  command.AppendSwitchPath(
      "--installer", installer_dir.AppendASCII("test_meta_installer.exe"));
  command.AppendSwitchPath("--output", output_installer);
  command.AppendSwitchPath("--script", installer_script);
  return RunVPythonCommand(command) == 0;
}

void RunOfflineInstallWithManifest(UpdaterScope scope,
                                   bool is_legacy_install,
                                   bool is_silent_install,
                                   const std::string& manifest_format,
                                   int string_resource_id_to_find,
                                   bool expect_success) {
  static constexpr wchar_t kTestAppID[] =
      L"{CDABE316-39CD-43BA-8440-6D1E0547AEE6}";
  static constexpr char kAppInstallerName[] = "TestAppSetup.exe";
  const base::Version kTestPV("1.2.3.4");
  const std::wstring manifest_filename(L"OfflineManifest.gup");
  const std::wstring offline_dir_guid(
      L"{7B3A5597-DDEA-409B-B900-4C3D2A94A75C}");
  const HKEY root = UpdaterScopeToHKeyRoot(scope);
  const std::wstring app_clients_key = GetAppClientsKey(kTestAppID);
  const std::wstring app_client_state_key = GetAppClientStateKey(kTestAppID);

  EXPECT_TRUE(DeleteRegKey(root, app_clients_key));
  EXPECT_TRUE(DeleteRegKey(root, app_client_state_key));

  const std::optional<base::FilePath> updater_exe =
      GetUpdaterExecutablePath(scope);
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
      offline_app_scripts_dir.AppendASCII("AppSetup.bat"));

  // Create a shared event to be waited for in this process and signaled in the
  // test process. If the test is running elevated with UAC on, the test will
  // also confirm that the test process is launched at medium integrity, by
  // creating an event with a security descriptor that allows the medium
  // integrity process to signal it.
  test::EventHolder event_holder(IsElevatedWithUACOn()
                                     ? CreateEveryoneWaitableEventForTest()
                                     : test::CreateWaitableEventForTest());
  EXPECT_TRUE(base::WriteFile(batch_script_path, [&] {
    const std::string reg_hive = IsSystemInstall(scope) ? "HKLM" : "HKCU";
    const std::string app_client_state_key_utf8 =
        base::WideToUTF8(app_client_state_key);

    base::CommandLine post_install_cmd(
        GetTestProcessCommandLine(scope, GetTestName()));
    post_install_cmd.AppendSwitchNative(
        IsElevatedWithUACOn() ? kTestEventToSignalIfMediumIntegrity
                              : kTestEventToSignal,
        event_holder.name);
    std::vector<std::string> commands;
    const struct {
      const std::string subkey;
      const char* value_name;
      const char* type;
      const std::string value;
    } reg_items[] = {
        {base::WideToUTF8(app_clients_key), "pv", "REG_SZ",
         kTestPV.GetString()},
        {app_client_state_key_utf8, "InstallerResult", "REG_DWORD", "0"},
        {app_client_state_key_utf8, "InstallerError", "REG_DWORD", "0"},
        {app_client_state_key_utf8, "InstallerExtraCode1", "REG_DWORD", "0"},
        {app_client_state_key_utf8, "InstallerResultUIString", "REG_SZ",
         "CoolApp"},
        {app_client_state_key_utf8, "InstallerSuccessLaunchCmdLine", "REG_SZ",
         base::WideToASCII(post_install_cmd.GetCommandLineString())},
    };
    for (const auto& reg_item : reg_items) {
      commands.push_back(base::StringPrintf(
          "REG.exe ADD \"%s\\%s\" /v %s /t %s /d %s /f /reg:32",
          reg_hive.c_str(), reg_item.subkey.c_str(), reg_item.value_name,
          reg_item.type,
          base::WideToASCII(base::CommandLine::QuoteForCommandLineToArgvW(
                                base::ASCIIToWide(reg_item.value)))
              .c_str()));
    }
    return base::JoinString(commands, "\n");
  }()));

  const base::FilePath& app_installer =
      offline_app_dir.AppendASCII(kAppInstallerName);
  EXPECT_TRUE(BuildTestAppInstaller(batch_script_path, app_installer));
  base::FilePath manifest_path = offline_dir.Append(manifest_filename);
  int64_t app_installer_size = 0;
  EXPECT_TRUE(base::GetFileSize(app_installer, &app_installer_size));
  const std::string manifest = base::StringPrintfNonConstexpr(
      manifest_format.c_str(), kTestAppID, /*pv=*/"", kAppInstallerName,
      app_installer_size, kAppInstallerName);
  EXPECT_TRUE(base::WriteFile(manifest_path, manifest));

  // Trigger offline install.
  ASSERT_TRUE(LaunchOfflineInstallProcess(
                  is_legacy_install, updater_exe.value(), scope, kTestAppID,
                  offline_dir_guid, is_silent_install)
                  .IsValid());

  // * Silent installs do not show any UI.
  // * Successful interactive installs show a progress UI, but once the install
  //   completes, the `InstallerSuccessLaunchCmdLine` is launched and the UI
  //   closes automatically.
  // * Unsuccessful interactive installs show an install error dialog that needs
  //   to be explicitly closed via `CloseInstallCompleteDialog`.
  if (is_silent_install || expect_success) {
    EXPECT_TRUE(WaitForUpdaterExit());
  } else {
    CloseInstallCompleteDialog({},
                               GetLocalizedString(string_resource_id_to_find));
  }

  scoped_refptr<GlobalPrefs> global_prefs = CreateGlobalPrefs(scope);
  ASSERT_TRUE(global_prefs);
  const base::Version pv =
      base::MakeRefCounted<PersistedData>(scope, global_prefs->GetPrefService(),
                                          nullptr)
          ->GetProductVersion(base::WideToASCII(kTestAppID));

  base::win::RegKey key;
  LONG registry_result =
      key.Open(root, app_client_state_key.c_str(), Wow6432(KEY_QUERY_VALUE));

  if (!expect_success) {
    EXPECT_EQ(registry_result, ERROR_FILE_NOT_FOUND);
    EXPECT_FALSE(pv.IsValid());
    return;
  }

  EXPECT_EQ(registry_result, ERROR_SUCCESS);

  // Updater should have written "pv".
  ASSERT_TRUE(pv.IsValid());
  EXPECT_EQ(pv, kTestPV);

  // Check for expected installer result API reg values.
  base::win::RegKey updater_key(root, UPDATER_KEY, Wow6432(KEY_QUERY_VALUE));
  ASSERT_TRUE(updater_key.Valid());
  for (const base::win::RegKey& regkey :
       {std::cref(key), std::cref(updater_key)}) {
    std::wstring value;
    EXPECT_EQ(regkey.ReadValue(kRegValueLastInstallerResultUIString, &value),
              ERROR_SUCCESS);
    EXPECT_EQ(value, L"CoolApp");
  }

  if (!is_silent_install) {
    // Silent install does not run post-install command. For other cases the
    // event should have been signaled by the post-install command via the
    // installer result API.
    EXPECT_TRUE(
        event_holder.event.TimedWait(TestTimeouts::action_max_timeout()));
  }

  EXPECT_TRUE(DeleteRegKey(root, app_client_state_key));
}

}  // namespace

base::FilePath GetSetupExecutablePath() {
  base::FilePath out_dir;
  if (!base::PathService::Get(base::DIR_EXE, &out_dir)) {
    return base::FilePath();
  }
  return out_dir.AppendASCII("UpdaterSetup_test.exe");
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
    if (IsSystemInstall(scope)) {
      EXPECT_TRUE(DeleteRegKeyCOM(root, GetComServerAppidRegistryPath(clsid)));
    }

    const std::wstring progid(GetProgIdForClsid(clsid));
    if (!progid.empty()) {
      EXPECT_TRUE(DeleteRegKeyCOM(root, GetComProgIdRegistryPath(progid)));
    }
  }

  // To avoid `TYPE_E_CANTLOADLIBRARY` errors due to a failed cleanup of a
  // previous user test run, this code cleans up both system and user
  // interface/typelib entries when running system tests.
  for (const UpdaterScope interface_scope : [&]() -> std::vector<UpdaterScope> {
         if (IsSystemInstall(scope)) {
           return {scope, UpdaterScope::kUser};
         } else {
           return {scope};
         }
       }()) {
    for (const auto& [iid, interface_name] :
         JoinVectors(GetSideBySideInterfaces(interface_scope),
                     GetActiveInterfaces(interface_scope))) {
      const HKEY interface_root = UpdaterScopeToHKeyRoot(interface_scope);
      EXPECT_TRUE(DeleteRegKey(interface_root, GetComIidRegistryPath(iid)));
      EXPECT_TRUE(DeleteRegKey64(interface_root, GetComIidRegistryPath(iid)));
      EXPECT_TRUE(
          DeleteRegKeyCOM(interface_root, GetComTypeLibRegistryPath(iid)));
    }
  }

  if (!IsSystemInstall(scope)) {
    ForEachRegistryRunValueWithPrefix(
        base::ASCIIToWide(PRODUCT_FULLNAME_STRING),
        [](const std::wstring& run_name) {
          base::win::RegKey(HKEY_CURRENT_USER, REGSTR_PATH_RUN, KEY_WRITE)
              .DeleteValue(run_name.c_str());
        });
  }

  if (IsSystemInstall(scope)) {
    ForEachServiceWithPrefix(base::ASCIIToWide(PRODUCT_FULLNAME_STRING), {},
                             [](const std::wstring& service_name) {
                               EXPECT_TRUE(DeleteService(service_name));
                             });
  }

  scoped_refptr<TaskScheduler> task_scheduler =
      TaskScheduler::CreateInstance(scope);
  task_scheduler->ForEachTaskWithPrefix(
      base::ASCIIToWide(PRODUCT_FULLNAME_STRING),
      [&task_scheduler](const std::wstring& task_name) {
        EXPECT_TRUE(task_scheduler->DeleteTask(task_name));
      });

  const std::optional<base::FilePath> target_path =
      GetGoogleUpdateExePath(scope);
  if (target_path) {
    base::DeleteFile(*target_path);
  }

  std::optional<base::FilePath> path = GetInstallDirectory(scope);
  ASSERT_TRUE(path);
  ASSERT_TRUE(base::DeletePathRecursively(*path)) << *path;

  // Delete any updater logs in the temp directory.
  for (const auto& file : GetUpdaterLogFilesInTmp()) {
    ASSERT_TRUE(base::DeleteFile(file));
  }

  if (IsSystemInstall(scope)) {
    ASSERT_NO_FATAL_FAILURE(UninstallEnterpriseCompanionApp());
  }
}

base::TimeDelta GetOverinstallTimeoutForEnterTestMode() {
  return base::Seconds(11);
}

void ExpectInstalled(UpdaterScope scope) {
  CheckInstallation(scope, CheckInstallationStatus::kCheckIsInstalled,
                    CheckInstallationVersions::kCheckSxSOnly);
}

void ExpectClean(UpdaterScope scope) {
  ExpectCleanProcesses();
  CheckInstallation(scope, CheckInstallationStatus::kCheckIsNotInstalled,
                    CheckInstallationVersions::kCheckActiveAndSxS);

  // Check that the caches have been removed.
  const std::optional<base::FilePath> path = GetCacheBaseDirectory(scope);
  ASSERT_TRUE(path);
  EXPECT_TRUE(
      WaitFor([&] { return !base::PathExists(*path); },
              [&] { VLOG(0) << "Still waiting for cache removal: " << *path; }))
      << base::JoinString(
             [&path] {
               std::vector<base::FilePath::StringType> files;
               base::FileEnumerator(*path, true,
                                    base::FileEnumerator::FILES |
                                        base::FileEnumerator::DIRECTORIES)
                   .ForEach([&files](const base::FilePath& name) {
                     files.push_back(name.value());
                   });

               return files;
             }(),
             FILE_PATH_LITERAL(","));
  ASSERT_NO_FATAL_FAILURE(ExpectEnterpriseCompanionAppNotInstalled());
}

void ExpectCandidateUninstalled(UpdaterScope scope) {
  CheckInstallation(scope, CheckInstallationStatus::kCheckIsNotInstalled,
                    CheckInstallationVersions::kCheckSxSOnly);
}

void Uninstall(UpdaterScope scope) {
  // Note: the updater uninstall is run from the build dir, not the install dir,
  // because it is useful for tests to be able to run it to clean the system
  // even if installation has failed or the installed binaries have already been
  // removed.

  // The updater setup executable is used instead of `updater` because setup
  // knows how to de-elevate when run at high integrity to uninstall a per-user
  // install, which is what is done in the
  // `IntegrationTestUserInSystem.ElevatedInstallOfUserUpdaterAndApp` test.
  base::CommandLine command_line(GetSetupExecutablePath());
  ASSERT_FALSE(command_line.GetProgram().empty());
  command_line.AppendSwitch(kUninstallSwitch);
  int exit_code = -1;
  Run(scope, command_line, &exit_code);

  // Uninstallation involves a race with the uninstall.cmd script and the
  // process exit. Sleep to allow the script to complete its work.
  SleepFor(base::Seconds(5));
  ASSERT_EQ(0, exit_code);
}

void SetActive(UpdaterScope /*scope*/, const std::string& id) {
  base::win::RegKey key;
  ASSERT_EQ(key.Create(HKEY_CURRENT_USER, GetAppClientStateKey(id).c_str(),
                       Wow6432(KEY_WRITE)),
            ERROR_SUCCESS);
  EXPECT_EQ(key.WriteValue(kDidRun, L"1"), ERROR_SUCCESS);
}

void ExpectActive(UpdaterScope /*scope*/, const std::string& id) {
  base::win::RegKey key;
  ASSERT_EQ(key.Open(HKEY_CURRENT_USER, GetAppClientStateKey(id).c_str(),
                     Wow6432(KEY_READ)),
            ERROR_SUCCESS);
  std::wstring value;
  ASSERT_EQ(key.ReadValue(kDidRun, &value), ERROR_SUCCESS);
  EXPECT_EQ(value, L"1");
}

void ExpectNotActive(UpdaterScope /*scope*/, const std::string& id) {
  base::win::RegKey key;
  if (key.Open(HKEY_CURRENT_USER, GetAppClientStateKey(id).c_str(),
               Wow6432(KEY_READ)) == ERROR_SUCCESS) {
    std::wstring value;
    if (key.ReadValue(kDidRun, &value) == ERROR_SUCCESS) {
      EXPECT_EQ(value, L"0");
    }
  }
}

// Waits for all updater processes to end, including the server process holding
// the prefs lock.
bool WaitForUpdaterExit() {
  VersionProcessFilter filter;
  return WaitFor(
      [&] {
        return !test::IsProcessRunning(GetExecutableRelativePath().value(),
                                       &filter);
      },
      [&] {
        VLOG(0) << "Still waiting for updater to exit. "
                << test::PrintProcesses(GetExecutableRelativePath().value(),
                                        &filter);
      });
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
    for (const auto& [iid, interface_name] :
         GetInterfaces(is_internal, scope)) {
      const HKEY root = UpdaterScopeToHKeyRoot(scope);
      const std::wstring iid_reg_path = GetComIidRegistryPath(iid);
      const std::wstring typelib_reg_path = GetComTypeLibRegistryPath(iid);
      const std::wstring iid_string = StringFromGuid(iid);

      for (const auto& key_flag : {KEY_WOW64_32KEY, KEY_WOW64_64KEY}) {
        std::wstring val;
        EXPECT_EQ(ReadRegValue(root, iid_reg_path, L"", key_flag),
                  interface_name);
        EXPECT_EQ(ReadRegValue(root, iid_reg_path + L"\\ProxyStubClsid32", L"",
                               key_flag),
                  L"{00020424-0000-0000-C000-000000000046}");
        EXPECT_EQ(
            ReadRegValue(root, iid_reg_path + L"\\TypeLib", L"", key_flag),
            iid_string);
        EXPECT_EQ(ReadRegValue(root, iid_reg_path + L"\\TypeLib", L"Version",
                               key_flag),
                  L"1.0");
      }

      EXPECT_EQ(ReadRegValue(root, typelib_reg_path + L"\\1.0", L"", 0),
                base::StrCat({PRODUCT_FULLNAME_STRING L" TypeLib for ",
                              interface_name}));
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

void ExpectNoLegacyEntriesPerUser() {
  // The IProcessLauncher and IProcessLauncher2 interfaces are now only
  // registered for system since r1154562. So verify that these do not exist in
  // the user hive.
  for (const auto& iid :
       {__uuidof(IProcessLauncher), __uuidof(IProcessLauncher2)}) {
    for (const auto& reg_path :
         {GetComIidRegistryPath(iid), GetComTypeLibRegistryPath(iid)}) {
      EXPECT_FALSE(RegKeyExistsCOM(HKEY_CURRENT_USER, reg_path));
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

    // Verifies that the progid for the legacy clsid is registered.
    CLSID expected_clsid = {};
    EXPECT_HRESULT_SUCCEEDED(::CLSIDFromProgID(
        IsSystemInstall(scope) ? kGoogleUpdate3WebSystemClassProgId
                               : kGoogleUpdate3WebUserClassProgId,
        &expected_clsid));
    EXPECT_EQ(expected_clsid, IsSystemInstall(scope)
                                  ? __uuidof(GoogleUpdate3WebSystemClass)
                                  : __uuidof(GoogleUpdate3WebUserClass));

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

      // The non-user/system-specialized interfaces are registered for all
      // installs for backward compatibility.
      Microsoft::WRL::ComPtr<IGoogleUpdate3Web> google_update;
      ASSERT_HRESULT_SUCCEEDED(updater_legacy_server.As(&google_update));
      google_update.Reset();
      EXPECT_HRESULT_SUCCEEDED(updater_legacy_server.CopyTo(
          IsSystemInstall(scope) ? __uuidof(IGoogleUpdate3WebSystem)
                                 : __uuidof(IGoogleUpdate3WebUser),
          IID_PPV_ARGS_Helper(&google_update)));
      Microsoft::WRL::ComPtr<IAppBundleWeb> app_bundle;
      Microsoft::WRL::ComPtr<IDispatch> dispatch;
      ASSERT_HRESULT_SUCCEEDED(google_update->createAppBundleWeb(&dispatch));
      EXPECT_HRESULT_SUCCEEDED(dispatch.As(&app_bundle));
      app_bundle.Reset();
      EXPECT_HRESULT_SUCCEEDED(
          dispatch.CopyTo(IsSystemInstall(scope) ? __uuidof(IAppBundleWebSystem)
                                                 : __uuidof(IAppBundleWebUser),
                          IID_PPV_ARGS_Helper(&app_bundle)));
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
  if (!IsSystemInstall(scope)) {
    ExpectNoLegacyEntriesPerUser();
  }
}

void ExpectMarshalInterfaceSucceeds(UpdaterScope scope) {
  // Create proxy/stubs for the IUpdaterInternal interface.
  // Look up the ProxyStubClsid32.
  CLSID psclsid = {};
  REFIID iupdaterinternal_iid = IsSystemInstall(scope)
                                    ? __uuidof(IUpdaterInternalSystem)
                                    : __uuidof(IUpdaterInternalUser);
  EXPECT_HRESULT_SUCCEEDED(::CoGetPSClsid(iupdaterinternal_iid, &psclsid));
  EXPECT_EQ(base::ToUpperASCII(StringFromGuid(psclsid)),
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
      MakeAndInitializeComObject<UpdaterInternalImpl>(updater_internal));

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
  bundle.Reset();
  ASSERT_HRESULT_SUCCEEDED(dispatch.CopyTo(IsSystemInstall(scope)
                                               ? __uuidof(IAppBundleWebSystem)
                                               : __uuidof(IAppBundleWebUser),
                                           IID_PPV_ARGS_Helper(&bundle)));
  EXPECT_HRESULT_SUCCEEDED(bundle->initialize());

  bundle_web = bundle;
}

HRESULT DoUpdate(UpdaterScope scope,
                 const base::win::ScopedBstr& appid,
                 AppBundleWebCreateMode app_bundle_web_create_mode,
                 int expected_final_state,
                 HRESULT expected_error_code,
                 bool cancel_when_downloading) {
  Microsoft::WRL::ComPtr<IAppBundleWeb> bundle;
  InitializeBundle(scope, bundle);
  EXPECT_TRUE(bundle);
  EXPECT_HRESULT_SUCCEEDED(
      app_bundle_web_create_mode == AppBundleWebCreateMode::kCreateInstalledApp
          ? bundle->createInstalledApp(appid.Get())
          : bundle->createApp(appid.Get(), base::win::ScopedBstr(L"BRND").Get(),
                              base::win::ScopedBstr(L"en").Get(),
                              base::win::ScopedBstr(L"DoUpdateAP").Get()));
  EXPECT_HRESULT_SUCCEEDED(bundle->checkForUpdate());
  bool done = false;
  static const base::TimeDelta kExpirationTimeout =
      2 * TestTimeouts::action_max_timeout();
  base::ElapsedTimer timer;

  LONG state_value = 0;
  LONG error_code = 0;
  std::wstring extra_data;
  Microsoft::WRL::ComPtr<IDispatch> app_dispatch;
  EXPECT_HRESULT_SUCCEEDED(bundle->get_appWeb(0, &app_dispatch));
  Microsoft::WRL::ComPtr<IAppWeb> app;
  EXPECT_HRESULT_SUCCEEDED(app_dispatch.As(&app));
  app.Reset();
  EXPECT_HRESULT_SUCCEEDED(app_dispatch.CopyTo(
      IsSystemInstall(scope) ? __uuidof(IAppWebSystem) : __uuidof(IAppWebUser),
      IID_PPV_ARGS_Helper(&app)));

  if (app_bundle_web_create_mode == AppBundleWebCreateMode::kCreateApp) {
    EXPECT_HRESULT_SUCCEEDED(app->put_serverInstallDataIndex(
        base::win::ScopedBstr(L"expected_install_data_index").Get()));
    base::win::ScopedBstr install_data_index;
    EXPECT_HRESULT_SUCCEEDED(
        app->get_serverInstallDataIndex(install_data_index.Receive()));
    EXPECT_STREQ(install_data_index.Get(), L"expected_install_data_index");
  }

  while (!done && (timer.Elapsed() < kExpirationTimeout)) {
    Microsoft::WRL::ComPtr<IDispatch> state_dispatch;
    EXPECT_HRESULT_SUCCEEDED(app->get_currentState(&state_dispatch));
    Microsoft::WRL::ComPtr<ICurrentState> state;
    EXPECT_HRESULT_SUCCEEDED(state_dispatch.As(&state));
    state.Reset();
    EXPECT_HRESULT_SUCCEEDED(state_dispatch.CopyTo(
        IsSystemInstall(scope) ? __uuidof(ICurrentStateSystem)
                               : __uuidof(ICurrentStateUser),
        IID_PPV_ARGS_Helper(&state)));
    EXPECT_HRESULT_SUCCEEDED(state->get_stateValue(&state_value));
    EXPECT_HRESULT_SUCCEEDED(state->get_errorCode(&error_code));

    std::wstring state_description;
    done = state_value == expected_final_state;
    switch (state_value) {
      case STATE_INIT:
        state_description = L"Initializating...";
        break;

      case STATE_WAITING_TO_CHECK_FOR_UPDATE:
      case STATE_CHECKING_FOR_UPDATE: {
        state_description = L"Checking for update...";
        Microsoft::WRL::ComPtr<IDispatch> current_version_web_dispatch;
        EXPECT_HRESULT_SUCCEEDED(
            app->get_currentVersionWeb(&current_version_web_dispatch));
        extra_data = base::StrCat(
            {L"[Current Version: ",
             GetAppVersionWebString(current_version_web_dispatch), L"]"});
        break;
      }

      case STATE_UPDATE_AVAILABLE: {
        state_description = L"Update available!";
        Microsoft::WRL::ComPtr<IDispatch> next_version_web_dispatch;
        EXPECT_HRESULT_SUCCEEDED(
            app->get_nextVersionWeb(&next_version_web_dispatch));
        extra_data = base::StrCat(
            {L"[Next Version: ",
             GetAppVersionWebString(next_version_web_dispatch), L"]"});
        if (!done) {
          EXPECT_HRESULT_SUCCEEDED(bundle->download());
        }

        break;
      }

      case STATE_WAITING_TO_DOWNLOAD:
      case STATE_RETRYING_DOWNLOAD:
        state_description = L"Contacting server...";
        break;

      case STATE_DOWNLOADING: {
        state_description = L"Downloading...";
        ULONG bytes_downloaded = 0;
        state->get_bytesDownloaded(&bytes_downloaded);
        ULONG total_bytes_to_download = 0;
        state->get_totalBytesToDownload(&total_bytes_to_download);
        LONG download_time_remaining_ms = 0;
        state->get_downloadTimeRemainingMs(&download_time_remaining_ms);
        extra_data = base::ASCIIToWide(base::StringPrintf(
            "[Bytes downloaded: %lu][Bytes total: %lu][Time remaining: %ld]",
            bytes_downloaded, total_bytes_to_download,
            download_time_remaining_ms));

        if (cancel_when_downloading) {
          EXPECT_HRESULT_SUCCEEDED(bundle->cancel());
        }

        break;
      }

      case STATE_DOWNLOAD_COMPLETE:
      case STATE_EXTRACTING:
      case STATE_APPLYING_DIFFERENTIAL_PATCH:
      case STATE_READY_TO_INSTALL: {
        state_description = L"Ready to install!";
        ULONG bytes_downloaded = 0;
        state->get_bytesDownloaded(&bytes_downloaded);
        ULONG total_bytes_to_download = 0;
        state->get_totalBytesToDownload(&total_bytes_to_download);
        extra_data = base::ASCIIToWide(
            base::StringPrintf("[Bytes downloaded: %lu][Bytes total: %lu]",
                               bytes_downloaded, total_bytes_to_download));
        EXPECT_HRESULT_SUCCEEDED(bundle->install());
        break;
      }

      case STATE_WAITING_TO_INSTALL:
      case STATE_INSTALLING: {
        state_description = L"Installing...";
        LONG install_progress = 0;
        state->get_installProgress(&install_progress);
        LONG install_time_remaining_ms = 0;
        state->get_installTimeRemainingMs(&install_time_remaining_ms);
        extra_data = base::ASCIIToWide(
            base::StringPrintf("[Install Progress: %ld][Time remaining: %ld]",
                               install_progress, install_time_remaining_ms));
        break;
      }

      case STATE_INSTALL_COMPLETE:
        state_description = L"Done!";
        break;

      case STATE_PAUSED:
        state_description = L"Paused...";
        break;

      case STATE_NO_UPDATE:
        state_description = L"No update available!";
        break;

      case STATE_ERROR: {
        state_description = L"Error!";
        base::win::ScopedBstr completion_message;
        EXPECT_HRESULT_SUCCEEDED(
            state->get_completionMessage(completion_message.Receive()));
        LONG installer_result_code = 0;
        EXPECT_HRESULT_SUCCEEDED(
            state->get_installerResultCode(&installer_result_code));
        extra_data = base::ASCIIToWide(base::StringPrintf(
            "[errorCode: %ld][completionMessage: %ls][installerResultCode: "
            "%ld]",
            error_code, completion_message.Get(), installer_result_code));
        break;
      }

      default:
        state_description = L"Unhandled state...";
        break;
    }

    base::PlatformThread::Sleep(base::Seconds(1));
  }

  EXPECT_TRUE(done)
      << "The test timed out, consider increasing kExpirationTimeout which is: "
      << kExpirationTimeout << ": " << extra_data;
  EXPECT_EQ(expected_final_state, state_value) << extra_data;
  EXPECT_EQ(expected_error_code, error_code) << extra_data;
  return S_OK;
}

void ExpectLegacyUpdate3WebSucceeds(
    UpdaterScope scope,
    const std::string& app_id,
    AppBundleWebCreateMode app_bundle_web_create_mode,
    int expected_final_state,
    int expected_error_code,
    bool cancel_when_downloading) {
  EXPECT_HRESULT_SUCCEEDED(
      DoUpdate(scope, base::win::ScopedBstr(base::UTF8ToWide(app_id).c_str()),
               app_bundle_web_create_mode, expected_final_state,
               expected_error_code, cancel_when_downloading));
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

  // Enable usage stats to allow this launch command to send an app command
  // ping.
  base::win::RegKey client_state_key(
      CreateAppClientStateKey(UpdaterScope::kSystem, app_id));
  EXPECT_EQ(client_state_key.WriteValue(L"usagestats", 1), ERROR_SUCCESS);
}

void DeleteLaunchCommandElevated(const std::wstring& app_id,
                                 const std::wstring& command_id) {
  DeleteAppClientStateKey(UpdaterScope::kSystem, app_id);
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
  if (FAILED(hr)) {
    return hr;
  }

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
  if (!IsSystemInstall(scope)) {
    return;
  }

  Microsoft::WRL::ComPtr<IUnknown> unknown;
  ASSERT_HRESULT_SUCCEEDED(
      CreateLocalServer(__uuidof(ProcessLauncherClass), unknown));
  Microsoft::WRL::ComPtr<IProcessLauncher> process_launcher;
  EXPECT_HRESULT_SUCCEEDED(unknown.As(&process_launcher));
  process_launcher.Reset();
  EXPECT_HRESULT_SUCCEEDED(
      unknown.CopyTo(__uuidof(IProcessLauncherSystem),
                     IID_PPV_ARGS_Helper(&process_launcher)));

  Microsoft::WRL::ComPtr<IProcessLauncher2> process_launcher2;
  EXPECT_HRESULT_SUCCEEDED(unknown.As(&process_launcher2));
  process_launcher2.Reset();
  EXPECT_HRESULT_SUCCEEDED(
      unknown.CopyTo(__uuidof(IProcessLauncher2System),
                     IID_PPV_ARGS_Helper(&process_launcher2)));

  static constexpr wchar_t kAppId1[] =
      L"{831EF4D0-B729-4F61-AA34-91526481799D}";
  static constexpr wchar_t kCommandId[] = L"cmd";

  // Succeeds when the command is present in the registry.
  base::ScopedTempDir temp_dir;
  SetupLaunchCommandElevated(kAppId1, L"" BROWSER_PRODUCT_NAME_STRING,
                             L"1.0.0.0", kCommandId, L" /c \"exit 5420\"",
                             temp_dir);

  // Succeeds when the command is present in the registry.
  ASSERT_HRESULT_SUCCEEDED(
      ProcessLaunchCmdElevated(process_launcher, kAppId1, kCommandId, 5420));

  // Allows time for the app command ping to be sent.
  base::PlatformThread::Sleep(base::Seconds(1));

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
  app.Reset();
  ASSERT_HRESULT_SUCCEEDED(app_dispatch.CopyTo(
      IsSystemInstall(scope) ? __uuidof(IAppWebSystem) : __uuidof(IAppWebUser),
      IID_PPV_ARGS_Helper(&app)));

  Microsoft::WRL::ComPtr<IDispatch> command_dispatch;
  ASSERT_HRESULT_SUCCEEDED(app->get_command(
      base::win::ScopedBstr(commandid).Get(), &command_dispatch));
  Microsoft::WRL::ComPtr<IAppCommandWeb> app_command_web;
  ASSERT_HRESULT_SUCCEEDED(command_dispatch.As(&app_command_web));
  app_command_web.Reset();
  ASSERT_HRESULT_SUCCEEDED(command_dispatch.CopyTo(
      IsSystemInstall(scope) ? __uuidof(IAppCommandWebSystem)
                             : __uuidof(IAppCommandWebUser),
      IID_PPV_ARGS_Helper(&app_command_web)));

  std::vector<base::win::ScopedVariant> variant_params;
  variant_params.reserve(kMaxParameters);
  base::ranges::transform(parameters, std::back_inserter(variant_params),
                          [](const auto& param) {
                            return base::win::ScopedVariant(
                                base::UTF8ToWide(param.GetString()).c_str());
                          });
  for (size_t i = parameters.size(); i < kMaxParameters; ++i) {
    variant_params.emplace_back(base::win::ScopedVariant::kEmptyVariant);
  }

  ASSERT_HRESULT_SUCCEEDED(app_command_web->execute(
      variant_params[0], variant_params[1], variant_params[2],
      variant_params[3], variant_params[4], variant_params[5],
      variant_params[6], variant_params[7], variant_params[8]));

  EXPECT_TRUE(WaitFor([&] {
    UINT status = 0;
    EXPECT_HRESULT_SUCCEEDED(app_command_web->get_status(&status));
    return status == COMMAND_STATUS_COMPLETE;
  }));

  DWORD exit_code = 0;
  EXPECT_HRESULT_SUCCEEDED(app_command_web->get_exitCode(&exit_code));
  EXPECT_EQ(exit_code, static_cast<DWORD>(expected_exit_code));

  // Now also run the AppCommand using the IDispatch methods.
  command_dispatch.Reset();
  ASSERT_HRESULT_SUCCEEDED(app->get_command(
      base::win::ScopedBstr(commandid).Get(), &command_dispatch));

  CallDispatchMethod(command_dispatch, L"execute", variant_params);

  EXPECT_TRUE(WaitFor([&] {
    base::win::ScopedVariant status =
        GetDispatchProperty(command_dispatch, L"status");
    return V_UINT(status.ptr()) == COMMAND_STATUS_COMPLETE;
  }));

  base::win::ScopedVariant command_exit_code =
      GetDispatchProperty(command_dispatch, L"exitCode");
  EXPECT_EQ(V_UI4(command_exit_code.ptr()),
            static_cast<DWORD>(expected_exit_code));

  DeleteAppClientKey(scope, appid);
}

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

void ExpectLegacyPolicyStatusSucceeds(UpdaterScope scope) {
  Microsoft::WRL::ComPtr<IUnknown> policy_status_server;
  ASSERT_HRESULT_SUCCEEDED(CreateLocalServer(
      IsSystemInstall(scope) ? __uuidof(PolicyStatusSystemClass)
                             : __uuidof(PolicyStatusUserClass),
      policy_status_server));
  Microsoft::WRL::ComPtr<IPolicyStatus2> policy_status2;
  ASSERT_HRESULT_SUCCEEDED(policy_status_server.As(&policy_status2));
  policy_status2.Reset();
  ASSERT_HRESULT_SUCCEEDED(policy_status_server.CopyTo(
      IsSystemInstall(scope) ? __uuidof(IPolicyStatus2System)
                             : __uuidof(IPolicyStatus2User),
      IID_PPV_ARGS_Helper(&policy_status2)));

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
  ExpectPolicyStatusValues(policy_status_value, L"Default", L"270",
                           VARIANT_FALSE);

  const base::win::ScopedBstr test_app(L"test1");
  policy_status_value.Reset();
  ASSERT_HRESULT_SUCCEEDED(policy_status2->get_effectivePolicyForAppInstalls(
      test_app.Get(), &policy_status_value));
  ExpectPolicyStatusValues(policy_status_value, L"Default", L"1",
                           VARIANT_FALSE);

  policy_status_value.Reset();
  ASSERT_HRESULT_SUCCEEDED(policy_status2->get_effectivePolicyForAppUpdates(
      test_app.Get(), &policy_status_value));
  ExpectPolicyStatusValues(policy_status_value, L"Default", L"1",
                           VARIANT_FALSE);

  policy_status_value.Reset();
  ASSERT_HRESULT_SUCCEEDED(policy_status2->get_isRollbackToTargetVersionAllowed(
      test_app.Get(), &policy_status_value));
  ExpectPolicyStatusValues(policy_status_value, L"Default", L"false",
                           VARIANT_FALSE);

  ASSERT_HRESULT_SUCCEEDED(policy_status2->refreshPolicies());
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

base::FilePath GetRealUpdaterLowerVersionPath() {
  base::FilePath exe_path;
  EXPECT_TRUE(base::PathService::Get(base::DIR_EXE, &exe_path));
  base::FilePath old_updater_path =
      exe_path.Append(FILE_PATH_LITERAL("old_updater"));

#if BUILDFLAG(CHROMIUM_BRANDING)
#if defined(ARCH_CPU_ARM64)
  old_updater_path =
      old_updater_path.Append(FILE_PATH_LITERAL("chromium_win_arm64"));
#elif defined(ARCH_CPU_X86_64)
  old_updater_path =
      old_updater_path.Append(FILE_PATH_LITERAL("chromium_win_x86_64"));
#elif defined(ARCH_CPU_X86)
  old_updater_path =
      old_updater_path.Append(FILE_PATH_LITERAL("chromium_win_x86"));
#endif
#elif BUILDFLAG(GOOGLE_CHROME_BRANDING)
#if defined(ARCH_CPU_ARM64)
  old_updater_path =
      old_updater_path.Append(FILE_PATH_LITERAL("chrome_win_arm64"));
#elif defined(ARCH_CPU_X86_64)
  old_updater_path =
      old_updater_path.Append(FILE_PATH_LITERAL("chrome_win_x86_64"));
#elif defined(ARCH_CPU_X86)
  old_updater_path =
      old_updater_path.Append(FILE_PATH_LITERAL("chrome_win_x86"));
#endif
#endif

#if BUILDFLAG(CHROMIUM_BRANDING) || BUILDFLAG(GOOGLE_CHROME_BRANDING)
  old_updater_path = old_updater_path.Append(FILE_PATH_LITERAL("cipd"));
#endif
  return old_updater_path.Append(FILE_PATH_LITERAL("UpdaterSetup_test.exe"));
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
  const std::optional<base::FilePath> installed_executable_path =
      GetUpdaterExecutablePath(scope);
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

std::wstring GetFakeUpdaterTaskName(UpdaterScope scope,
                                    const std::wstring& type) {
  return base::StrCat({IsSystemInstall(scope) ? kLegacyTaskNamePrefixSystem
                                              : kLegacyTaskNamePrefixUser,
                       type, base::NumberToWString(::GetCurrentProcessId())});
}

void SetupFakeLegacyUpdater(UpdaterScope scope) {
  const HKEY root = UpdaterScopeToHKeyRoot(scope);

  base::win::RegKey key;
  ASSERT_EQ(key.Create(root,
                       base::StrCat(
                           {UPDATER_KEY L"Clients\\", kLegacyGoogleUpdateAppID})
                           .c_str(),
                       Wow6432(KEY_WRITE)),
            ERROR_SUCCESS);
  ASSERT_EQ(key.WriteValue(kRegValuePV, L"1.1.1.1"), ERROR_SUCCESS);
  ASSERT_EQ(key.WriteValue(kRegValueBrandCode, L"GOOG"), ERROR_SUCCESS);
  ASSERT_EQ(key.WriteValue(kRegValueAP, L"TestAP"), ERROR_SUCCESS);
  key.Close();

  ASSERT_EQ(
      key.Create(
          root,
          UPDATER_KEY L"\\Clients\\{8A69D345-D564-463C-AFF1-A69D9E530F96}",
          Wow6432(KEY_WRITE)),
      ERROR_SUCCESS);
  ASSERT_EQ(key.WriteValue(kRegValuePV, L"99.0.0.1"), ERROR_SUCCESS);
  key.Close();

  ASSERT_EQ(
      key.Create(
          root,
          UPDATER_KEY L"\\ClientState\\{8A69D345-D564-463C-AFF1-A69D9E530F96}",
          Wow6432(KEY_WRITE)),
      ERROR_SUCCESS);
  ASSERT_EQ(key.WriteValue(kRegValueBrandCode, L"GGLS"), ERROR_SUCCESS);
  ASSERT_EQ(key.WriteValue(kRegValueAP, L"TestAP"), ERROR_SUCCESS);
  ASSERT_EQ(key.WriteValue(kRegValueDateOfLastActivity, 0xFFFFFFFF),
            ERROR_SUCCESS);
  ASSERT_EQ(key.WriteValue(kRegValueDateOfLastRollcall, 5929), ERROR_SUCCESS);
  key.Close();

  ASSERT_EQ(key.Create(
                root,
                UPDATER_KEY L"\\ClientState"
                            L"\\{8A69D345-D564-463C-AFF1-A69D9E530F96}\\cohort",
                Wow6432(KEY_WRITE)),
            ERROR_SUCCESS);
  ASSERT_EQ(key.WriteValue(nullptr, L"TestCohort"), ERROR_SUCCESS);
  ASSERT_EQ(key.WriteValue(kRegValueCohortName, L"TestCohortName"),
            ERROR_SUCCESS);
  ASSERT_EQ(key.WriteValue(kRegValueCohortHint, L"TestCohortHint"),
            ERROR_SUCCESS);
  key.Close();

  ASSERT_EQ(
      key.Create(
          root,
          UPDATER_KEY L"\\Clients\\{fc54d8f9-b6fd-4274-92eb-c4335cd8761e}",
          Wow6432(KEY_WRITE)),
      ERROR_SUCCESS);
  ASSERT_EQ(key.WriteValue(kRegValueBrandCode, L"GGLS"), ERROR_SUCCESS);
  ASSERT_EQ(key.WriteValue(kRegValueAP, L"TestAP"), ERROR_SUCCESS);
  ASSERT_EQ(key.WriteValue(kRegValueDateOfLastActivity, L"5900"),
            ERROR_SUCCESS);
  key.Close();

  if (IsSystemInstall(scope)) {
    // Install mock GoogleUpdate services "gupdate" and "gupdatem".
    EXPECT_TRUE(CreateService(kLegacyServiceNamePrefix,
                              kLegacyServiceDisplayNamePrefix,
                              L"C:\\temp\\temp.exe"));
    EXPECT_TRUE(CreateService(base::StrCat({kLegacyServiceNamePrefix, L"m"}),
                              kLegacyServiceDisplayNamePrefix,
                              L"C:\\temp\\temp.exe"));
  } else {
    // Install mock GoogleUpdate run value.
    base::win::RegKey run_key;
    ASSERT_EQ(
        run_key.Open(HKEY_CURRENT_USER, REGSTR_PATH_RUN, KEY_READ | KEY_WRITE),
        ERROR_SUCCESS);
    ASSERT_EQ(run_key.WriteValue(kLegacyRunValuePrefix, L"C:\\temp\\temp.exe"),
              ERROR_SUCCESS);
  }

  // Install mock GoogleUpdate tasks.
  scoped_refptr<TaskScheduler> task_scheduler =
      TaskScheduler::CreateInstance(scope, /*use_task_subfolders=*/false);
  ASSERT_TRUE(task_scheduler);

  for (const std::wstring& task_name : {GetFakeUpdaterTaskName(scope, L"Core"),
                                        GetFakeUpdaterTaskName(scope, L"UA")}) {
    ASSERT_TRUE(task_scheduler->RegisterTask(
        task_name, task_name,
        base::CommandLine::FromString(L"C:\\temp\\temp.exe"),
        TaskScheduler::TriggerType::TRIGGER_TYPE_HOURLY, false));
  }

  // Set up a mock `GoogleUpdate.exe`, and the following mock directories:
  // `Download`, `Install`, and a versioned `1.2.3.4` directory.
  const std::optional<base::FilePath> google_update_exe =
      GetGoogleUpdateExePath(scope);
  ASSERT_TRUE(google_update_exe.has_value());
  SetupMockUpdater(google_update_exe.value());
}

void RunFakeLegacyUpdater(UpdaterScope scope) {
  const std::optional<base::FilePath> google_update_exe =
      GetGoogleUpdateExePath(scope);
  ASSERT_TRUE(base::PathExists(*google_update_exe));

  const base::FilePath exe_dir(google_update_exe->DirName());
  base::CommandLine command_line =
      GetTestProcessCommandLine(scope, test::GetTestName());
  command_line.AppendSwitchASCII(
      updater::kTestSleepSecondsSwitch,
      base::NumberToString(TestTimeouts::action_timeout().InSeconds() / 4));

  for (const base::FilePath& dir :
       {exe_dir, exe_dir.Append(L"1.2.3.4"), exe_dir.Append(L"Download"),
        exe_dir.Append(L"Install")}) {
    for (const std::wstring exe_name : {kLegacyExeName, L"mock.executable"}) {
      const base::FilePath exe(dir.Append(exe_name));
      ASSERT_TRUE(base::PathExists(exe));

      base::Process process = base::LaunchProcess(
          base::StrCat(
              {base::CommandLine::QuoteForCommandLineToArgvW(exe.value()), L" ",
               command_line.GetArgumentsString()}),
          {});
      ASSERT_TRUE(process.IsValid());
    }
  }
}

void CloseInstallCompleteDialog(const std::u16string& bundle_name,
                                const std::wstring& child_window_text_to_find,
                                bool verify_app_logo_loaded) {
  const std::wstring window_title = ui::GetInstallerDisplayName(bundle_name);
  bool found = false;
  base::Process process;
  ASSERT_TRUE(WaitFor(
      [&] {
        if (!found) {
          // Enumerate the top-level dialogs to find the setup dialog.
          base::win::EnumerateChildWindows(
              ::GetDesktopWindow(), base::BindLambdaForTesting([&](HWND hwnd) {
                if (!base::win::IsSystemDialog(hwnd) ||
                    !base::Contains(base::win::GetWindowTextString(hwnd),
                                    window_title)) {
                  return false;
                }
                // Enumerate the child windows to search for
                // `child_window_text_to_find`. If found, close the dialog.
                base::win::EnumerateChildWindows(
                    hwnd, base::BindLambdaForTesting([&](HWND hwnd) {
                      if (!base::Contains(base::win::GetWindowTextString(hwnd),
                                          child_window_text_to_find)) {
                        return false;
                      }
                      const HWND parent_hwnd = ::GetParent(hwnd);
                      if (verify_app_logo_loaded &&
                          !::SendDlgItemMessage(parent_hwnd, IDC_APP_BITMAP,
                                                STM_GETIMAGE, IMAGE_BITMAP,
                                                0)) {
                        return false;
                      }
                      found = true;
                      DWORD pid = 0;
                      EXPECT_TRUE(
                          ::GetWindowThreadProcessId(parent_hwnd, &pid));
                      process = base::Process::Open(pid);
                      EXPECT_TRUE(process.IsValid());
                      ::PostMessage(parent_hwnd, WM_CLOSE, 0, 0);
                      return found;
                    }));
                return found;
              }));
        }
        return found && !process.IsRunning();
      },
      [&] {
        VLOG(0) << "Still waiting, `found`: " << found
                << ": `process.IsRunning()`: " << process.IsRunning();
      }));
}

void ExpectLegacyUpdaterMigrated(UpdaterScope scope) {
  scoped_refptr<GlobalPrefs> global_prefs = CreateGlobalPrefs(scope);
  auto persisted_data = base::MakeRefCounted<PersistedData>(
      scope, global_prefs->GetPrefService(), nullptr);

  // Legacy updater itself should not be migrated.
  const std::string kLegacyUpdaterAppId =
      base::SysWideToUTF8(kLegacyGoogleUpdateAppID);
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
  EXPECT_EQ(persisted_data->GetDateLastActive(kNoPVAppId), -2);
  EXPECT_EQ(persisted_data->GetDateLastRollCall(kNoPVAppId), -2);

  EXPECT_EQ(persisted_data->GetProductVersion(kChromeAppId),
            base::Version("99.0.0.1"));
  EXPECT_EQ(persisted_data->GetAP(kChromeAppId), "TestAP");
  EXPECT_EQ(persisted_data->GetBrandCode(kChromeAppId), "GGLS");
  EXPECT_TRUE(persisted_data->GetFingerprint(kChromeAppId).empty());
  EXPECT_EQ(persisted_data->GetDateLastActive(kChromeAppId), -1);
  EXPECT_EQ(persisted_data->GetDateLastRollCall(kChromeAppId), 5929);
  EXPECT_EQ(persisted_data->GetCohort(kChromeAppId), "TestCohort");
  EXPECT_EQ(persisted_data->GetCohortName(kChromeAppId), "TestCohortName");
  EXPECT_EQ(persisted_data->GetCohortHint(kChromeAppId), "TestCohortHint");

  int count_entries = 0;
  if (IsSystemInstall(scope)) {
    // Expect no GoogleUpdate services.
    ForEachServiceWithPrefix(
        kLegacyServiceNamePrefix, kLegacyServiceDisplayNamePrefix,
        [&count_entries](const std::wstring& /*service_name*/) {
          ++count_entries;
        });
  } else {
    // Expect no GoogleUpdate run value.
    ForEachRegistryRunValueWithPrefix(
        kLegacyRunValuePrefix,
        [&count_entries](const std::wstring& /* run_name*/) {
          ++count_entries;
        });
  }
  EXPECT_EQ(count_entries, 0);

  // Expect no GoogleUpdate tasks.
  scoped_refptr<TaskScheduler> task_scheduler =
      TaskScheduler::CreateInstance(scope, /*use_task_subfolders=*/false);
  ASSERT_TRUE(task_scheduler);
  for (const std::wstring& task_name : {GetFakeUpdaterTaskName(scope, L"Core"),
                                        GetFakeUpdaterTaskName(scope, L"UA")}) {
    count_entries = 0;
    task_scheduler->ForEachTaskWithPrefix(
        task_name, [&count_entries](const std::wstring& /*task_name*/) {
          ++count_entries;
        });

    EXPECT_EQ(count_entries, 0);
  }

  // Expect only a single file `GoogleUpdate.exe` and nothing else under
  // `\Google\Update`.
  const std::optional<base::FilePath> google_update_exe =
      GetGoogleUpdateExePath(scope);
  ASSERT_TRUE(google_update_exe.has_value());
  ExpectOnlyMockUpdater(google_update_exe.value());
}

void InstallApp(UpdaterScope scope,
                const std::string& app_id,
                const base::Version& version) {
  base::win::RegKey key;
  ASSERT_EQ(key.Create(UpdaterScopeToHKeyRoot(scope),
                       GetAppClientsKey(app_id).c_str(), Wow6432(KEY_WRITE)),
            ERROR_SUCCESS);
  RegistrationRequest registration;
  registration.app_id = app_id;
  registration.version = version;
  RegisterApp(scope, registration);
}

void UninstallApp(UpdaterScope scope, const std::string& app_id) {
  base::win::RegKey key;
  ASSERT_EQ(
      key.Open(UpdaterScopeToHKeyRoot(scope), CLIENTS_KEY, Wow6432(DELETE)),
      ERROR_SUCCESS);
  LONG result = key.DeleteKey(base::SysUTF8ToWide(app_id).c_str());
  ASSERT_TRUE(result == ERROR_SUCCESS || result == ERROR_FILE_NOT_FOUND);
}

void RunOfflineInstall(UpdaterScope scope,
                       bool is_legacy_install,
                       bool is_silent_install) {
  static constexpr char kManifestFormat[] =
      R"(<?xml version="1.0" encoding="UTF-8"?>
<response protocol="3.0">
  <systemrequirements platform="win"/>
  <app appid="%ls" status="ok">
    <updatecheck status="ok">
      <manifest version="%s">
        <packages>
          <package hash_sha256="sha256hash_foobar"
            name="%s" required="true" size="%)" PRId64 R"("/>
        </packages>
        <actions>
          <action event="install"
            run="%s"/>
        </actions>
      </manifest>
    </updatecheck>
    <data index="verboselogging" name="install" status="ok">
      {"distribution": { "verbose_logging": true}}
    </data>
  </app>
</response>)";
  RunOfflineInstallWithManifest(scope, is_legacy_install, is_silent_install,
                                kManifestFormat,
                                IDS_BUNDLE_INSTALLED_SUCCESSFULLY_BASE, true);
}

void RunOfflineInstallOsNotSupported(UpdaterScope scope,
                                     bool is_legacy_install,
                                     bool is_silent_install) {
  static constexpr char kManifestFormat[] =
      R"(<?xml version="1.0" encoding="UTF-8"?>
<response protocol="3.0">
  <systemrequirements platform="minix"/>
  <app appid="%ls" status="ok">
    <updatecheck status="ok">
      <manifest version="%s">
        <packages>
          <package hash_sha256="sha256hash_foobar"
            name="%s" required="true" size="%)" PRId64 R"("/>
        </packages>
        <actions>
          <action event="install"
            run="%s"/>
        </actions>
      </manifest>
    </updatecheck>
    <data index="verboselogging" name="install" status="ok">
      {"distribution": { "verbose_logging": true}}
    </data>
  </app>
</response>)";
  RunOfflineInstallWithManifest(scope, is_legacy_install, is_silent_install,
                                kManifestFormat,
                                IDS_UPDATER_OS_NOT_SUPPORTED_BASE, false);
}

base::CommandLine MakeElevated(base::CommandLine command_line) {
  return command_line;
}

void SetPlatformPolicies(const base::Value::Dict& values) {
  base::win::RegKey policy_key;
  ASSERT_EQ(ERROR_SUCCESS,
            policy_key.Create(HKEY_LOCAL_MACHINE, UPDATER_POLICIES_KEY,
                              KEY_SET_VALUE));

  for (const auto [app_id, policies] : values) {
    ASSERT_TRUE(policies.is_dict());
    for (const auto [name, value] : policies.GetDict()) {
      const std::wstring& key = base::ASCIIToWide(
          base::StringPrintf("%s%s", name.c_str(), app_id.c_str()));
      if (value.is_string()) {
        policy_key.WriteValue(key.c_str(),
                              base::ASCIIToWide(value.GetString()).c_str());
      } else if (value.is_int()) {
        policy_key.WriteValue(key.c_str(), static_cast<DWORD>(value.GetInt()));
      } else if (value.is_bool()) {
        policy_key.WriteValue(key.c_str(), static_cast<DWORD>(value.GetBool()));
      }
    }
  }
}

void ExpectAppVersion(UpdaterScope scope,
                      const std::string& app_id,
                      const base::Version& version) {
  const base::Version app_version =
      base::MakeRefCounted<PersistedData>(
          scope, CreateGlobalPrefsForTesting(scope)->GetPrefService(), nullptr)
          ->GetProductVersion(app_id);
  EXPECT_TRUE(app_version.IsValid());
  EXPECT_EQ(version, app_version);

  std::wstring pv;
  EXPECT_EQ(
      ERROR_SUCCESS,
      base::win::RegKey(UpdaterScopeToHKeyRoot(scope),
                        GetAppClientStateKey(app_id).c_str(), Wow6432(KEY_READ))
          .ReadValue(kRegValuePV, &pv));
  EXPECT_EQ(base::SysUTF8ToWide(version.GetString()), pv);
}

}  // namespace updater::test
