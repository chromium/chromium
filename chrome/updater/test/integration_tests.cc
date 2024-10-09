// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <cstdlib>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "base/files/file_enumerator.h"
#include "base/files/file_path.h"
#include "base/files/scoped_temp_dir.h"
#include "base/files/scoped_temp_file.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/functional/function_ref.h"
#include "base/json/json_file_value_serializer.h"
#include "base/json/json_reader.h"
#include "base/memory/scoped_refptr.h"
#include "base/path_service.h"
#include "base/run_loop.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/bind.h"
#include "base/test/scoped_run_loop_timeout.h"
#include "base/test/task_environment.h"
#include "base/test/test_timeouts.h"
#include "base/threading/platform_thread.h"
#include "base/time/time.h"
#include "base/values.h"
#include "base/version.h"
#include "build/branding_buildflags.h"
#include "build/build_config.h"
#include "build/buildflag.h"
#include "chrome/enterprise_companion/device_management_storage/dm_storage.h"
#include "chrome/enterprise_companion/installer_paths.h"
#include "chrome/updater/constants.h"
#include "chrome/updater/device_management/dm_policy_builder_for_testing.h"
#include "chrome/updater/ipc/ipc_support.h"
#include "chrome/updater/policy/dm_policy_manager.h"
#include "chrome/updater/protos/omaha_settings.pb.h"
#include "chrome/updater/registration_data.h"
#include "chrome/updater/service_proxy_factory.h"
#include "chrome/updater/test/integration_test_commands.h"
#include "chrome/updater/test/integration_tests_impl.h"
#include "chrome/updater/test/request_matcher.h"
#include "chrome/updater/test/server.h"
#include "chrome/updater/test/test_scope.h"
#include "chrome/updater/test/unit_test_util.h"
#include "chrome/updater/update_service.h"
#include "chrome/updater/updater_branding.h"
#include "chrome/updater/updater_version.h"
#include "chrome/updater/util/util.h"
#include "components/policy/proto/device_management_backend.pb.h"
#include "components/update_client/protocol_definition.h"
#include "components/update_client/update_client.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/cleanup/cleanup.h"
#include "url/gurl.h"

#if BUILDFLAG(IS_LINUX)
#include <unistd.h>

#include "base/environment.h"
#include "base/strings/strcat.h"
#include "chrome/updater/util/posix_util.h"
#endif

#if BUILDFLAG(IS_MAC)
#include "chrome/updater/test/integration_tests_mac.h"
#include "chrome/updater/util/mac_util.h"
#endif

#if BUILDFLAG(IS_WIN)
#include <shlobj.h>
#include <wrl/client.h>

#include "base/strings/utf_string_conversions.h"
#include "base/win/com_init_util.h"
#include "base/win/registry.h"
#include "base/win/scoped_bstr.h"
#include "chrome/updater/app/server/win/updater_idl.h"
#include "chrome/updater/app/server/win/updater_internal_idl.h"
#include "chrome/updater/app/server/win/updater_legacy_idl.h"
#include "chrome/updater/test/test_scope.h"
#include "chrome/updater/util/win_util.h"
#include "chrome/updater/win/setup/setup_util.h"
#include "chrome/updater/win/ui/l10n_util.h"
#include "chrome/updater/win/ui/resources/updater_installer_strings.h"
#include "chrome/updater/win/win_constants.h"
#endif  // BUILDFLAG(IS_WIN)

namespace updater::test {
namespace {

namespace enterprise_management =
    ::wireless_android_enterprise_devicemanagement;
using enterprise_management::ApplicationSettings;
using enterprise_management::OmahaSettingsClientProto;

void ExpectNoUpdateSequence(ScopedServer* test_server,
                            const std::string& app_id) {
  test_server->ExpectOnce({request::GetUpdaterUserAgentMatcher(),
                           request::GetContentMatcher({base::StringPrintf(
                               R"(.*"appid":"%s".*)", app_id.c_str())})},
                          base::StringPrintf(")]}'\n"
                                             R"({"response":{)"
                                             R"(  "protocol":"3.1",)"
                                             R"(  "app":[)"
                                             R"(    {)"
                                             R"(      "appid":"%s",)"
                                             R"(      "status":"ok",)"
                                             R"(      "updatecheck":{)"
                                             R"(        "status":"noupdate")"
                                             R"(      })"
                                             R"(    })"
                                             R"(  ])"
                                             R"(}})",
                                             app_id.c_str()));
}

base::FilePath GetInstallerPath(const std::string& installer) {
  return base::FilePath::FromASCII("test_installer").AppendASCII(installer);
}

struct TestApp {
  std::string appid;
  base::Version v1;
  std::string v1_crx;
  base::Version v2;
  std::string v2_crx;

  base::CommandLine GetInstallCommandSwitches(bool install_v1) const {
    base::CommandLine command(base::CommandLine::NO_PROGRAM);
    if (IsSystemInstall(GetUpdaterScopeForTesting())) {
      command.AppendArg("--system");
    }
    command.AppendSwitchASCII("--appid", appid);
    command.AppendSwitchASCII("--company", COMPANY_SHORTNAME_STRING);
    command.AppendSwitchASCII("--product_version",
                              install_v1 ? v1.GetString() : v2.GetString());
    return command;
  }

  std::string GetInstallCommandLineArgs(bool install_v1) const {
#if BUILDFLAG(IS_WIN)
    return base::WideToASCII(
        GetInstallCommandSwitches(install_v1).GetCommandLineString());
#else
    return GetInstallCommandSwitches(install_v1).GetCommandLineString();
#endif
  }

  base::CommandLine GetInstallCommandLine(bool install_v1) const {
    base::FilePath exe_path;
    base::PathService::Get(base::DIR_EXE, &exe_path);
    const base::FilePath installer_path =
        GetInstallerPath(install_v1 ? v1_crx : v2_crx);
    base::CommandLine command = GetInstallCommandSwitches(install_v1);
#if BUILDFLAG(IS_WIN)
    command.SetProgram(exe_path.Append(
        installer_path.ReplaceExtension(FILE_PATH_LITERAL(".exe"))));
#else
    command.SetProgram(exe_path.Append(
        installer_path.DirName().AppendASCII("test_app_setup.sh")));
#endif
    return command;
  }
};

}  // namespace

class IntegrationTest : public ::testing::Test {
 public:
  IntegrationTest() : test_commands_(CreateIntegrationTestCommands()) {}
  ~IntegrationTest() override = default;

 protected:
  void SetUp() override {
    VLOG(2) << __func__ << " entered.";
#if BUILDFLAG(IS_WIN) && defined(ADDRESS_SANITIZER)
    if (IsSystemInstall(GetUpdaterScopeForTesting())) {
      // TODO(crbug.com/366973330): updater_tests_system fail under Win/ASan.
      GTEST_SKIP() << "Skipping on Windows/ASan";
    }
#endif

    ASSERT_NO_FATAL_FAILURE(CleanProcesses());
    ASSERT_TRUE(WaitForUpdaterExit());
    ASSERT_NO_FATAL_FAILURE(Clean());
    ASSERT_NO_FATAL_FAILURE(ExpectClean());
    ASSERT_NO_FATAL_FAILURE(EnterTestMode(
        GURL("http://localhost:1234"), GURL("http://localhost:1235"),
        GURL("http://localhost:1236"), {}, base::Minutes(5)));
    ASSERT_NO_FATAL_FAILURE(SetMachineManaged(false));
#if BUILDFLAG(IS_LINUX)
    // On LUCI the XDG_RUNTIME_DIR and DBUS_SESSION_BUS_ADDRESS environment
    // variables may not be set. These are required for systemctl to connect to
    // its bus in user mode.
    std::unique_ptr<base::Environment> env = base::Environment::Create();
    const std::string xdg_runtime_dir =
        base::StrCat({"/run/user/", base::NumberToString(getuid())});
    if (!env->HasVar("XDG_RUNTIME_DIR")) {
      ASSERT_TRUE(env->SetVar("XDG_RUNTIME_DIR", xdg_runtime_dir));
    }
    if (!env->HasVar("DBUS_SESSION_BUS_ADDRESS")) {
      ASSERT_TRUE(
          env->SetVar("DBUS_SESSION_BUS_ADDRESS",
                      base::StrCat({"unix:path=", xdg_runtime_dir, "/bus"})));
    }
#endif

    // Mark the device as de-registered. This stops sending DM requests
    // that mess up the request expectations in the mock server.
    ASSERT_NO_FATAL_FAILURE(DMDeregisterDevice());

    // Abandon the test if there are any non-fatal errors during setup.
    ASSERT_FALSE(HasFailure());

    VLOG(2) << __func__ << "completed.";
  }

  void TearDown() override {
    VLOG(2) << __func__ << " entered.";
    if (IsSkipped()) {
      return;
    }

    ExitTestMode();
    if (!HasFailure()) {
      ExpectClean();
    }
    ExpectNoCrashes();

    PrintLog();
    CopyLog();

    DMCleanup();

    // Updater process must not be running for `Clean()` to succeed.
    ASSERT_TRUE(WaitForUpdaterExit());
    Clean();

    VLOG(2) << __func__ << "completed.";
  }

  void ExpectNoCrashes() { test_commands_->ExpectNoCrashes(); }

  void CopyLog() { test_commands_->CopyLog(/*infix=*/""); }

  void PrintLog() { test_commands_->PrintLog(); }

  void Install(const base::Value::List& switches = {}) {
    test_commands_->Install(switches);
  }

  void InstallUpdaterAndApp(const std::string& app_id,
                            const bool is_silent_install,
                            const std::string& tag,
                            const std::string& child_window_text_to_find = {},
                            const bool always_launch_cmd = false,
                            const bool verify_app_logo_loaded = false,
                            const bool expect_success = true,
                            const bool wait_for_the_installer = true) {
    test_commands_->InstallUpdaterAndApp(
        app_id, is_silent_install, tag, child_window_text_to_find,
        always_launch_cmd, verify_app_logo_loaded, expect_success,
        wait_for_the_installer);
  }

  void ExpectInstalled() { test_commands_->ExpectInstalled(); }

  void Uninstall() {
    ASSERT_TRUE(WaitForUpdaterExit());
    ExpectNoCrashes();
    PrintLog();
    CopyLog();
    test_commands_->Uninstall();
    ASSERT_TRUE(WaitForUpdaterExit());
  }

  void ExpectCandidateUninstalled() {
    test_commands_->ExpectCandidateUninstalled();
  }

  void Clean() { test_commands_->Clean(); }

  void ExpectClean() { test_commands_->ExpectClean(); }

  void EnterTestMode(
      const GURL& update_url,
      const GURL& crash_upload_url,
      const GURL& device_management_url,
      const GURL& app_logo_url,
      base::TimeDelta idle_timeout,
      base::TimeDelta server_keep_alive_time = base::Seconds(2),
      base::TimeDelta ceca_connection_timeout = base::Seconds(10)) {
    test_commands_->EnterTestMode(
        update_url, crash_upload_url, device_management_url, app_logo_url,
        idle_timeout, server_keep_alive_time, ceca_connection_timeout);
  }

  void ExitTestMode() { test_commands_->ExitTestMode(); }

  void SetGroupPolicies(const base::Value::Dict& values) {
    test_commands_->SetGroupPolicies(values);
  }

  void SetPlatformPolicies(const base::Value::Dict& values) {
    test_commands_->SetPlatformPolicies(values);
  }

  void SetMachineManaged(bool is_managed_device) {
    test_commands_->SetMachineManaged(is_managed_device);
  }

  void ExpectVersionActive(const std::string& version) {
    test_commands_->ExpectVersionActive(version);
  }

  void ExpectVersionNotActive(const std::string& version) {
    test_commands_->ExpectVersionNotActive(version);
  }

#if BUILDFLAG(IS_WIN)
  void ExpectInterfacesRegistered() {
    test_commands_->ExpectInterfacesRegistered();
  }

  void ExpectMarshalInterfaceSucceeds() {
    test_commands_->ExpectMarshalInterfaceSucceeds();
  }

  void ExpectLegacyUpdate3WebSucceeds(
      const std::string& app_id,
      AppBundleWebCreateMode app_bundle_web_create_mode,
      int expected_final_state,
      int expected_error_code,
      bool cancel_when_downloading = false) {
    test_commands_->ExpectLegacyUpdate3WebSucceeds(
        app_id, app_bundle_web_create_mode, expected_final_state,
        expected_error_code, cancel_when_downloading);
  }

  void ExpectLegacyProcessLauncherSucceeds() {
    test_commands_->ExpectLegacyProcessLauncherSucceeds();
  }

  void ExpectLegacyAppCommandWebSucceeds(const std::string& app_id,
                                         const std::string& command_id,
                                         const base::Value::List& parameters,
                                         int expected_exit_code) {
    test_commands_->ExpectLegacyAppCommandWebSucceeds(
        app_id, command_id, parameters, expected_exit_code);
  }

  void ExpectLegacyPolicyStatusSucceeds() {
    test_commands_->ExpectLegacyPolicyStatusSucceeds();
  }

  void RunUninstallCmdLine() { test_commands_->RunUninstallCmdLine(); }

  void RunHandoff(const std::string& app_id) {
    test_commands_->RunHandoff(app_id);
  }

#endif  // BUILDFLAG(IS_WIN)

  void InstallAppViaService(
      const std::string& app_id,
      const base::Value::Dict& expected_final_values = {}) {
    test_commands_->InstallAppViaService(app_id, expected_final_values);
  }

  void SetupFakeUpdaterHigherVersion() {
    test_commands_->SetupFakeUpdaterHigherVersion();
  }

  void SetupFakeUpdaterLowerVersion() {
    test_commands_->SetupFakeUpdaterLowerVersion();
  }

  void SetupRealUpdaterLowerVersion() {
    test_commands_->SetupRealUpdaterLowerVersion();
  }

  void SetActive(const std::string& app_id) {
    test_commands_->SetActive(app_id);
  }

  void ExpectActive(const std::string& app_id) {
    test_commands_->ExpectActive(app_id);
  }

  void ExpectNotActive(const std::string& app_id) {
    test_commands_->ExpectNotActive(app_id);
  }

  void SetExistenceCheckerPath(const std::string& app_id,
                               const base::FilePath& path) {
    test_commands_->SetExistenceCheckerPath(app_id, path);
  }

  void SetServerStarts(int value) { test_commands_->SetServerStarts(value); }

  void FillLog() { test_commands_->FillLog(); }

  void ExpectLogRotated() { test_commands_->ExpectLogRotated(); }

  void ExpectRegistered(const std::string& app_id) {
    test_commands_->ExpectRegistered(app_id);
  }

  void ExpectNotRegistered(const std::string& app_id) {
    test_commands_->ExpectNotRegistered(app_id);
  }

  void ExpectAppTag(const std::string& app_id, const std::string& tag) {
    test_commands_->ExpectAppTag(app_id, tag);
  }

  void SetAppTag(const std::string& app_id, const std::string& tag) {
    test_commands_->SetAppTag(app_id, tag);
  }

  void ExpectAppVersion(const std::string& app_id,
                        const base::Version& version) {
    test_commands_->ExpectAppVersion(app_id, version);
  }

  void InstallApp(const std::string& app_id,
                  const base::Version& version = base::Version("0.1")) {
    test_commands_->InstallApp(app_id, version);
  }

  void UninstallApp(const std::string& app_id) {
    test_commands_->UninstallApp(app_id);
  }

  void RunWake(int exit_code) {
    ASSERT_TRUE(WaitForUpdaterExit());
    test_commands_->RunWake(exit_code);
  }

  void RunWakeAll() {
    ASSERT_TRUE(WaitForUpdaterExit());
    test_commands_->RunWakeAll();
  }

  void RunCrashMe() { test_commands_->RunCrashMe(); }

  void RunWakeActive(int exit_code) {
    ASSERT_TRUE(WaitForUpdaterExit());
    test_commands_->RunWakeActive(exit_code);
  }

  void RunServer(int exit_code, bool internal) {
    ASSERT_TRUE(WaitForUpdaterExit());
    test_commands_->RunServer(exit_code, internal);
  }

  void CheckForUpdate(const std::string& app_id) {
    test_commands_->CheckForUpdate(app_id);
  }

  void Update(const std::string& app_id,
              const std::string& install_data_index) {
    test_commands_->Update(app_id, install_data_index);
  }

  void UpdateAll() { test_commands_->UpdateAll(); }

  void GetAppStates(const base::Value::Dict& expected_app_states) {
    test_commands_->GetAppStates(expected_app_states);
  }

  void DeleteUpdaterDirectory() { test_commands_->DeleteUpdaterDirectory(); }

  void DeleteActiveUpdaterExecutable() {
    test_commands_->DeleteActiveUpdaterExecutable();
  }

  void DeleteFile(const base::FilePath& path) {
    test_commands_->DeleteFile(path);
  }

  base::FilePath GetDifferentUserPath() {
    return test_commands_->GetDifferentUserPath();
  }

  void ExpectUpdateCheckRequest(ScopedServer* test_server) {
    test_commands_->ExpectUpdateCheckRequest(test_server);
  }

  void ExpectUpdateCheckSequence(ScopedServer* test_server,
                                 const std::string& app_id,
                                 UpdateService::Priority priority,
                                 const base::Version& from_version,
                                 const base::Version& to_version) {
    test_commands_->ExpectUpdateCheckSequence(test_server, app_id, priority,
                                              from_version, to_version);
  }

  void ExpectUninstallPing(ScopedServer* test_server,
                           std::optional<GURL> target_url = {}) {
    test_commands_->ExpectPing(test_server,
                               update_client::protocol_request::kEventUninstall,
                               target_url);
  }

  void ExpectAppCommandPing(ScopedServer* test_server,
                            const std::string& appid,
                            const std::string& appcommandid,
                            int errorcode,
                            int eventresult,
                            int event_type,
                            const base::Version& version) {
    test_commands_->ExpectAppCommandPing(test_server, appid, appcommandid,
                                         errorcode, eventresult, event_type,
                                         version);
  }

  void ExpectUpdateSequence(ScopedServer* test_server,
                            const std::string& app_id,
                            const std::string& install_data_index,
                            UpdateService::Priority priority,
                            const base::Version& from_version,
                            const base::Version& to_version,
                            bool do_fault_injection = false) {
    test_commands_->ExpectUpdateSequence(
        test_server, app_id, install_data_index, priority, from_version,
        to_version, do_fault_injection);
  }

  void ExpectUpdateSequenceBadHash(ScopedServer* test_server,
                                   const std::string& app_id,
                                   const std::string& install_data_index,
                                   UpdateService::Priority priority,
                                   const base::Version& from_version,
                                   const base::Version& to_version) {
    test_commands_->ExpectUpdateSequenceBadHash(test_server, app_id,
                                                install_data_index, priority,
                                                from_version, to_version);
  }

  void ExpectSelfUpdateSequence(ScopedServer* test_server) {
    test_commands_->ExpectSelfUpdateSequence(test_server);
  }

  void ExpectInstallSequence(ScopedServer* test_server,
                             const std::string& app_id,
                             const std::string& install_data_index,
                             UpdateService::Priority priority,
                             const base::Version& from_version,
                             const base::Version& to_version,
                             bool do_fault_injection = false) {
    test_commands_->ExpectInstallSequence(
        test_server, app_id, install_data_index, priority, from_version,
        to_version, do_fault_injection);
  }

  void StressUpdateService() { test_commands_->StressUpdateService(); }

  void CallServiceUpdate(
      const std::string& app_id,
      const std::string& install_data_index,
      UpdateService::PolicySameVersionUpdate policy_same_version_update) {
    test_commands_->CallServiceUpdate(app_id, install_data_index,
                                      policy_same_version_update);
  }

  void SetupFakeLegacyUpdater() { test_commands_->SetupFakeLegacyUpdater(); }

#if BUILDFLAG(IS_WIN)
  void RunFakeLegacyUpdater() { test_commands_->RunFakeLegacyUpdater(); }
#endif  // BUILDFLAG(IS_WIN)

#if BUILDFLAG(IS_MAC)
  void PrivilegedHelperInstall() { test_commands_->PrivilegedHelperInstall(); }
  void DeleteLegacyUpdater() { test_commands_->DeleteLegacyUpdater(); }
  void ExpectPrepareToRunBundleSuccess(const base::FilePath& bundle_path) {
    test_commands_->ExpectPrepareToRunBundleSuccess(bundle_path);
  }

  void ExpectKSAdminFetchTag(bool elevate,
                             const std::string& product_id,
                             const base::FilePath& xc_path,
                             std::optional<UpdaterScope> store_flag,
                             std::optional<std::string> want_tag) {
    test_commands_->ExpectKSAdminFetchTag(elevate, product_id, xc_path,
                                          store_flag, want_tag);
  }

#endif  // BUILDFLAG(IS_MAC)

  void ExpectAppInstalled(const std::string& appid,
                          const base::Version& expected_version) {
    ASSERT_NO_FATAL_FAILURE(ExpectAppVersion(appid, expected_version));

    // Verify installed app artifacts.
#if BUILDFLAG(IS_WIN)
    std::wstring pv;
    EXPECT_EQ(
        ERROR_SUCCESS,
        base::win::RegKey(UpdaterScopeToHKeyRoot(GetUpdaterScopeForTesting()),
                          GetAppClientsKey(appid).c_str(), Wow6432(KEY_READ))
            .ReadValue(kRegValuePV, &pv));
    EXPECT_EQ(pv, base::ASCIIToWide(expected_version.GetString()));
#else
    const base::FilePath app_json_path =
        GetInstallDirectory(GetUpdaterScopeForTesting())
            ->DirName()
            .AppendASCII(appid)
            .AppendASCII("app.json");
    JSONFileValueDeserializer parser(app_json_path,
                                     base::JSON_ALLOW_TRAILING_COMMAS);
    int error_code = 0;
    std::string error_message;
    std::unique_ptr<base::Value> app_data(
        parser.Deserialize(&error_code, &error_message));
    EXPECT_EQ(error_code, 0)
        << "Failed to load app json file at: " << app_json_path;
    EXPECT_TRUE(app_data);
    EXPECT_TRUE(app_data->is_dict());
    const base::Value::Dict& app_info = app_data->GetDict();
    EXPECT_EQ(*app_info.FindString("app"), appid);
    EXPECT_EQ(*app_info.FindString("company"), COMPANY_SHORTNAME_STRING);
    EXPECT_EQ(*app_info.FindString("pv"), expected_version.GetString());
#endif  // BUILDFLAG(IS_WIN)
  }

  void InstallTestApp(const TestApp& app, bool install_v1 = true) {
    const base::Version version = install_v1 ? app.v1 : app.v2;
    InstallApp(app.appid, version);
    base::FilePath exe_path;
    ASSERT_TRUE(base::PathService::Get(base::DIR_EXE, &exe_path));
    const base::CommandLine command = app.GetInstallCommandLine(install_v1);
    VLOG(2) << "Launch app setup command: " << command.GetCommandLineString();
    const base::Process process = base::LaunchProcess(
        IsSystemInstall(GetUpdaterScopeForTesting()) ? MakeElevated(command)
                                                     : command,
        {});
    if (!process.IsValid()) {
      VLOG(2) << "Failed to launch the app setup command.";
    }
    int exit_code = -1;
    EXPECT_TRUE(process.WaitForExitWithTimeout(TestTimeouts::action_timeout(),
                                               &exit_code));
    EXPECT_EQ(0, exit_code);
#if !BUILDFLAG(IS_WIN)
    SetExistenceCheckerPath(app.appid,
                            GetInstallDirectory(GetUpdaterScopeForTesting())
                                ->DirName()
                                .AppendASCII(app.appid));
#endif

    ExpectAppInstalled(app.appid, version);
  }

  void ExpectLegacyUpdaterMigrated() {
    test_commands_->ExpectLegacyUpdaterMigrated();
  }

  void RunRecoveryComponent(const std::string& app_id,
                            const base::Version& version) {
    test_commands_->RunRecoveryComponent(app_id, version);
  }

  void SetLastChecked(const base::Time& time) {
    test_commands_->SetLastChecked(time);
  }

  void ExpectLastChecked() { test_commands_->ExpectLastChecked(); }

  void ExpectLastStarted() { test_commands_->ExpectLastStarted(); }

  void RunOfflineInstall(bool is_legacy_install, bool is_silent_install) {
    test_commands_->RunOfflineInstall(is_legacy_install, is_silent_install);
  }

  void RunOfflineInstallOsNotSupported(bool is_legacy_install,
                                       bool is_silent_install) {
    test_commands_->RunOfflineInstallOsNotSupported(is_legacy_install,
                                                    is_silent_install);
  }

  void DMPushEnrollmentToken(const std::string& enrollment_token) {
    test_commands_->DMPushEnrollmentToken(enrollment_token);
  }

  void DMDeregisterDevice() { test_commands_->DMDeregisterDevice(); }

  void DMCleanup() { test_commands_->DMCleanup(); }

  void InstallEnterpriseCompanionApp() {
    test_commands_->InstallEnterpriseCompanionApp();
  }

  void InstallBrokenEnterpriseCompanionApp() {
    test_commands_->InstallBrokenEnterpriseCompanionApp();
  }

  void UninstallBrokenEnterpriseCompanionApp() {
    test_commands_->UninstallBrokenEnterpriseCompanionApp();
  }

  void InstallEnterpriseCompanionAppOverrides(
      const base::Value::Dict& external_overrides) {
    test_commands_->InstallEnterpriseCompanionAppOverrides(external_overrides);
  }

  void ExpectEnterpriseCompanionAppNotInstalled() {
    test_commands_->ExpectEnterpriseCompanionAppNotInstalled();
  }

  void UninstallEnterpriseCompanionApp() {
    test_commands_->UninstallEnterpriseCompanionApp();
  }

  scoped_refptr<IntegrationTestCommands> test_commands_;

#if BUILDFLAG(IS_WIN)
  static constexpr char kGlobalPolicyKey[] = "";
  const TestApp kApp1 = {"test1", base::Version("1.0.0.0"),
                         "Testapp2Setup.crx3", base::Version("2.0.0.0"),
                         "Testapp2Setup.crx3"};
  const TestApp kApp2 = {"test2", base::Version("100.0.0.0"),
                         "Testapp2Setup.crx3", base::Version("101.0.0.0"),
                         "Testapp2Setup.crx3"};
  const TestApp kApp3 = {"test3", base::Version("1.0"), "Testapp2Setup.crx3",
                         base::Version("1.1"), "Testapp2Setup.crx3"};
#else
  static constexpr char kGlobalPolicyKey[] = "global";
  const TestApp kApp1 = {
      "test1", base::Version("1.0.0.0"), "test_installer_test1_v1.crx3",
      base::Version("2.0.0.0"), "test_installer_test1_v2.crx3"};
  const TestApp kApp2 = {
      "test2", base::Version("100.0.0.0"), "test_installer_test2_v1.crx3",
      base::Version("101.0.0.0"), "test_installer_test2_v2.crx3"};
  const TestApp kApp3 = {"test3", base::Version("1.0"),
                         "test_installer_test3_v1.crx3", base::Version("1.1"),
                         "test_installer_test3_v2.crx3"};
#endif  // BUILDFLAG(IS_WIN)

 private:
  base::test::TaskEnvironment environment_;
  ScopedIPCSupportWrapper ipc_support_;
};

#if defined(ADDRESS_SANITIZER)
#define MAYBE_UpdateServiceStress DISABLED_UpdateServiceStress
#else
#define MAYBE_UpdateServiceStress UpdateServiceStress
#endif

// Tests the setup and teardown of the fixture.
TEST_F(IntegrationTest, DoNothing) {}

TEST_F(IntegrationTest, Install) {
  ASSERT_NO_FATAL_FAILURE(Install());
  ASSERT_TRUE(WaitForUpdaterExit());
  ASSERT_NO_FATAL_FAILURE(ExpectInstalled());
  ASSERT_NO_FATAL_FAILURE(ExpectVersionActive(kUpdaterVersion));
#if BUILDFLAG(IS_WIN)
  // Tests the COM registration after the install. For now, tests that the
  // COM interfaces are registered, which is indirectly testing the type
  // library separation for the public, private, and legacy interfaces.
  ASSERT_NO_FATAL_FAILURE(ExpectInterfacesRegistered());
#endif  // BUILDFLAG(IS_WIN)
  ASSERT_NO_FATAL_FAILURE(Uninstall());
}

// Tests running the installer when the updater is already installed at the
// same version. It should have no notable effect.
TEST_F(IntegrationTest, OverinstallRedundant) {
  ASSERT_NO_FATAL_FAILURE(Install());
  ASSERT_NO_FATAL_FAILURE(ExpectInstalled());
  ASSERT_NO_FATAL_FAILURE(InstallApp("test"));

  ASSERT_TRUE(WaitForUpdaterExit());
  ASSERT_NO_FATAL_FAILURE(ExpectVersionActive(kUpdaterVersion));
  ASSERT_NO_FATAL_FAILURE(ExpectRegistered("test"));

  ASSERT_NO_FATAL_FAILURE(Install());
  ASSERT_TRUE(WaitForUpdaterExit());
  ASSERT_NO_FATAL_FAILURE(ExpectInstalled());
  ASSERT_NO_FATAL_FAILURE(ExpectVersionActive(kUpdaterVersion));
  ASSERT_NO_FATAL_FAILURE(ExpectRegistered("test"));

  ASSERT_NO_FATAL_FAILURE(Uninstall());
}

TEST_F(IntegrationTest, OverinstallWorking) {
  ASSERT_NO_FATAL_FAILURE(SetupRealUpdaterLowerVersion());
  ASSERT_NO_FATAL_FAILURE(InstallApp("test"));
  ASSERT_TRUE(WaitForUpdaterExit());
  ASSERT_NO_FATAL_FAILURE(ExpectVersionNotActive(kUpdaterVersion));
  ASSERT_NO_FATAL_FAILURE(ExpectRegistered("test"));

  // A new version hands off installation to the old version, and doesn't
  // change the active version of the updater.
  ASSERT_NO_FATAL_FAILURE(Install());
  ASSERT_TRUE(WaitForUpdaterExit());
  ASSERT_NO_FATAL_FAILURE(ExpectVersionNotActive(kUpdaterVersion));
  ASSERT_NO_FATAL_FAILURE(ExpectRegistered("test"));

  // After two wakes, the new updater is active.
  ScopedServer test_server(test_commands_);
  ASSERT_NO_FATAL_FAILURE(
      ExpectUpdateSequence(&test_server, kQualificationAppId, "",
                           UpdateService::Priority::kBackground,
                           base::Version("0.1"), base::Version("0.2")));
  ASSERT_NO_FATAL_FAILURE(RunWake(0));
  ASSERT_TRUE(WaitForUpdaterExit());
  ASSERT_NO_FATAL_FAILURE(ExpectNoUpdateSequence(&test_server, kUpdaterAppId));
  ASSERT_NO_FATAL_FAILURE(RunWake(0));
  ASSERT_TRUE(WaitForUpdaterExit());
  ASSERT_NO_FATAL_FAILURE(ExpectVersionActive(kUpdaterVersion));
  ASSERT_NO_FATAL_FAILURE(ExpectRegistered("test"));

  ASSERT_NO_FATAL_FAILURE(ExpectUninstallPing(&test_server));
  ASSERT_NO_FATAL_FAILURE(Uninstall());
}

TEST_F(IntegrationTest, OverinstallBroken) {
  ASSERT_NO_FATAL_FAILURE(SetupRealUpdaterLowerVersion());
  ASSERT_NO_FATAL_FAILURE(InstallApp("test"));
  ASSERT_TRUE(WaitForUpdaterExit());
  ASSERT_NO_FATAL_FAILURE(DeleteActiveUpdaterExecutable());

  // Since the old version is not working, the new version should install and
  // become active.
  ASSERT_NO_FATAL_FAILURE(Install());
  ASSERT_TRUE(WaitForUpdaterExit());
  ASSERT_NO_FATAL_FAILURE(ExpectVersionActive(kUpdaterVersion));
  ASSERT_NO_FATAL_FAILURE(ExpectRegistered("test"));

  ASSERT_NO_FATAL_FAILURE(Uninstall());

  // Cleanup the older version by reinstalling and uninstalling.
  ASSERT_NO_FATAL_FAILURE(SetupRealUpdaterLowerVersion());
  ASSERT_TRUE(WaitForUpdaterExit());
  ASSERT_NO_FATAL_FAILURE(Install());
  ASSERT_TRUE(WaitForUpdaterExit());
  ASSERT_NO_FATAL_FAILURE(Uninstall());
}

TEST_F(IntegrationTest, OverinstallBrokenSameVersion) {
  ASSERT_NO_FATAL_FAILURE(Install());
  ASSERT_NO_FATAL_FAILURE(InstallApp("test"));
  ASSERT_TRUE(WaitForUpdaterExit());
  ASSERT_NO_FATAL_FAILURE(ExpectInstalled());
  ASSERT_NO_FATAL_FAILURE(DeleteActiveUpdaterExecutable());

  // Since the existing version is now not working, it should reinstall. This
  // will ultimately result in no visible change to the prefs file since the
  // new active version number will be the same as the old one.
  ASSERT_NO_FATAL_FAILURE(Install());
  ASSERT_TRUE(WaitForUpdaterExit());
  ASSERT_NO_FATAL_FAILURE(ExpectInstalled());
  ASSERT_NO_FATAL_FAILURE(ExpectVersionActive(kUpdaterVersion));
  ASSERT_NO_FATAL_FAILURE(ExpectRegistered("test"));

  ASSERT_NO_FATAL_FAILURE(Uninstall());
}

TEST_F(IntegrationTest, SelfUninstallOutdatedUpdater) {
  ASSERT_NO_FATAL_FAILURE(Install());
  ASSERT_NO_FATAL_FAILURE(ExpectInstalled());
  ASSERT_TRUE(WaitForUpdaterExit());
  ASSERT_NO_FATAL_FAILURE(InstallApp("test"));
  ASSERT_NO_FATAL_FAILURE(SetupFakeUpdaterHigherVersion());
  ASSERT_NO_FATAL_FAILURE(ExpectVersionNotActive(kUpdaterVersion));
  ASSERT_NO_FATAL_FAILURE(ExpectRegistered("test"));

  ASSERT_NO_FATAL_FAILURE(RunWake(0));
  ASSERT_TRUE(WaitForUpdaterExit());

  ASSERT_NO_FATAL_FAILURE(ExpectCandidateUninstalled());
  // The candidate uninstall should not have altered global prefs.
  ASSERT_NO_FATAL_FAILURE(ExpectVersionNotActive(kUpdaterVersion));
  ASSERT_NO_FATAL_FAILURE(ExpectVersionNotActive("0.0.0.0"));
  ASSERT_NO_FATAL_FAILURE(ExpectRegistered("test"));

  // Do not call `Uninstall()` since the outdated updater uninstalled itself.
  // Additional clean up is needed because of how this test is set up. After
  // the outdated instance uninstalls, a few files are left in the product
  // directory: prefs.json, updater.log, and overrides.json. These files are
  // owned by the active instance of the updater but in this case there is
  // no active instance left; therefore, explicit clean up is required.
  PrintLog();
  CopyLog();
  Clean();
}

TEST_F(IntegrationTest, QualifyUpdater) {
  ScopedServer test_server(test_commands_);
  ASSERT_NO_FATAL_FAILURE(Install());
  ASSERT_NO_FATAL_FAILURE(ExpectInstalled());
  ASSERT_TRUE(WaitForUpdaterExit());
  ASSERT_NO_FATAL_FAILURE(SetupFakeUpdaterLowerVersion());
  ASSERT_NO_FATAL_FAILURE(ExpectVersionNotActive(kUpdaterVersion));

  ASSERT_NO_FATAL_FAILURE(
      ExpectUpdateSequence(&test_server, kQualificationAppId, "",
                           UpdateService::Priority::kBackground,
                           base::Version("0.1"), base::Version("0.2")));

  ASSERT_NO_FATAL_FAILURE(RunWake(0));
  ASSERT_TRUE(WaitForUpdaterExit());

  // This instance is now qualified and should activate itself and check itself
  // for updates on the next check.
  test_server.ExpectOnce({request::GetUpdaterUserAgentMatcher(),
                          request::GetContentMatcher(
                              {base::StringPrintf(".*%s.*", kUpdaterAppId)})},
                         ")]}'\n");
  ASSERT_NO_FATAL_FAILURE(RunWake(0));
  ASSERT_TRUE(WaitForUpdaterExit());
  ASSERT_NO_FATAL_FAILURE(ExpectVersionActive(kUpdaterVersion));

  ASSERT_NO_FATAL_FAILURE(ExpectUninstallPing(&test_server));
  ASSERT_NO_FATAL_FAILURE(Uninstall());
}

TEST_F(IntegrationTest, CleanupOldVersion) {
  ASSERT_NO_FATAL_FAILURE(SetupFakeUpdaterLowerVersion());

  // Since the old version is not working, the new version should install and
  // become active.
  ASSERT_NO_FATAL_FAILURE(Install());
  ASSERT_TRUE(WaitForUpdaterExit());
  ASSERT_NO_FATAL_FAILURE(ExpectVersionActive(kUpdaterVersion));

  // Waking the new version should clean up the old.
  ASSERT_NO_FATAL_FAILURE(RunWake(0));
  ASSERT_TRUE(WaitForUpdaterExit());
  std::optional<base::FilePath> path =
      GetInstallDirectory(GetUpdaterScopeForTesting());
  ASSERT_TRUE(path);
  int dirs = 0;
  base::FileEnumerator(*path, false, base::FileEnumerator::DIRECTORIES)
      .ForEach([&dirs](const base::FilePath& path) {
        if (base::Version(path.BaseName().MaybeAsASCII()).IsValid()) {
          ++dirs;
        }
      });
  EXPECT_EQ(dirs, 1);

  ASSERT_NO_FATAL_FAILURE(Uninstall());
}

TEST_F(IntegrationTest, SelfUpdate) {
  ScopedServer test_server(test_commands_);
  ASSERT_NO_FATAL_FAILURE(Install());
  ASSERT_NO_FATAL_FAILURE(InstallApp("test"));

  base::Version next_version(base::StringPrintf("%s1", kUpdaterVersion));
  ASSERT_NO_FATAL_FAILURE(ExpectUpdateSequence(
      &test_server, kUpdaterAppId, "", UpdateService::Priority::kBackground,
      base::Version(kUpdaterVersion), next_version));

  ASSERT_NO_FATAL_FAILURE(RunWake(0));
  ASSERT_TRUE(WaitForUpdaterExit());
  ASSERT_NO_FATAL_FAILURE(ExpectAppVersion(kUpdaterAppId, next_version));
  ASSERT_NO_FATAL_FAILURE(ExpectRegistered("test"));

  ASSERT_NO_FATAL_FAILURE(ExpectUninstallPing(&test_server));
  ASSERT_NO_FATAL_FAILURE(Uninstall());
}

TEST_F(IntegrationTest, SelfUpdateWithWakeAll) {
  ScopedServer test_server(test_commands_);
  ASSERT_NO_FATAL_FAILURE(Install());
  ASSERT_NO_FATAL_FAILURE(InstallApp("test"));

  base::Version next_version(base::StringPrintf("%s1", kUpdaterVersion));
  ASSERT_NO_FATAL_FAILURE(ExpectUpdateSequence(
      &test_server, kUpdaterAppId, "", UpdateService::Priority::kBackground,
      base::Version(kUpdaterVersion), next_version));

  ASSERT_NO_FATAL_FAILURE(RunWakeAll());
  ASSERT_TRUE(WaitForUpdaterExit());
  ASSERT_NO_FATAL_FAILURE(ExpectAppVersion(kUpdaterAppId, next_version));
  ASSERT_NO_FATAL_FAILURE(ExpectRegistered("test"));

  ASSERT_NO_FATAL_FAILURE(ExpectUninstallPing(&test_server));
  ASSERT_NO_FATAL_FAILURE(Uninstall());
}

TEST_F(IntegrationTest, NoSelfUpdateIfNoEula) {
  ScopedServer test_server(test_commands_);
  ASSERT_NO_FATAL_FAILURE(
      Install(base::Value::List().Append(kEulaRequiredSwitch)));
  ASSERT_NO_FATAL_FAILURE(RunWake(0));
  ASSERT_TRUE(WaitForUpdaterExit());
  ASSERT_NO_FATAL_FAILURE(
      ExpectAppVersion(kUpdaterAppId, base::Version(kUpdaterVersion)));
  ASSERT_NO_FATAL_FAILURE(Uninstall());
}

#if BUILDFLAG(IS_WIN)
TEST_F(IntegrationTest, UninstallWithoutPingIfNoEula) {
  ScopedServer test_server(test_commands_);
  ASSERT_NO_FATAL_FAILURE(
      Install(base::Value::List().Append(kEulaRequiredSwitch)));
  ASSERT_NO_FATAL_FAILURE(RunOfflineInstall(/*is_legacy_install=*/false,
                                            /*is_silent_install=*/false));
  ASSERT_TRUE(WaitForUpdaterExit());
  ASSERT_NO_FATAL_FAILURE(SetServerStarts(24));
  ASSERT_NO_FATAL_FAILURE(
      UninstallApp("{CDABE316-39CD-43BA-8440-6D1E0547AEE6}"));
  ASSERT_NO_FATAL_FAILURE(RunWake(0));
  ASSERT_TRUE(WaitForUpdaterExit());
  ASSERT_NO_FATAL_FAILURE(ExpectClean());
}

TEST_F(IntegrationTest, SelfUpdateAfterEulaAcceptedViaRegistry) {
  if (!IsSystemInstall(GetUpdaterScopeForTesting())) {
    GTEST_SKIP() << "HKLM/CSM only exists in system scope.";
  }
  ScopedServer test_server(test_commands_);
  ASSERT_NO_FATAL_FAILURE(
      Install(base::Value::List().Append(kEulaRequiredSwitch)));

  // Set EULA accepted on the updater app itself.
  ASSERT_EQ(
      base::win::RegKey(UpdaterScopeToHKeyRoot(GetUpdaterScopeForTesting()),
                        base::StrCat({CLIENT_STATE_MEDIUM_KEY,
                                      base::UTF8ToWide(kUpdaterAppId)})
                            .c_str(),
                        Wow6432(KEY_WRITE))
          .WriteValue(L"eulaaccepted", 1),
      ERROR_SUCCESS);

  base::Version next_version(base::StringPrintf("%s1", kUpdaterVersion));
  ASSERT_NO_FATAL_FAILURE(ExpectUpdateSequence(
      &test_server, kUpdaterAppId, "", UpdateService::Priority::kBackground,
      base::Version(kUpdaterVersion), next_version));
  ASSERT_NO_FATAL_FAILURE(RunWake(0));
  ASSERT_TRUE(WaitForUpdaterExit());
  ASSERT_NO_FATAL_FAILURE(ExpectAppVersion(kUpdaterAppId, next_version));
  ASSERT_NO_FATAL_FAILURE(ExpectUninstallPing(&test_server));
  ASSERT_NO_FATAL_FAILURE(Uninstall());
}
#endif  // BUILDFLAG(IS_WIN)

#if !BUILDFLAG(IS_LINUX)
// InstallAppViaService does not work on Linux.
TEST_F(IntegrationTest, SelfUpdateAfterEulaAcceptedViaInstall) {
  ScopedServer test_server(test_commands_);
  ASSERT_NO_FATAL_FAILURE(
      Install(base::Value::List().Append(kEulaRequiredSwitch)));

  // Installing an app implies EULA accepted.
  ASSERT_NO_FATAL_FAILURE(ExpectAppsUpdateSequence(
      GetUpdaterScopeForTesting(), &test_server,
      /*request_attributes=*/{},
      {
          AppUpdateExpectation(
              kApp1.GetInstallCommandLineArgs(/*install_v1=*/true), kApp1.appid,
              base::Version({0, 0, 0, 0}), kApp1.v1,
              /*is_install=*/true,
              /*should_update=*/true, false, "", "",
              GetInstallerPath(kApp1.v1_crx)),
      }));

  ASSERT_NO_FATAL_FAILURE(InstallAppViaService(kApp1.appid));

  base::Version next_version(base::StringPrintf("%s1", kUpdaterVersion));
  ASSERT_NO_FATAL_FAILURE(ExpectUpdateSequence(
      &test_server, kUpdaterAppId, "", UpdateService::Priority::kBackground,
      base::Version(kUpdaterVersion), next_version));
  ASSERT_NO_FATAL_FAILURE(RunWake(0));
  ASSERT_TRUE(WaitForUpdaterExit());
  ASSERT_NO_FATAL_FAILURE(ExpectAppVersion(kUpdaterAppId, next_version));
  ASSERT_NO_FATAL_FAILURE(ExpectRegistered(kApp1.appid));
  ASSERT_NO_FATAL_FAILURE(ExpectUninstallPing(&test_server));
  ASSERT_NO_FATAL_FAILURE(Uninstall());
}
#endif  // !BUILDFLAG(IS_LINUX)

TEST_F(IntegrationTest, ReportsActive) {
  // A longer than usual timeout is needed for this test because the macOS
  // UpdateServiceInternal server takes at least 10 seconds to shut down after
  // Install, and InstallApp cannot make progress until it shut downs and
  // releases the global prefs lock.
  ASSERT_GE(TestTimeouts::action_timeout(), base::Seconds(18));
  base::test::ScopedRunLoopTimeout timeout(FROM_HERE,
                                           TestTimeouts::action_timeout());

  ScopedServer test_server(test_commands_);
  ASSERT_NO_FATAL_FAILURE(Install());
  ASSERT_NO_FATAL_FAILURE(ExpectInstalled());

  // Register apps test1 and test2. Expect pings for each.
  ASSERT_NO_FATAL_FAILURE(InstallApp("test1"));
  ASSERT_NO_FATAL_FAILURE(InstallApp("test2"));

  // Set test1 to be active and do a background updatecheck.
  ASSERT_NO_FATAL_FAILURE(SetActive("test1"));
  ASSERT_NO_FATAL_FAILURE(ExpectActive("test1"));
  ASSERT_NO_FATAL_FAILURE(ExpectNotActive("test2"));
  test_server.ExpectOnce(
      {request::GetUpdaterUserAgentMatcher(),
       request::GetContentMatcher(
           {R"(.*"appid":"test1","enabled":true,"installdate":-1,)",
            R"("ping":{"ad":-1,.*)"})},
      R"()]}')"
      "\n"
      R"({"response":{"protocol":"3.1","daystart":{"elapsed_)"
      R"(days":5098}},"app":[{"appid":"test1","status":"ok",)"
      R"("updatecheck":{"status":"noupdate"}},{"appid":"test2",)"
      R"("status":"ok","updatecheck":{"status":"noupdate"}}]})");
  ASSERT_NO_FATAL_FAILURE(RunWake(0));

  // The updater has cleared the active bits.
  ASSERT_NO_FATAL_FAILURE(ExpectNotActive("test1"));
  ASSERT_NO_FATAL_FAILURE(ExpectNotActive("test2"));

  ASSERT_NO_FATAL_FAILURE(ExpectUninstallPing(&test_server));
  ASSERT_NO_FATAL_FAILURE(Uninstall());
}

// Tests calling `CheckForUpdate` when the updater is not installed.
TEST_F(IntegrationTest, CheckForUpdate_UpdaterNotInstalled) {
  scoped_refptr<UpdateService> update_service =
      CreateUpdateServiceProxy(GetUpdaterScopeForTesting());
  base::RunLoop loop;
  update_service->CheckForUpdate(
      "test", UpdateService::Priority::kForeground,
      UpdateService::PolicySameVersionUpdate::kNotAllowed, base::DoNothing(),
      base::BindLambdaForTesting([&loop](UpdateService::Result result) {
        EXPECT_TRUE(result == UpdateService::Result::kServiceFailed ||
                    result == UpdateService::Result::kIPCConnectionFailed)
            << "result == " << result;
        loop.Quit();
      }));
  loop.Run();
}

TEST_F(IntegrationTest, CheckForUpdate) {
  ScopedServer test_server(test_commands_);
  ASSERT_NO_FATAL_FAILURE(Install());

  const std::string kAppId("test");
  ASSERT_NO_FATAL_FAILURE(InstallApp(kAppId));
  ASSERT_NO_FATAL_FAILURE(ExpectUpdateCheckSequence(
      &test_server, kAppId, UpdateService::Priority::kForeground,
      base::Version("0.1"), base::Version("1")));
  ASSERT_NO_FATAL_FAILURE(CheckForUpdate(kAppId));

  ASSERT_NO_FATAL_FAILURE(ExpectUninstallPing(&test_server));
  ASSERT_NO_FATAL_FAILURE(Uninstall());
}

TEST_F(IntegrationTest, UpdateBadHash) {
  ScopedServer test_server(test_commands_);
  ASSERT_NO_FATAL_FAILURE(Install());

  const std::string kAppId("test");
  ASSERT_NO_FATAL_FAILURE(InstallApp(kAppId));
  ASSERT_NO_FATAL_FAILURE(ExpectUpdateSequenceBadHash(
      &test_server, kAppId, "", UpdateService::Priority::kBackground,
      base::Version("0.1"), base::Version("1")));
  ASSERT_NO_FATAL_FAILURE(RunWake(0));

  ASSERT_NO_FATAL_FAILURE(ExpectUninstallPing(&test_server));
  ASSERT_NO_FATAL_FAILURE(Uninstall());
}

TEST_F(IntegrationTest, UpdateErrorStatus) {
  ScopedServer test_server(test_commands_);

  ASSERT_NO_FATAL_FAILURE(Install());
  ASSERT_NO_FATAL_FAILURE(ExpectInstalled());
  ASSERT_NO_FATAL_FAILURE(InstallTestApp(kApp1, /*install_v1=*/true));

  for (const char* app_response_status :
       {"noupdate", "error-internal", "error-hash", "error-osnotsupported",
        "error-hwnotsupported", "error-unsupportedprotocol"}) {
    ExpectAppsUpdateSequence(
        GetUpdaterScopeForTesting(), &test_server, {},
        {
            AppUpdateExpectation(
                kApp1.GetInstallCommandLineArgs(/*install_v1=*/false),
                kApp1.appid, kApp1.v1, kApp1.v2,
                /*is_install=*/false,
                /*should_update=*/false, false, "", "",
                GetInstallerPath(kApp1.v2_crx),
                /*always_serve_crx=*/false,
                /*error_category=*/UpdateService::ErrorCategory::kNone,
                /*error_code=*/0,
                /*event_type=*/0,
                /*custom_app_response=*/{}, app_response_status),
        });
    ASSERT_NO_FATAL_FAILURE(RunWake(0));
    ASSERT_TRUE(WaitForUpdaterExit());
    ASSERT_NO_FATAL_FAILURE(ExpectAppInstalled(kApp1.appid, kApp1.v1))
        << "App is unexpectedly updated with update check status: "
        << app_response_status;
    ASSERT_NO_FATAL_FAILURE(SetLastChecked(base::Time::Now() - base::Hours(9)))
        << "Failed to set last-checked to force next update check.";
  }

  ASSERT_NO_FATAL_FAILURE(ExpectUninstallPing(&test_server));
  ASSERT_NO_FATAL_FAILURE(UninstallApp(kApp1.appid));
  ASSERT_NO_FATAL_FAILURE(Uninstall());
}

TEST_F(IntegrationTest, UpdateApp) {
  ScopedServer test_server(test_commands_);
  ASSERT_NO_FATAL_FAILURE(Install());

  const std::string kAppId("test");
  ASSERT_NO_FATAL_FAILURE(InstallApp(kAppId));
  base::Version v1("1");
  ASSERT_NO_FATAL_FAILURE(ExpectUpdateSequence(
      &test_server, kAppId, "", UpdateService::Priority::kBackground,
      base::Version("0.1"), v1));
  ASSERT_NO_FATAL_FAILURE(RunWake(0));

  base::Version v2("2");
  const std::string kInstallDataIndex("test_install_data_index");
  ASSERT_NO_FATAL_FAILURE(
      ExpectUpdateSequence(&test_server, kAppId, kInstallDataIndex,
                           UpdateService::Priority::kForeground, v1, v2));
  ASSERT_NO_FATAL_FAILURE(Update(kAppId, kInstallDataIndex));

  ASSERT_TRUE(WaitForUpdaterExit());
  ASSERT_NO_FATAL_FAILURE(ExpectAppVersion(kAppId, v2));
  ASSERT_NO_FATAL_FAILURE(ExpectLastChecked());
  ASSERT_NO_FATAL_FAILURE(ExpectLastStarted());

  ASSERT_NO_FATAL_FAILURE(ExpectUninstallPing(&test_server));
  ASSERT_NO_FATAL_FAILURE(Uninstall());
}

#if BUILDFLAG(IS_WIN)
TEST_F(IntegrationTest, GZipUpdateResponses) {
  ScopedServer test_server(test_commands_);
  test_server.set_gzip_response(true);
  ASSERT_NO_FATAL_FAILURE(Install());

  const std::string kAppId("test");
  ASSERT_NO_FATAL_FAILURE(InstallApp(kAppId));
  base::Version v1("1");
  ASSERT_NO_FATAL_FAILURE(ExpectUpdateSequence(
      &test_server, kAppId, "", UpdateService::Priority::kBackground,
      base::Version("0.1"), v1));
  ASSERT_NO_FATAL_FAILURE(RunWake(0));

  base::Version v2("2");
  const std::string kInstallDataIndex("test_install_data_index");
  ASSERT_NO_FATAL_FAILURE(
      ExpectUpdateSequence(&test_server, kAppId, kInstallDataIndex,
                           UpdateService::Priority::kForeground, v1, v2));
  ASSERT_NO_FATAL_FAILURE(Update(kAppId, kInstallDataIndex));

  ASSERT_TRUE(WaitForUpdaterExit());
  ASSERT_NO_FATAL_FAILURE(ExpectAppVersion(kAppId, v2));
  ASSERT_NO_FATAL_FAILURE(ExpectLastChecked());
  ASSERT_NO_FATAL_FAILURE(ExpectLastStarted());

  ASSERT_NO_FATAL_FAILURE(ExpectUninstallPing(&test_server));
  ASSERT_NO_FATAL_FAILURE(Uninstall());
}

TEST_F(IntegrationTest, UpdateAppSucceedsEvenAfterDeletingInterfaces) {
  ScopedServer test_server(test_commands_);
  ASSERT_NO_FATAL_FAILURE(Install());
  ASSERT_TRUE(WaitForUpdaterExit());

  const UpdaterScope scope = GetUpdaterScopeForTesting();
  ASSERT_TRUE(AreComInterfacesPresent(scope, true));
  ASSERT_TRUE(AreComInterfacesPresent(scope, false));
  // Delete IUpdaterXXX, used by `InstallApp` via `RegisterApp`.
  // Delete IUpdaterInternal, used by the `wake` task.
  {
    for (const IID& iid : [&scope]() -> std::vector<IID> {
           switch (scope) {
             case UpdaterScope::kUser:
               return {
                   __uuidof(IUpdaterUser),
                   __uuidof(IUpdaterCallbackUser),
                   __uuidof(IUpdaterInternalUser),
                   __uuidof(IUpdaterInternalCallbackUser),
               };
             case UpdaterScope::kSystem:
               return {
                   __uuidof(IUpdaterSystem),
                   __uuidof(IUpdaterCallbackSystem),
                   __uuidof(IUpdaterInternalSystem),
                   __uuidof(IUpdaterInternalCallbackSystem),
               };
           }
         }()) {
      LONG result =
          base::win::RegKey(UpdaterScopeToHKeyRoot(scope), L"", DELETE)
              .DeleteKey(GetComIidRegistryPath(iid).c_str());
      ASSERT_TRUE(result == ERROR_SUCCESS || result == ERROR_FILE_NOT_FOUND);
    }
  }
  ASSERT_FALSE(AreComInterfacesPresent(scope, true));
  ASSERT_FALSE(AreComInterfacesPresent(scope, false));

  const std::string kAppId("test");
  ASSERT_NO_FATAL_FAILURE(InstallApp(kAppId));
  base::Version v1("1");
  ASSERT_NO_FATAL_FAILURE(ExpectUpdateSequence(
      &test_server, kAppId, "", UpdateService::Priority::kBackground,
      base::Version("0.1"), v1));

  ASSERT_NO_FATAL_FAILURE(RunWake(0));

  base::Version v2("2");
  const std::string kInstallDataIndex("test_install_data_index");
  ASSERT_NO_FATAL_FAILURE(
      ExpectUpdateSequence(&test_server, kAppId, kInstallDataIndex,
                           UpdateService::Priority::kForeground, v1, v2));
  ASSERT_NO_FATAL_FAILURE(Update(kAppId, kInstallDataIndex));

  ASSERT_TRUE(WaitForUpdaterExit());
  ASSERT_NO_FATAL_FAILURE(ExpectAppVersion(kAppId, v2));
  ASSERT_NO_FATAL_FAILURE(ExpectLastChecked());
  ASSERT_NO_FATAL_FAILURE(ExpectLastStarted());

  ASSERT_NO_FATAL_FAILURE(ExpectUninstallPing(&test_server));
  ASSERT_NO_FATAL_FAILURE(Uninstall());
}
#endif  // BUILDFLAG(IS_WIN)

TEST_F(IntegrationTest, NoCheckWhenLastCheckedRecently) {
  ScopedServer test_server(test_commands_);
  ASSERT_NO_FATAL_FAILURE(SetLastChecked(base::Time::Now() - base::Minutes(5)));
  ASSERT_NO_FATAL_FAILURE(Install());
  ASSERT_NO_FATAL_FAILURE(InstallApp("test"));
  ASSERT_NO_FATAL_FAILURE(RunWake(0));
  ASSERT_NO_FATAL_FAILURE(ExpectUninstallPing(&test_server));
  ASSERT_NO_FATAL_FAILURE(Uninstall());
}

TEST_F(IntegrationTest, NoCheckWhenLastCheckedRecentlyPolicy) {
  ScopedServer test_server(test_commands_);
  base::Value::Dict group_policies;
  group_policies.Set("autoupdatecheckperiodminutes", 60 * 18);
  ASSERT_NO_FATAL_FAILURE(SetLastChecked(base::Time::Now() - base::Hours(12)));
  ASSERT_NO_FATAL_FAILURE(Install());
  ASSERT_NO_FATAL_FAILURE(SetGroupPolicies(group_policies));
  ASSERT_NO_FATAL_FAILURE(InstallApp("test"));
  ASSERT_NO_FATAL_FAILURE(RunWake(0));
  ASSERT_NO_FATAL_FAILURE(ExpectUninstallPing(&test_server));
  ASSERT_NO_FATAL_FAILURE(Uninstall());
}

TEST_F(IntegrationTest, NoCheckWhenSuppressed) {
  ScopedServer test_server(test_commands_);
  base::Time::Exploded now;
  base::Time::Now().LocalExplode(&now);
  base::Value::Dict group_policies;
  group_policies.Set("updatessuppressedstarthour", (now.hour - 1 + 24) % 24);
  group_policies.Set("updatessuppressedstartmin", 0);
  group_policies.Set("updatessuppresseddurationmin", 120);
  ASSERT_NO_FATAL_FAILURE(Install());
  ASSERT_NO_FATAL_FAILURE(SetGroupPolicies(group_policies));
  ASSERT_NO_FATAL_FAILURE(InstallApp("test"));
  ASSERT_NO_FATAL_FAILURE(RunWake(0));
  ASSERT_NO_FATAL_FAILURE(ExpectUninstallPing(&test_server));
  ASSERT_NO_FATAL_FAILURE(Uninstall());
}

TEST_F(IntegrationTest, InstallUpdaterAndApp) {
  ScopedServer test_server(test_commands_);
  const std::string kAppId("test");
  const base::Version v1("1");
  ASSERT_NO_FATAL_FAILURE(ExpectInstallSequence(
      &test_server, kAppId, "", UpdateService::Priority::kForeground,
      base::Version({0, 0, 0, 0}), v1));

  ASSERT_NO_FATAL_FAILURE(
      InstallUpdaterAndApp(kAppId, /*is_silent_install=*/true, "usagestats=1"));
  ASSERT_TRUE(WaitForUpdaterExit());

  ASSERT_NO_FATAL_FAILURE(ExpectAppVersion(kAppId, v1));

  ASSERT_NO_FATAL_FAILURE(ExpectUninstallPing(&test_server));
  ASSERT_NO_FATAL_FAILURE(Uninstall());
}

TEST_F(IntegrationTest, InstallUpdaterAndTwoApps) {
  ScopedServer test_server(test_commands_);
  const std::string kAppId("test");
  const std::string kAppId2("test2");
  const base::Version v1("1");
  ASSERT_NO_FATAL_FAILURE(ExpectInstallSequence(
      &test_server, kAppId, "", UpdateService::Priority::kForeground,
      base::Version({0, 0, 0, 0}), v1));
  ASSERT_NO_FATAL_FAILURE(InstallUpdaterAndApp(
      kAppId, /*is_silent_install=*/true,
      base::StrCat({"appguid=", kAppId, "&ap=foo&usagestats=1"})));
  ASSERT_NO_FATAL_FAILURE(ExpectInstallSequence(
      &test_server, kAppId2, "", UpdateService::Priority::kForeground,
      base::Version({0, 0, 0, 0}), v1));
  ASSERT_NO_FATAL_FAILURE(InstallUpdaterAndApp(
      kAppId2, /*is_silent_install=*/true,
      base::StrCat({"appguid=", kAppId2, "&ap=foo2&usagestats=1"})));
  ASSERT_TRUE(WaitForUpdaterExit());

  ASSERT_NO_FATAL_FAILURE(ExpectAppVersion(kAppId, v1));
  ASSERT_NO_FATAL_FAILURE(ExpectAppVersion(kAppId2, v1));
  ASSERT_NO_FATAL_FAILURE(ExpectAppTag(kAppId, "foo"));
  ASSERT_NO_FATAL_FAILURE(ExpectAppTag(kAppId2, "foo2"));

  ASSERT_NO_FATAL_FAILURE(ExpectUninstallPing(&test_server));
  ASSERT_NO_FATAL_FAILURE(Uninstall());
}

TEST_F(IntegrationTest, ChangeTag) {
  ScopedServer test_server(test_commands_);
  const std::string kAppId("test");
  const base::Version v1("1");
  ASSERT_NO_FATAL_FAILURE(ExpectInstallSequence(
      &test_server, kAppId, "", UpdateService::Priority::kForeground,
      base::Version({0, 0, 0, 0}), v1));
  ASSERT_NO_FATAL_FAILURE(InstallUpdaterAndApp(
      kAppId, /*is_silent_install=*/true,
      base::StrCat({"appguid=", kAppId, "&ap=foo&usagestats=1"})));
  ASSERT_NO_FATAL_FAILURE(ExpectInstallSequence(
      &test_server, kAppId, "", UpdateService::Priority::kForeground,
      base::Version({1}), v1));
  ASSERT_NO_FATAL_FAILURE(InstallUpdaterAndApp(
      kAppId, /*is_silent_install=*/true,
      base::StrCat({"appguid=", kAppId, "&ap=foo2&usagestats=1"})));
  ASSERT_TRUE(WaitForUpdaterExit());

  ASSERT_NO_FATAL_FAILURE(ExpectAppVersion(kAppId, v1));
  ASSERT_NO_FATAL_FAILURE(ExpectAppTag(kAppId, "foo2"));

  ASSERT_NO_FATAL_FAILURE(ExpectUninstallPing(&test_server));
  ASSERT_NO_FATAL_FAILURE(Uninstall());
}

TEST_F(IntegrationTest, SetTagRoundTrip) {
  ASSERT_NO_FATAL_FAILURE(Install());

  ASSERT_NO_FATAL_FAILURE(InstallApp("test"));
  ASSERT_NO_FATAL_FAILURE(ExpectAppTag("test", ""));

  ASSERT_NO_FATAL_FAILURE(SetAppTag("test", "abc"));
  ASSERT_NO_FATAL_FAILURE(ExpectAppTag("test", "abc"));

  ASSERT_NO_FATAL_FAILURE(Uninstall());
}

TEST_F(IntegrationTest, MultipleWakesOneNetRequest) {
  ScopedServer test_server(test_commands_);
  ASSERT_NO_FATAL_FAILURE(Install());

  // Only one sequence visible to the server despite multiple wakes.
  ASSERT_NO_FATAL_FAILURE(ExpectNoUpdateSequence(&test_server, kUpdaterAppId));
  ASSERT_NO_FATAL_FAILURE(RunWake(0));
  ASSERT_NO_FATAL_FAILURE(RunWake(0));

  ASSERT_NO_FATAL_FAILURE(ExpectUninstallPing(&test_server));
  ASSERT_NO_FATAL_FAILURE(Uninstall());
}

TEST_F(IntegrationTest, MultipleUpdateAllsMultipleNetRequests) {
  ScopedServer test_server(test_commands_);
  ASSERT_NO_FATAL_FAILURE(Install());

  ASSERT_NO_FATAL_FAILURE(ExpectNoUpdateSequence(&test_server, kUpdaterAppId));
  ASSERT_NO_FATAL_FAILURE(UpdateAll());
  ASSERT_NO_FATAL_FAILURE(ExpectNoUpdateSequence(&test_server, kUpdaterAppId));
  ASSERT_NO_FATAL_FAILURE(UpdateAll());

  ASSERT_NO_FATAL_FAILURE(ExpectUninstallPing(&test_server));
  ASSERT_NO_FATAL_FAILURE(Uninstall());
}

TEST_F(IntegrationTest, GetAppStates) {
  ScopedServer test_server(test_commands_);
  ASSERT_NO_FATAL_FAILURE(Install());

  const std::string kAppId("test");
  const base::Version v1("0.1");
  ASSERT_NO_FATAL_FAILURE(InstallApp(kAppId));

  ASSERT_NO_FATAL_FAILURE(ExpectAppVersion(kAppId, v1));

  base::Value::Dict expected_app_state;
  expected_app_state.Set("app_id", kAppId);
  expected_app_state.Set("version", v1.GetString());
  expected_app_state.Set("ap", "");
  expected_app_state.Set("brand_code", "");
  expected_app_state.Set("brand_path", "");
  expected_app_state.Set("ecp", "");
  expected_app_state.Set("cohort", "");
  base::Value::Dict expected_app_states;
  expected_app_states.Set(kAppId, std::move(expected_app_state));

  ASSERT_NO_FATAL_FAILURE(GetAppStates(expected_app_states));

  ASSERT_NO_FATAL_FAILURE(ExpectUninstallPing(&test_server));
  ASSERT_NO_FATAL_FAILURE(Uninstall());
}

TEST_F(IntegrationTest, UnregisterUninstalledApp) {
  ASSERT_NO_FATAL_FAILURE(Install());
  ASSERT_NO_FATAL_FAILURE(ExpectInstalled());
  ASSERT_NO_FATAL_FAILURE(InstallApp("test1"));
  ASSERT_NO_FATAL_FAILURE(InstallApp("test2"));

  ASSERT_TRUE(WaitForUpdaterExit());
  ASSERT_NO_FATAL_FAILURE(ExpectVersionActive(kUpdaterVersion));
  ASSERT_NO_FATAL_FAILURE(UninstallApp("test1"));

  ASSERT_NO_FATAL_FAILURE(RunWake(0));

  ASSERT_TRUE(WaitForUpdaterExit());
  ASSERT_NO_FATAL_FAILURE(ExpectInstalled());
  ASSERT_NO_FATAL_FAILURE(ExpectNotRegistered("test1"));
  ASSERT_NO_FATAL_FAILURE(ExpectRegistered("test2"));

  ASSERT_NO_FATAL_FAILURE(Uninstall());
}

TEST_F(IntegrationTest, UninstallIfMaxServerWakesBeforeRegistrationExceeded) {
  ASSERT_NO_FATAL_FAILURE(Install());
  ASSERT_TRUE(WaitForUpdaterExit());
  ASSERT_NO_FATAL_FAILURE(ExpectInstalled());
  ASSERT_NO_FATAL_FAILURE(SetServerStarts(24));
  ASSERT_NO_FATAL_FAILURE(RunWake(0));
  ASSERT_TRUE(WaitForUpdaterExit());
}

TEST_F(IntegrationTest, UninstallUpdaterWhenAllAppsUninstalled) {
  ASSERT_NO_FATAL_FAILURE(Install());
  ASSERT_NO_FATAL_FAILURE(InstallApp("test1"));
  ASSERT_NO_FATAL_FAILURE(ExpectInstalled());
  ASSERT_TRUE(WaitForUpdaterExit());
  ASSERT_NO_FATAL_FAILURE(SetServerStarts(24));
  ASSERT_NO_FATAL_FAILURE(RunWake(0));
  ASSERT_TRUE(WaitForUpdaterExit());
  ASSERT_NO_FATAL_FAILURE(ExpectInstalled());
  ASSERT_NO_FATAL_FAILURE(ExpectVersionActive(kUpdaterVersion));
  ASSERT_NO_FATAL_FAILURE(UninstallApp("test1"));
  ASSERT_NO_FATAL_FAILURE(RunWake(0));
  ASSERT_TRUE(WaitForUpdaterExit());
}

TEST_F(IntegrationTest, RotateLog) {
  ASSERT_NO_FATAL_FAILURE(Install());
  ASSERT_TRUE(WaitForUpdaterExit());
  ASSERT_NO_FATAL_FAILURE(FillLog());
  ASSERT_NO_FATAL_FAILURE(RunWake(0));
  ASSERT_TRUE(WaitForUpdaterExit());
  ASSERT_NO_FATAL_FAILURE(ExpectLogRotated());
  ASSERT_NO_FATAL_FAILURE(Uninstall());
}

#if BUILDFLAG(CHROMIUM_BRANDING) || BUILDFLAG(GOOGLE_CHROME_BRANDING)

TEST_F(IntegrationTest, SelfUpdateFromOldReal) {
  ScopedServer test_server(test_commands_);

  ASSERT_NO_FATAL_FAILURE(SetupRealUpdaterLowerVersion());
  ASSERT_NO_FATAL_FAILURE(ExpectVersionNotActive(kUpdaterVersion));

  // Trigger an old instance update check.
  ASSERT_NO_FATAL_FAILURE(ExpectSelfUpdateSequence(&test_server));
  ASSERT_NO_FATAL_FAILURE(RunWakeActive(0));

  // Qualify the new instance.
  ASSERT_NO_FATAL_FAILURE(
      ExpectUpdateSequence(&test_server, kQualificationAppId, "",
                           UpdateService::Priority::kBackground,
                           base::Version("0.1"), base::Version("0.2")));
  ASSERT_NO_FATAL_FAILURE(RunWake(0));
  ASSERT_TRUE(WaitForUpdaterExit());

  // Activate the new instance. (It should not check itself for updates.)
  ASSERT_NO_FATAL_FAILURE(RunWake(0));
  ASSERT_TRUE(WaitForUpdaterExit());

  ASSERT_NO_FATAL_FAILURE(ExpectVersionActive(kUpdaterVersion));
  ASSERT_NO_FATAL_FAILURE(ExpectUninstallPing(&test_server));
  ASSERT_NO_FATAL_FAILURE(Uninstall());
}

TEST_F(IntegrationTest, UninstallIfUnusedSelfAndOldReal) {
  ScopedServer test_server(test_commands_);

  ASSERT_NO_FATAL_FAILURE(SetupRealUpdaterLowerVersion());
  ASSERT_NO_FATAL_FAILURE(ExpectVersionNotActive(kUpdaterVersion));

  // Trigger an old instance update check.
  ASSERT_NO_FATAL_FAILURE(ExpectSelfUpdateSequence(&test_server));
  ASSERT_NO_FATAL_FAILURE(RunWakeActive(0));

  // Qualify the new instance.
  ASSERT_NO_FATAL_FAILURE(
      ExpectUpdateSequence(&test_server, kQualificationAppId, "",
                           UpdateService::Priority::kBackground,
                           base::Version("0.1"), base::Version("0.2")));
  ASSERT_NO_FATAL_FAILURE(RunWake(0));
  ASSERT_TRUE(WaitForUpdaterExit());

  // Activate the new instance. (It should not check itself for updates.)
  ASSERT_NO_FATAL_FAILURE(RunWake(0));
  ASSERT_TRUE(WaitForUpdaterExit());

  ASSERT_NO_FATAL_FAILURE(ExpectVersionActive(kUpdaterVersion));

  ASSERT_NO_FATAL_FAILURE(ExpectUninstallPing(&test_server));
  ASSERT_NO_FATAL_FAILURE(SetServerStarts(24));
  ASSERT_NO_FATAL_FAILURE(RunWake(0));
  ASSERT_TRUE(WaitForUpdaterExit());

  // Expect that the updater uninstalled itself as well as the lower version.
}

// Tests that installing and uninstalling an old version of the updater from
// CIPD is possible.
TEST_F(IntegrationTest, InstallLowerVersion) {
  ASSERT_NO_FATAL_FAILURE(SetupRealUpdaterLowerVersion());
  ASSERT_NO_FATAL_FAILURE(ExpectVersionNotActive(kUpdaterVersion));
  ASSERT_NO_FATAL_FAILURE(Uninstall());

#if BUILDFLAG(IS_WIN)
  // This deletes a tree of empty subdirectories corresponding to the crash
  // handler of the lower version updater installed above. `Uninstall` runs
  // `updater --uninstall` from the out directory of the build, which attempts
  // to launch the `uninstall.cmd` script corresponding to this version of the
  // updater from the install directory. However, there is no such script
  // because this version was never installed, and the script is not found
  // there.
  ASSERT_NO_FATAL_FAILURE(DeleteUpdaterDirectory());
#endif  // IS_WIN
}

#endif  // BUILDFLAG(CHROMIUM_BRANDING) || BUILDFLAG(GOOGLE_CHROME_BRANDING)

TEST_F(IntegrationTest, MAYBE_UpdateServiceStress) {
  ASSERT_NO_FATAL_FAILURE(Install());
  ASSERT_NO_FATAL_FAILURE(ExpectInstalled());
  ASSERT_NO_FATAL_FAILURE(StressUpdateService());
  ASSERT_NO_FATAL_FAILURE(Uninstall());
}

TEST_F(IntegrationTest, IdleServerExits) {
#if BUILDFLAG(IS_WIN)
  if (IsSystemInstall(GetUpdaterScopeForTesting())) {
    GTEST_SKIP() << "System server startup is complicated on Windows.";
  }
#endif
  ASSERT_NO_FATAL_FAILURE(EnterTestMode(
      GURL("http://localhost:1234"), GURL("http://localhost:1234"),
      GURL("http://localhost:1234"), {}, base::Seconds(1)));
  ASSERT_NO_FATAL_FAILURE(Install());
  ASSERT_NO_FATAL_FAILURE(ExpectInstalled());
  ASSERT_NO_FATAL_FAILURE(RunServer(kErrorIdle, true));
  ASSERT_NO_FATAL_FAILURE(RunServer(kErrorIdle, false));
  ASSERT_NO_FATAL_FAILURE(Uninstall());
}

TEST_F(IntegrationTest, SameVersionUpdate) {
  ScopedServer test_server(test_commands_);
  ASSERT_NO_FATAL_FAILURE(Install());
  ASSERT_NO_FATAL_FAILURE(ExpectInstalled());

  const std::string app_id = "test-appid";
  ASSERT_NO_FATAL_FAILURE(InstallApp(app_id));

  const std::string response = base::StringPrintf(
      ")]}'\n"
      R"({"response":{)"
      R"(  "protocol":"3.1",)"
      R"(  "app":[)"
      R"(    {)"
      R"(      "appid":"%s",)"
      R"(      "status":"ok",)"
      R"(      "updatecheck":{)"
      R"(        "status":"noupdate")"
      R"(      })"
      R"(    })"
      R"(  ])"
      R"(}})",
      app_id.c_str());
  test_server.ExpectOnce(
      {request::GetUpdaterUserAgentMatcher(),
       request::GetContentMatcher(
           {R"("updatecheck":{"sameversionupdate":true},"version":"0.1"}.*)"})},
      response);
  ASSERT_NO_FATAL_FAILURE(CallServiceUpdate(
      app_id, "", UpdateService::PolicySameVersionUpdate::kAllowed));

  test_server.ExpectOnce({request::GetUpdaterUserAgentMatcher(),
                          request::GetContentMatcher(
                              {R"(.*"updatecheck":{},"version":"0.1"}.*)"})},
                         response);
  ASSERT_NO_FATAL_FAILURE(CallServiceUpdate(
      app_id, "", UpdateService::PolicySameVersionUpdate::kNotAllowed));
  ASSERT_NO_FATAL_FAILURE(ExpectUninstallPing(&test_server));
  ASSERT_NO_FATAL_FAILURE(Uninstall());
}

TEST_F(IntegrationTest, InstallDataIndex) {
  ScopedServer test_server(test_commands_);
  ASSERT_NO_FATAL_FAILURE(Install());
  ASSERT_NO_FATAL_FAILURE(ExpectInstalled());

  const std::string app_id = "test-appid";
  const std::string install_data_index = "test-install-data-index";

  ASSERT_NO_FATAL_FAILURE(InstallApp(app_id));

  const std::string response = base::StringPrintf(
      ")]}'\n"
      R"({"response":{)"
      R"(  "protocol":"3.1",)"
      R"(  "app":[)"
      R"(    {)"
      R"(      "appid":"%s",)"
      R"(      "status":"ok",)"
      R"(      "updatecheck":{)"
      R"(        "status":"noupdate")"
      R"(      })"
      R"(    })"
      R"(  ])"
      R"(}})",
      app_id.c_str());

  test_server.ExpectOnce(
      {request::GetUpdaterUserAgentMatcher(),
       request::GetContentMatcher({base::StringPrintf(
           R"(.*"data":\[{"index":"%s","name":"install"}],.*)",
           install_data_index.c_str())})},
      response);

  ASSERT_NO_FATAL_FAILURE(
      CallServiceUpdate(app_id, install_data_index,
                        UpdateService::PolicySameVersionUpdate::kAllowed));

  ASSERT_NO_FATAL_FAILURE(ExpectUninstallPing(&test_server));
  ASSERT_NO_FATAL_FAILURE(Uninstall());
}

TEST_F(IntegrationTest, MigrateLegacyUpdater) {
  ASSERT_NO_FATAL_FAILURE(SetupFakeLegacyUpdater());
#if BUILDFLAG(IS_WIN)
  ASSERT_NO_FATAL_FAILURE(RunFakeLegacyUpdater());
#endif  // BUILDFLAG(IS_WIN)
  ASSERT_NO_FATAL_FAILURE(Install());
  ASSERT_TRUE(WaitForUpdaterExit());
  ASSERT_NO_FATAL_FAILURE(ExpectInstalled());
  ASSERT_NO_FATAL_FAILURE(ExpectLegacyUpdaterMigrated());
  ASSERT_NO_FATAL_FAILURE(Uninstall());
}

TEST_F(IntegrationTest, RecoveryNoUpdater) {
  const std::string appid = "test1";
  const base::Version version("0.1");
  ASSERT_NO_FATAL_FAILURE(RunRecoveryComponent(appid, version));
  ASSERT_TRUE(WaitForUpdaterExit());
  ASSERT_NO_FATAL_FAILURE(ExpectInstalled());
  ASSERT_NO_FATAL_FAILURE(ExpectAppVersion(appid, version));
  ASSERT_NO_FATAL_FAILURE(Uninstall());
}

TEST_F(IntegrationTest, RegisterApp) {
  ASSERT_NO_FATAL_FAILURE(Install());
  ASSERT_NO_FATAL_FAILURE(ExpectInstalled());
  ASSERT_TRUE(WaitForUpdaterExit());

  RegistrationRequest registration;
  registration.app_id = "e595682b-02d5-46d1-b7ab-90034bd6be0f";
  registration.brand_code = "TSBD";
  registration.brand_path = base::FilePath::FromASCII("/bp");
  registration.ap = "TestAp";
  registration.version = base::Version("11.22.33.44");
  registration.existence_checker_path = base::FilePath::FromASCII("/tmp");
  registration.cohort = "cohort_test";
  test_commands_->RegisterApp(registration);

  base::Value::Dict expected_app_state;
  expected_app_state.Set("app_id", "e595682b-02d5-46d1-b7ab-90034bd6be0f");
  expected_app_state.Set("brand_code", "TSBD");
  expected_app_state.Set("brand_path", "/bp");
  expected_app_state.Set("ap", "TestAp");
  expected_app_state.Set("version", "11.22.33.44");
  expected_app_state.Set("ecp", "/tmp");
#if BUILDFLAG(IS_POSIX)
  // Cohort is only communicated over IPC on POSIX. Refer to crbug.com/40283110.
  expected_app_state.Set("cohort", "cohort_test");
#endif
  base::Value::Dict expected_app_states;
  expected_app_states.Set("e595682b-02d5-46d1-b7ab-90034bd6be0f",
                          std::move(expected_app_state));
  ASSERT_NO_FATAL_FAILURE(GetAppStates(expected_app_states));
  ASSERT_NO_FATAL_FAILURE(Uninstall());
}

TEST_F(IntegrationTest, CrashUsageStatsEnabled) {
#if BUILDFLAG(IS_WIN) && defined(ADDRESS_SANITIZER)
  GTEST_SKIP() << "Crash tests disabled for Win ASAN.";
#else
  ScopedServer test_server(test_commands_);
  ASSERT_NO_FATAL_FAILURE(Install());
  ASSERT_NO_FATAL_FAILURE(ExpectInstalled());
  ASSERT_TRUE(WaitForUpdaterExit());

  const std::string response;
  test_server.ExpectOnce(
      {
          request::GetPathMatcher(
              base::StringPrintf(R"(%s\?product=%s&version=%s&guid=.*)",
                                 test_server.crash_report_path().c_str(),
                                 CRASH_PRODUCT_NAME, kUpdaterVersion)),
          request::GetHeaderMatcher({{"User-Agent", R"(Crashpad/.*)"}}),
          request::GetMultipartContentMatcher({
              {"guid", std::vector<std::string>({})},  // Crash guid.
              {"prod", std::vector<std::string>({CRASH_PRODUCT_NAME})},
              {"ver", std::vector<std::string>({kUpdaterVersion})},
              {"upload_file_minidump",  // Dump file name and its content.
               std::vector<std::string>(
                   {R"(filename=".*dmp")",
                    R"(Content-Type: application/octet-stream)", R"(MDMP)"})},
          }),
      },
      response);
  ExpectUninstallPing(&test_server);
  RunCrashMe();
  ASSERT_TRUE(WaitForUpdaterExit());

  // Delete the dmp files generated by this test, so `ExpectNoCrashes` won't
  // complain at TearDown.
  std::optional<base::FilePath> database_path(
      GetCrashDatabasePath(GetUpdaterScopeForTesting()));
  if (database_path && base::PathExists(*database_path)) {
    base::FileEnumerator(*database_path, true, base::FileEnumerator::FILES,
                         FILE_PATH_LITERAL("*.dmp"),
                         base::FileEnumerator::FolderSearchPolicy::ALL)
        .ForEach([](const base::FilePath& name) {
          VLOG(0) << "Deleting file at: " << name;
          EXPECT_TRUE(base::DeleteFile(name));
        });
  }
  ASSERT_NO_FATAL_FAILURE(Uninstall());
#endif
}

class IntegrationTestDeviceManagement : public IntegrationTest {
 public:
  IntegrationTestDeviceManagement() = default;
  ~IntegrationTestDeviceManagement() override = default;

 protected:
  void SetUp() override {
    IntegrationTest::SetUp();
    if (IsSkipped()) {
      return;
    }
    test_server_ = std::make_unique<ScopedServer>(test_commands_);
    if (!IsSystemInstall(GetUpdaterScopeForTesting())) {
      GTEST_SKIP();
    }
    DMCleanup();
    UninstallEnterpriseCompanionApp();
    ASSERT_NO_FATAL_FAILURE(SetMachineManaged(true));
    ASSERT_TRUE(vapid_test_server_.Start());
    InstallEnterpriseCompanionAppOverrides(
        base::Value::Dict()
            .Set("crash_upload_url", test_server_->crash_upload_url().spec())
            .Set("dm_encrypted_reporting_url",
                 vapid_test_server_.base_url().spec())
            .Set("dm_realtime_reporting_url",
                 vapid_test_server_.base_url().spec())
            .Set("dm_server_url", test_server_->device_management_url().spec())
            .Set("event_logging_url", vapid_test_server_.base_url().spec()));
  }

  void TearDown() override {
    if (IsSkipped()) {
      return;
    }
    if (IsSystemInstall(GetUpdaterScopeForTesting())) {
      UninstallEnterpriseCompanionApp();
    }
    DMCleanup();
    IntegrationTest::TearDown();
  }

  void SetCloudPolicyOverridesPlatformPolicy() {
// Cloud policy overrides platform policy default, except on Windows.
#if BUILDFLAG(IS_WIN)
    EXPECT_EQ(ERROR_SUCCESS,
              base::win::RegKey(HKEY_LOCAL_MACHINE, UPDATER_POLICIES_KEY,
                                Wow6432(KEY_WRITE))
                  .WriteValue(L"CloudPolicyOverridesPlatformPolicy", 1));
#endif  // BUILDFLAG(IS_WIN)
  }

  std::unique_ptr<ScopedServer> test_server_;
  // A test server that is not configured with any expectations or interesting
  // responses. This is useful for providing addresses to the enterprise
  // companion app for interactions not intended to be covered by these tests.
  net::test_server::EmbeddedTestServer vapid_test_server_;
  static constexpr char kEnrollmentToken[] =
      "00001111-beef-f00d-2222-333344445555";
  static constexpr char kDMToken[] = "integration-dm-token";

#if BUILDFLAG(IS_WIN)
  static constexpr char kGlobalPolicyKey[] = "";
#else
  static constexpr char kGlobalPolicyKey[] = "global";
#endif  // BUILDFLAG(IS_WIN)
};

// Tests the setup and teardown of the fixture.
TEST_F(IntegrationTestDeviceManagement, Nothing) {}

TEST_F(IntegrationTestDeviceManagement, PolicyFetchBeforeInstall) {
  OmahaSettingsClientProto omaha_settings;
  omaha_settings.set_install_default(
      enterprise_management::INSTALL_DEFAULT_DISABLED);
  omaha_settings.set_download_preference("not-cacheable");
  omaha_settings.set_proxy_mode("system");
  omaha_settings.set_proxy_server("test.proxy.server");
  ApplicationSettings app;
  app.set_app_guid(kApp1.appid);
  app.set_update(enterprise_management::AUTOMATIC_UPDATES_ONLY);
  app.set_target_version_prefix("0.1");
  app.set_rollback_to_target_version(
      enterprise_management::ROLLBACK_TO_TARGET_VERSION_ENABLED);
  omaha_settings.mutable_application_settings()->Add(std::move(app));

  DMPushEnrollmentToken(kEnrollmentToken);

  ExpectDeviceManagementRegistrationRequestFromDefaultPolicyAgent(
      test_server_.get(), kEnrollmentToken, kDMToken);
  ExpectDeviceManagementPolicyFetchRequestFromDefaultPolicyAgent(
      test_server_.get(), kDMToken, omaha_settings);
  ASSERT_NO_FATAL_FAILURE(Install());
  ASSERT_NO_FATAL_FAILURE(ExpectInstalled());

  scoped_refptr<device_management_storage::DMStorage> dm_storage =
      device_management_storage::GetDefaultDMStorage();
  ASSERT_NE(dm_storage, nullptr);
  std::optional<OmahaSettingsClientProto> omaha_policy =
      GetOmahaPolicySettings(dm_storage);
  ASSERT_TRUE(omaha_policy);
  EXPECT_EQ(omaha_policy->download_preference(), "not-cacheable");
  EXPECT_EQ(omaha_policy->proxy_mode(), "system");
  EXPECT_EQ(omaha_policy->proxy_server(), "test.proxy.server");
  ASSERT_GT(omaha_policy->application_settings_size(), 0);
  const ApplicationSettings& app_policy =
      omaha_policy->application_settings()[0];
  EXPECT_EQ(app_policy.app_guid(), kApp1.appid);
  EXPECT_EQ(app_policy.update(), enterprise_management::AUTOMATIC_UPDATES_ONLY);
  EXPECT_EQ(app_policy.target_version_prefix(), "0.1");
  EXPECT_EQ(app_policy.rollback_to_target_version(),
            enterprise_management::ROLLBACK_TO_TARGET_VERSION_ENABLED);
  ASSERT_NO_FATAL_FAILURE(ExpectUninstallPing(test_server_.get()));
  ASSERT_NO_FATAL_FAILURE(Uninstall());
}

#if BUILDFLAG(IS_MAC)
TEST_F(IntegrationTestDeviceManagement, FallbackToOutOfProcessNetworkFetcher) {
  ASSERT_NO_FATAL_FAILURE(InstallBrokenEnterpriseCompanionApp());
  OmahaSettingsClientProto omaha_settings;
  omaha_settings.set_install_default(
      enterprise_management::INSTALL_DEFAULT_DISABLED);
  omaha_settings.set_download_preference("not-cacheable");
  ApplicationSettings app;
  app.set_app_guid(kApp1.appid);
  app.set_update(enterprise_management::AUTOMATIC_UPDATES_ONLY);
  omaha_settings.mutable_application_settings()->Add(std::move(app));

  DMPushEnrollmentToken(kEnrollmentToken);

  // Verify that a single HTTP error from DM server is recovered by the
  // fallback fetcher.
  test_server_->ExpectOnce({}, "", net::HTTP_INTERNAL_SERVER_ERROR);
  ExpectDeviceManagementRegistrationRequest(test_server_.get(),
                                            kEnrollmentToken, kDMToken);
  test_server_->ExpectOnce({}, "", net::HTTP_INTERNAL_SERVER_ERROR);
  ExpectDeviceManagementPolicyFetchRequest(test_server_.get(), kDMToken,
                                           omaha_settings);
  ASSERT_NO_FATAL_FAILURE(Install());
  ASSERT_NO_FATAL_FAILURE(ExpectInstalled());

  scoped_refptr<device_management_storage::DMStorage> dm_storage =
      device_management_storage::GetDefaultDMStorage();
  ASSERT_NE(dm_storage, nullptr);
  std::optional<OmahaSettingsClientProto> omaha_policy =
      GetOmahaPolicySettings(dm_storage);
  ASSERT_TRUE(omaha_policy);
  EXPECT_EQ(omaha_policy->download_preference(), "not-cacheable");
  ASSERT_GT(omaha_policy->application_settings_size(), 0);
  const ApplicationSettings& app_policy =
      omaha_policy->application_settings()[0];
  EXPECT_EQ(app_policy.app_guid(), kApp1.appid);
  EXPECT_EQ(app_policy.update(), enterprise_management::AUTOMATIC_UPDATES_ONLY);
  ASSERT_NO_FATAL_FAILURE(ExpectUninstallPing(test_server_.get()));
  ASSERT_NO_FATAL_FAILURE(UninstallBrokenEnterpriseCompanionApp());
  ASSERT_NO_FATAL_FAILURE(Uninstall());
}
#endif  // BUILDFLAG(IS_MAC)

TEST_F(IntegrationTestDeviceManagement, AppInstall) {
  const base::Version kApp1Version = base::Version("1.0.0.0");
  OmahaSettingsClientProto omaha_settings;
  omaha_settings.set_install_default(
      enterprise_management::INSTALL_DEFAULT_DISABLED);
  ApplicationSettings app;
  app.set_app_guid(kApp1.appid);
  app.set_install(enterprise_management::INSTALL_ENABLED);
  omaha_settings.mutable_application_settings()->Add(std::move(app));

  DMPushEnrollmentToken(kEnrollmentToken);
  ExpectDeviceManagementRegistrationRequestFromDefaultPolicyAgent(
      test_server_.get(), kEnrollmentToken, kDMToken);
  ExpectDeviceManagementPolicyFetchRequestFromDefaultPolicyAgent(
      test_server_.get(), kDMToken, omaha_settings);
  ASSERT_NO_FATAL_FAILURE(Install());
  ASSERT_NO_FATAL_FAILURE(ExpectInstalled());

  ASSERT_NO_FATAL_FAILURE(ExpectAppsUpdateSequence(
      UpdaterScope::kSystem, test_server_.get(),
      /*request_attributes=*/{},
      {
          AppUpdateExpectation(
              kApp1.GetInstallCommandLineArgs(/*install_v1=*/true), kApp1.appid,
              base::Version({0, 0, 0, 0}), kApp1.v1,
              /*is_install=*/true,
              /*should_update=*/true, false, "", "",
              GetInstallerPath(kApp1.v1_crx)),
      }));

  ASSERT_NO_FATAL_FAILURE(InstallAppViaService(kApp1.appid));
  ASSERT_NO_FATAL_FAILURE(InstallAppViaService(kApp2.appid));
  ExpectAppInstalled(kApp1.appid, kApp1.v1);
  ASSERT_NO_FATAL_FAILURE(ExpectNotRegistered(kApp2.appid));

  ASSERT_NO_FATAL_FAILURE(ExpectUninstallPing(test_server_.get()));
  ASSERT_NO_FATAL_FAILURE(Uninstall());
}

TEST_F(IntegrationTestDeviceManagement, ForceInstall) {
  const base::Version kApp1Version = base::Version("1.0.0.0");

  ASSERT_NO_FATAL_FAILURE(Install());
  ASSERT_NO_FATAL_FAILURE(ExpectInstalled());

  // Force-install app1, enable install app2.
  OmahaSettingsClientProto omaha_settings;
  omaha_settings.set_install_default(
      enterprise_management::INSTALL_DEFAULT_DISABLED);
  ApplicationSettings app1;
  app1.set_app_guid(kApp1.appid);
  app1.set_install(enterprise_management::INSTALL_FORCED);
  omaha_settings.mutable_application_settings()->Add(std::move(app1));
  ApplicationSettings app2;
  app2.set_app_guid(kApp2.appid);
  app2.set_install(enterprise_management::INSTALL_ENABLED);
  omaha_settings.mutable_application_settings()->Add(std::move(app2));

  DMPushEnrollmentToken(kEnrollmentToken);
  ExpectDeviceManagementRegistrationRequestFromDefaultPolicyAgent(
      test_server_.get(), kEnrollmentToken, kDMToken);
  ExpectDeviceManagementPolicyFetchRequestFromDefaultPolicyAgent(
      test_server_.get(), kDMToken, omaha_settings);
  ExpectUpdateCheckRequest(test_server_.get());
  ExpectAppsUpdateSequence(
      UpdaterScope::kSystem, test_server_.get(),
      /*request_attributes=*/{},
      {
          AppUpdateExpectation(
              kApp1.GetInstallCommandLineArgs(/*install_v1=*/true), kApp1.appid,
              base::Version({0, 0, 0, 0}), kApp1.v1,
              /*is_install=*/true,
              /*should_update=*/true, false, "", "",
              GetInstallerPath(kApp1.v1_crx)),
      });

  ASSERT_NO_FATAL_FAILURE(RunWake(0));
  ASSERT_TRUE(WaitForUpdaterExit());
  ExpectAppInstalled(kApp1.appid, kApp1.v1);
  ASSERT_NO_FATAL_FAILURE(ExpectNotRegistered(kApp2.appid));

  ASSERT_NO_FATAL_FAILURE(ExpectUninstallPing(test_server_.get()));
  ASSERT_NO_FATAL_FAILURE(Uninstall());
}

TEST_F(IntegrationTestDeviceManagement, QualifyUpdaterWhenUpdateDisabled) {
  // Disable global update via CBCM.
  DMPushEnrollmentToken(kEnrollmentToken);
  OmahaSettingsClientProto omaha_settings;
  omaha_settings.set_update_default(enterprise_management::UPDATES_DISABLED);
  omaha_settings.set_cloud_policy_overrides_platform_policy(true);
  ExpectDeviceManagementRegistrationRequestFromDefaultPolicyAgent(
      test_server_.get(), kEnrollmentToken, kDMToken);
  ExpectDeviceManagementPolicyFetchRequestFromDefaultPolicyAgent(
      test_server_.get(), kDMToken, omaha_settings);
  ASSERT_NO_FATAL_FAILURE(Install());
  ASSERT_NO_FATAL_FAILURE(ExpectInstalled());
  ASSERT_TRUE(WaitForUpdaterExit());
  ASSERT_NO_FATAL_FAILURE(SetupFakeUpdaterLowerVersion());
  ASSERT_NO_FATAL_FAILURE(ExpectVersionNotActive(kUpdaterVersion));

  ASSERT_NO_FATAL_FAILURE(
      ExpectUpdateSequence(test_server_.get(), kQualificationAppId, "",
                           UpdateService::Priority::kBackground,
                           base::Version("0.1"), base::Version("0.2")));
  ASSERT_NO_FATAL_FAILURE(RunWake(0));
  ASSERT_TRUE(WaitForUpdaterExit());

  // Verify the new instance is qualified and activated itself.
  ExpectDeviceManagementPolicyFetchRequestFromDefaultPolicyAgent(
      test_server_.get(), kDMToken, omaha_settings, /*first_request=*/false);
  test_server_->ExpectOnce({request::GetUpdaterUserAgentMatcher(),
                            request::GetContentMatcher(
                                {base::StringPrintf(".*%s.*", kUpdaterAppId)})},
                           ")]}'\n");
  ASSERT_NO_FATAL_FAILURE(RunWake(0));
  ASSERT_TRUE(WaitForUpdaterExit());
  ASSERT_NO_FATAL_FAILURE(ExpectVersionActive(kUpdaterVersion));
  ASSERT_NO_FATAL_FAILURE(ExpectUninstallPing(test_server_.get()));
  ASSERT_NO_FATAL_FAILURE(Uninstall());
}

TEST_F(IntegrationTestDeviceManagement,
       QualifyUpdaterWhenNextCheckDelayIsZero) {
  // Set update check period to zero via CBCM.
  DMPushEnrollmentToken(kEnrollmentToken);
  OmahaSettingsClientProto omaha_settings;
  omaha_settings.set_auto_update_check_period_minutes(0);
  omaha_settings.set_cloud_policy_overrides_platform_policy(true);
  ExpectDeviceManagementRegistrationRequestFromDefaultPolicyAgent(
      test_server_.get(), kEnrollmentToken, kDMToken);
  ExpectDeviceManagementPolicyFetchRequestFromDefaultPolicyAgent(
      test_server_.get(), kDMToken, omaha_settings);
  ASSERT_NO_FATAL_FAILURE(Install());
  ASSERT_NO_FATAL_FAILURE(ExpectInstalled());
  ASSERT_TRUE(WaitForUpdaterExit());
  ASSERT_NO_FATAL_FAILURE(SetupFakeUpdaterLowerVersion());
  ASSERT_NO_FATAL_FAILURE(ExpectVersionNotActive(kUpdaterVersion));

  ASSERT_NO_FATAL_FAILURE(RunWake(0));
  ASSERT_TRUE(WaitForUpdaterExit());

  // Verify the new instance is qualified and activated itself.
  ExpectDeviceManagementPolicyFetchRequestFromDefaultPolicyAgent(
      test_server_.get(), kDMToken, omaha_settings, /*first_request=*/false);
  ASSERT_NO_FATAL_FAILURE(RunWake(0));
  ASSERT_TRUE(WaitForUpdaterExit());
  ASSERT_NO_FATAL_FAILURE(ExpectVersionActive(kUpdaterVersion));
  ASSERT_NO_FATAL_FAILURE(ExpectUninstallPing(test_server_.get()));
  ASSERT_NO_FATAL_FAILURE(Uninstall());
}

#ifdef INCLUDE_ENTERPRISE_COMPANION_IN_INSTALLER
// During the updater's installation and periodic tasks, the enterprise
// companion app should not be installed if the device is not cloud managed.
TEST_F(IntegrationTestDeviceManagement, FetchPolicy_SkipCompanionAppInstall) {
  ASSERT_NO_FATAL_FAILURE(Install());
  ASSERT_NO_FATAL_FAILURE(ExpectInstalled());
  ASSERT_TRUE(WaitForUpdaterExit());

  ASSERT_NO_FATAL_FAILURE(ExpectEnterpriseCompanionAppNotInstalled());

  ExpectUpdateCheckRequest(test_server_.get());
  ASSERT_NO_FATAL_FAILURE(RunWake(0));
  ASSERT_TRUE(WaitForUpdaterExit());

  ASSERT_NO_FATAL_FAILURE(ExpectEnterpriseCompanionAppNotInstalled());

  ASSERT_NO_FATAL_FAILURE(ExpectUninstallPing(test_server_.get()));
  ASSERT_NO_FATAL_FAILURE(Uninstall());
}
#endif  // INCLUDE_ENTERPRISE_COMPANION_IN_INSTALLER

TEST_F(IntegrationTestDeviceManagement,
       LongLivedUpdaterIsHealthyWithBrokenCompanionApp) {
  EnterTestMode(test_server_->update_url(), test_server_->crash_upload_url(),
                test_server_->device_management_url(), {},
                /*idle_timeout=*/base::Minutes(5),
                /*server_keep_alive_time=*/base::Seconds(3),
                /*ceca_connection_timeout=*/base::Seconds(1));
  ASSERT_NO_FATAL_FAILURE(InstallBrokenEnterpriseCompanionApp());

  DMPushEnrollmentToken(kEnrollmentToken);
  ApplicationSettings app;
  app.set_app_guid(kApp1.appid);
  app.set_update(enterprise_management::AUTOMATIC_UPDATES_ONLY);
  OmahaSettingsClientProto omaha_settings;
  omaha_settings.mutable_application_settings()->Add(std::move(app));
  ExpectDeviceManagementRegistrationRequest(test_server_.get(),
                                            kEnrollmentToken, kDMToken);
  ExpectDeviceManagementPolicyFetchRequest(test_server_.get(), kDMToken,
                                           omaha_settings);
  ASSERT_NO_FATAL_FAILURE(Install());
  ASSERT_NO_FATAL_FAILURE(ExpectInstalled());
  ASSERT_TRUE(WaitForUpdaterExit());

  ASSERT_NO_FATAL_FAILURE(ExpectUninstallPing(test_server_.get()));
  ASSERT_NO_FATAL_FAILURE(UninstallBrokenEnterpriseCompanionApp());
  ASSERT_NO_FATAL_FAILURE(Uninstall());
}

// During the updater's installation, the updater should fall back to it's own
// policy fetching implementation if the enterprise companion app is broken.
TEST_F(IntegrationTestDeviceManagement, FetchPolicy_FallbackFromCompanionApp) {
  ASSERT_NO_FATAL_FAILURE(InstallBrokenEnterpriseCompanionApp());

  DMPushEnrollmentToken(kEnrollmentToken);
  OmahaSettingsClientProto omaha_settings;
  omaha_settings.set_install_default(
      enterprise_management::INSTALL_DEFAULT_DISABLED);
  omaha_settings.set_download_preference("not-cacheable");
  ApplicationSettings app;
  app.set_app_guid(kApp1.appid);
  app.set_update(enterprise_management::AUTOMATIC_UPDATES_ONLY);
  omaha_settings.mutable_application_settings()->Add(std::move(app));
  ExpectDeviceManagementRegistrationRequest(test_server_.get(),
                                            kEnrollmentToken, kDMToken);
  ExpectDeviceManagementPolicyFetchRequest(test_server_.get(), kDMToken,
                                           omaha_settings);
  ASSERT_NO_FATAL_FAILURE(Install());
  ASSERT_NO_FATAL_FAILURE(ExpectInstalled());
  ASSERT_TRUE(WaitForUpdaterExit());

  // Verify the policies downloaded by the updater.
  scoped_refptr<device_management_storage::DMStorage> dm_storage =
      device_management_storage::GetDefaultDMStorage();
  ASSERT_NE(dm_storage, nullptr);
  std::optional<OmahaSettingsClientProto> omaha_policy =
      GetOmahaPolicySettings(dm_storage);
  ASSERT_TRUE(omaha_policy);
  EXPECT_EQ(omaha_policy->install_default(),
            enterprise_management::INSTALL_DEFAULT_DISABLED);
  EXPECT_EQ(omaha_policy->download_preference(), "not-cacheable");
  ASSERT_GT(omaha_policy->application_settings_size(), 0);
  const ApplicationSettings& app_policy =
      omaha_policy->application_settings()[0];
  EXPECT_EQ(app_policy.app_guid(), kApp1.appid);
  EXPECT_EQ(app_policy.update(), enterprise_management::AUTOMATIC_UPDATES_ONLY);

  ASSERT_NO_FATAL_FAILURE(ExpectUninstallPing(test_server_.get()));
  ASSERT_NO_FATAL_FAILURE(UninstallBrokenEnterpriseCompanionApp());
  ASSERT_NO_FATAL_FAILURE(Uninstall());
}

TEST_F(IntegrationTestDeviceManagement, FetchPolicyViaCompanionApp) {
  ASSERT_NO_FATAL_FAILURE(Install());
  ASSERT_NO_FATAL_FAILURE(ExpectInstalled());

  InstallEnterpriseCompanionApp();
  ASSERT_TRUE(WaitForUpdaterExit());

  OmahaSettingsClientProto omaha_settings;
  omaha_settings.set_install_default(
      enterprise_management::INSTALL_DEFAULT_DISABLED);
  omaha_settings.set_download_preference("not-cacheable");
  ApplicationSettings app;
  app.set_app_guid(kApp1.appid);
  app.set_update(enterprise_management::AUTOMATIC_UPDATES_ONLY);
  omaha_settings.mutable_application_settings()->Add(std::move(app));

  DMPushEnrollmentToken(kEnrollmentToken);
  ExpectDeviceManagementRegistrationRequestViaCompanionApp(
      test_server_.get(), kEnrollmentToken, kDMToken);
  ExpectDeviceManagementPolicyFetchRequestViaCompanionApp(
      test_server_.get(), kDMToken, omaha_settings);

  ExpectUpdateCheckRequest(test_server_.get());
  ASSERT_NO_FATAL_FAILURE(RunWake(0));
  ASSERT_TRUE(WaitForUpdaterExit());

  // Verify the policies downloaded by the companion app.
  scoped_refptr<device_management_storage::DMStorage> dm_storage =
      device_management_storage::GetDefaultDMStorage();
  ASSERT_NE(dm_storage, nullptr);
  std::optional<OmahaSettingsClientProto> omaha_policy =
      GetOmahaPolicySettings(dm_storage);
  ASSERT_TRUE(omaha_policy);
  EXPECT_EQ(omaha_policy->install_default(),
            enterprise_management::INSTALL_DEFAULT_DISABLED);
  EXPECT_EQ(omaha_policy->download_preference(), "not-cacheable");
  ASSERT_GT(omaha_policy->application_settings_size(), 0);
  const ApplicationSettings& app_policy =
      omaha_policy->application_settings()[0];
  EXPECT_EQ(app_policy.app_guid(), kApp1.appid);
  EXPECT_EQ(app_policy.update(), enterprise_management::AUTOMATIC_UPDATES_ONLY);

  ASSERT_NO_FATAL_FAILURE(ExpectUninstallPing(test_server_.get()));
  ASSERT_NO_FATAL_FAILURE(Uninstall());
}

#if !defined(ADDRESS_SANITIZER)
TEST_F(IntegrationTestDeviceManagement,
       UninstallCompanionAppWhenUninstallUpdater) {
  ASSERT_NO_FATAL_FAILURE(ExpectInstallSequence(
      test_server_.get(), kApp1.appid, "", UpdateService::Priority::kForeground,
      base::Version({0, 0, 0, 0}), kApp1.v1));
  ASSERT_NO_FATAL_FAILURE(InstallUpdaterAndApp(
      kApp1.appid, /*is_silent_install=*/true,
      base::StrCat({"appguid=", kApp1.appid, "&usagestats=1"})));
  ASSERT_TRUE(WaitForUpdaterExit());

  ASSERT_NO_FATAL_FAILURE(InstallEnterpriseCompanionApp());

  const base::FilePath companion_app_exe =
      enterprise_companion::GetInstallDirectory()->AppendASCII(
          kCompanionAppExecutableName);
  ASSERT_TRUE(base::PathExists(companion_app_exe));

  // Uninstall ping for the app.
  ASSERT_NO_FATAL_FAILURE(ExpectUninstallPing(test_server_.get()));
  // Expect an update check and then the uninstall ping for the updater itself.
  ExpectUpdateCheckRequest(test_server_.get());
  ASSERT_NO_FATAL_FAILURE(ExpectUninstallPing(test_server_.get()));
  ASSERT_NO_FATAL_FAILURE(UninstallApp(kApp1.appid));
  ASSERT_NO_FATAL_FAILURE(RunWake(0));
  ASSERT_TRUE(WaitForUpdaterExit());
  ASSERT_TRUE(WaitFor([&] { return !base::PathExists(companion_app_exe); }));
  ASSERT_NO_FATAL_FAILURE(ExpectUninstallPing(test_server_.get()));
  ASSERT_NO_FATAL_FAILURE(Uninstall());
}
#endif

#if BUILDFLAG(IS_WIN)
// RuntimeEnrollmentToken is supported on Windows only.
TEST_F(IntegrationTestDeviceManagement, RuntimeEnrollmentToken) {
  ExpectDeviceManagementRegistrationRequestFromDefaultPolicyAgent(
      test_server_.get(), kEnrollmentToken, kDMToken);
  ExpectDeviceManagementPolicyFetchRequestFromDefaultPolicyAgent(
      test_server_.get(), kDMToken, OmahaSettingsClientProto());
  ASSERT_NO_FATAL_FAILURE(ExpectInstallSequence(
      test_server_.get(), kApp1.appid, "", UpdateService::Priority::kForeground,
      base::Version({0, 0, 0, 0}), kApp1.v1));
  ASSERT_NO_FATAL_FAILURE(InstallUpdaterAndApp(
      kApp1.appid, /*is_silent_install=*/true,
      base::StrCat({"etoken=", kEnrollmentToken, "&appguid=", kApp1.appid,
                    "&usagestats=1"})));
  ASSERT_TRUE(WaitForUpdaterExit());
  ASSERT_NO_FATAL_FAILURE(ExpectAppVersion(kApp1.appid, kApp1.v1));
  ASSERT_NO_FATAL_FAILURE(ExpectUninstallPing(test_server_.get()));
  ASSERT_NO_FATAL_FAILURE(Uninstall());
}

// This test depends on platform policy overriding cloud policy, which is not
// the default on POSIX. Therefore, this test is Windows only.
TEST_F(IntegrationTestDeviceManagement, AppUpdateConflictPolicies) {
  ASSERT_NO_FATAL_FAILURE(Install());
  ASSERT_NO_FATAL_FAILURE(ExpectInstalled());
  ASSERT_NO_FATAL_FAILURE(InstallTestApp(kApp1, /*install_v1=*/true));
  ASSERT_NO_FATAL_FAILURE(InstallTestApp(kApp2, /*install_v1=*/true));
  ASSERT_NO_FATAL_FAILURE(InstallTestApp(kApp3, /*install_v1=*/true));

  base::Value::Dict policies;
  policies.Set(kApp2.appid, base::Value::Dict().Set("Update", kPolicyEnabled));
  ASSERT_NO_FATAL_FAILURE(SetPlatformPolicies(policies));

  // Cloud policy sets update default to disabled, app1 to auto-update, and
  // app2 to manual-update.
  DMPushEnrollmentToken(kEnrollmentToken);
  ExpectDeviceManagementRegistrationRequestFromDefaultPolicyAgent(
      test_server_.get(), kEnrollmentToken, kDMToken);
  OmahaSettingsClientProto omaha_settings;
  omaha_settings.set_update_default(enterprise_management::UPDATES_DISABLED);
  ApplicationSettings app1;
  app1.set_app_guid(kApp1.appid);
  app1.set_update(enterprise_management::AUTOMATIC_UPDATES_ONLY);
  omaha_settings.mutable_application_settings()->Add(std::move(app1));
  ApplicationSettings app2;
  app2.set_app_guid(kApp2.appid);
  app2.set_update(enterprise_management::MANUAL_UPDATES_ONLY);
  omaha_settings.mutable_application_settings()->Add(std::move(app2));
  ExpectDeviceManagementPolicyFetchRequestFromDefaultPolicyAgent(
      test_server_.get(), kDMToken, omaha_settings);

  ExpectAppsUpdateSequence(
      UpdaterScope::kSystem, test_server_.get(),
      /*request_attributes=*/{},
      {
          AppUpdateExpectation(
              kApp1.GetInstallCommandLineArgs(/*install_v1=*/false),
              kApp1.appid, kApp1.v1, kApp1.v2,
              /*is_install=*/false,
              /*should_update=*/true, false, "", "",
              GetInstallerPath(kApp1.v2_crx)),
          AppUpdateExpectation(
              kApp2.GetInstallCommandLineArgs(/*install_v1=*/false),
              kApp2.appid, kApp2.v1, kApp2.v2,
              /*is_install=*/false,
              /*should_update=*/true, false, "", "",
              GetInstallerPath(kApp2.v2_crx)),
          AppUpdateExpectation(
              kApp3.GetInstallCommandLineArgs(/*install_v1=*/false),
              kApp3.appid, kApp3.v1, kApp3.v2,
              /*is_install=*/false,
              /*should_update=*/false, false, "", "",
              GetInstallerPath(kApp3.v2_crx)),
      });
  ASSERT_NO_FATAL_FAILURE(RunWake(0));
  ASSERT_TRUE(WaitForUpdaterExit());
  ASSERT_NO_FATAL_FAILURE(ExpectAppInstalled(kApp1.appid, kApp1.v2));
  ASSERT_NO_FATAL_FAILURE(ExpectAppInstalled(kApp2.appid, kApp2.v2));
  ASSERT_NO_FATAL_FAILURE(ExpectAppInstalled(kApp3.appid, kApp3.v1));
  ASSERT_NO_FATAL_FAILURE(ExpectUninstallPing(test_server_.get()));
  ASSERT_NO_FATAL_FAILURE(UninstallApp(kApp1.appid));
  ASSERT_NO_FATAL_FAILURE(UninstallApp(kApp2.appid));
  ASSERT_NO_FATAL_FAILURE(UninstallApp(kApp3.appid));
  ASSERT_NO_FATAL_FAILURE(Uninstall());
}

TEST_F(IntegrationTestDeviceManagement, IPolicyStatus) {
  ASSERT_NO_FATAL_FAILURE(Install());
  ASSERT_NO_FATAL_FAILURE(ExpectInstalled());
  ASSERT_NO_FATAL_FAILURE(InstallTestApp(kApp1, /*install_v1=*/true));

  base::Value::Dict policies;
  policies.Set(kApp2.appid, base::Value::Dict().Set("Update", kPolicyEnabled));
  ASSERT_NO_FATAL_FAILURE(SetPlatformPolicies(policies));
  DMPushEnrollmentToken(kEnrollmentToken);
  ExpectDeviceManagementRegistrationRequestFromDefaultPolicyAgent(
      test_server_.get(), kEnrollmentToken, kDMToken);
  OmahaSettingsClientProto omaha_settings;
  omaha_settings.set_download_preference("cacheable");
  omaha_settings.set_update_default(enterprise_management::UPDATES_DISABLED);
  omaha_settings.set_cloud_policy_overrides_platform_policy(true);
  ApplicationSettings app1;
  app1.set_app_guid(kApp1.appid);
  app1.set_target_channel("stable");
  app1.set_update(enterprise_management::AUTOMATIC_UPDATES_ONLY);
  app1.set_rollback_to_target_version(
      enterprise_management::ROLLBACK_TO_TARGET_VERSION_ENABLED);
  app1.set_target_version_prefix("2.0.");
  omaha_settings.mutable_application_settings()->Add(std::move(app1));
  ExpectDeviceManagementPolicyFetchRequestFromDefaultPolicyAgent(
      test_server_.get(), kDMToken, omaha_settings);
  ExpectAppsUpdateSequence(
      UpdaterScope::kSystem, test_server_.get(),
      /*request_attributes=*/{},
      {
          AppUpdateExpectation(
              kApp1.GetInstallCommandLineArgs(/*install_v1=*/false),
              kApp1.appid, kApp1.v1, kApp1.v2,
              /*is_install=*/false,
              /*should_update=*/true, false, "", "",
              GetInstallerPath(kApp1.v2_crx)),
      });
  ASSERT_NO_FATAL_FAILURE(RunWake(0));
  ASSERT_TRUE(WaitForUpdaterExit());
  ASSERT_NO_FATAL_FAILURE(ExpectAppInstalled(kApp1.appid, kApp1.v2));

  {
    const bool is_system_install = IsSystemInstall(GetUpdaterScopeForTesting());
    base::win::AssertComInitialized();
    Microsoft::WRL::ComPtr<IUnknown> unknown;
    ASSERT_HRESULT_SUCCEEDED(
        ::CoCreateInstance(is_system_install ? CLSID_PolicyStatusSystemClass
                                             : CLSID_PolicyStatusUserClass,
                           nullptr, CLSCTX_ALL, IID_PPV_ARGS(&unknown)));

    const base::win::ScopedBstr app_id(base::ASCIIToWide(kApp1.appid));
    Microsoft::WRL::ComPtr<IPolicyStatus4> policy_status;
    ASSERT_TRUE(SUCCEEDED(unknown.CopyTo(is_system_install
                                             ? __uuidof(IPolicyStatus4System)
                                             : __uuidof(IPolicyStatus4User),
                                         IID_PPV_ARGS_Helper(&policy_status))));
    {
      Microsoft::WRL::ComPtr<IPolicyStatusValue> policy;
      EXPECT_HRESULT_SUCCEEDED(
          policy_status->get_downloadPreferenceGroupPolicy(&policy));
      ExpectPolicyStatusValues(policy, L"Device Management", L"cacheable",
                               VARIANT_FALSE);
    }
    {
      Microsoft::WRL::ComPtr<IPolicyStatusValue> policy;
      EXPECT_HRESULT_SUCCEEDED(
          policy_status->get_cloudPolicyOverridesPlatformPolicy(&policy));
      ExpectPolicyStatusValues(policy, L"Device Management", L"true",
                               VARIANT_FALSE);
    }
    {
      Microsoft::WRL::ComPtr<IPolicyStatusValue> policy;
      EXPECT_HRESULT_SUCCEEDED(policy_status->get_effectivePolicyForAppInstalls(
          app_id.Get(), &policy));
      ExpectPolicyStatusValues(policy, L"Default", L"1", VARIANT_FALSE);
    }
    {
      Microsoft::WRL::ComPtr<IPolicyStatusValue> policy;
      EXPECT_HRESULT_SUCCEEDED(policy_status->get_effectivePolicyForAppUpdates(
          app_id.Get(), &policy));
      ExpectPolicyStatusValues(policy, L"Device Management", L"3",
                               VARIANT_TRUE);
    }
    {
      Microsoft::WRL::ComPtr<IPolicyStatusValue> policy;
      EXPECT_HRESULT_SUCCEEDED(
          policy_status->get_targetChannel(app_id.Get(), &policy));
      ExpectPolicyStatusValues(policy, L"Device Management", L"stable",
                               VARIANT_FALSE);
    }
    {
      Microsoft::WRL::ComPtr<IPolicyStatusValue> policy;
      EXPECT_HRESULT_SUCCEEDED(
          policy_status->get_isRollbackToTargetVersionAllowed(app_id.Get(),
                                                              &policy));
      ExpectPolicyStatusValues(policy, L"Device Management", L"true",
                               VARIANT_TRUE);
    }
    {
      Microsoft::WRL::ComPtr<IPolicyStatusValue> policy;
      EXPECT_HRESULT_SUCCEEDED(
          policy_status->get_targetVersionPrefix(app_id.Get(), &policy));
      ExpectPolicyStatusValues(policy, L"Device Management", L"2.0.",
                               VARIANT_FALSE);
    }
    {
      const base::win::ScopedBstr app_id2(base::ASCIIToWide(kApp2.appid));
      Microsoft::WRL::ComPtr<IPolicyStatusValue> policy;
      EXPECT_HRESULT_SUCCEEDED(policy_status->get_effectivePolicyForAppUpdates(
          app_id2.Get(), &policy));
      ExpectPolicyStatusValues(policy, L"Device Management", L"0",
                               VARIANT_TRUE);
    }
  }
  ASSERT_TRUE(WaitForUpdaterExit());

  // Uninstall
  ASSERT_NO_FATAL_FAILURE(ExpectUninstallPing(test_server_.get()));
  ASSERT_NO_FATAL_FAILURE(UninstallApp(kApp1.appid));
  ASSERT_NO_FATAL_FAILURE(Uninstall());
}
#endif  // BUILDFLAG(IS_WIN)

class IntegrationTestCloudPolicyOverridesPlatformPolicy
    : public ::testing::WithParamInterface<bool>,
      public IntegrationTestDeviceManagement {};

TEST_P(IntegrationTestCloudPolicyOverridesPlatformPolicy, UseCloudPolicy) {
  ASSERT_NO_FATAL_FAILURE(InstallBrokenEnterpriseCompanionApp());
  ASSERT_NO_FATAL_FAILURE(Install());
  ASSERT_NO_FATAL_FAILURE(ExpectInstalled());
  ASSERT_NO_FATAL_FAILURE(InstallTestApp(kApp1, /*install_v1=*/true));
  ASSERT_NO_FATAL_FAILURE(InstallTestApp(kApp2, /*install_v1=*/true));
  ASSERT_NO_FATAL_FAILURE(InstallTestApp(kApp3, /*install_v1=*/true));

  base::Value::Dict policies;
  policies.Set(kGlobalPolicyKey, base::Value::Dict()
                                     .Set("UpdateDefault", kPolicyDisabled)
                                     .Set("DownloadPreference", "cacheable"));
  policies.Set(kApp1.appid, base::Value::Dict()
                                .Set("Update", kPolicyDisabled)
                                .Set("TargetChannel", "beta"));
  policies.Set(kApp2.appid, base::Value::Dict().Set("Update", kPolicyEnabled));
  policies.Set(kApp3.appid, base::Value::Dict()
                                .Set("Update", kPolicyEnabled)
                                .Set("TargetChannel", "canary"));
  ASSERT_NO_FATAL_FAILURE(SetPlatformPolicies(policies));

  // Overrides app1 to auto-update, app2 to manual-update with cloud policy.
  DMPushEnrollmentToken(kEnrollmentToken);
  ExpectDeviceManagementRegistrationRequest(test_server_.get(),
                                            kEnrollmentToken, kDMToken);
  OmahaSettingsClientProto omaha_settings;
  ApplicationSettings app1;
  app1.set_app_guid(kApp1.appid);
  app1.set_update(enterprise_management::AUTOMATIC_UPDATES_ONLY);
  app1.set_target_channel("beta_canary");
  omaha_settings.mutable_application_settings()->Add(std::move(app1));
  ApplicationSettings app2;
  app2.set_app_guid(kApp2.appid);
  app2.set_update(enterprise_management::MANUAL_UPDATES_ONLY);
  omaha_settings.mutable_application_settings()->Add(std::move(app2));
  if (GetParam()) {
    omaha_settings.set_cloud_policy_overrides_platform_policy(true);
  } else {
    ASSERT_NO_FATAL_FAILURE(SetCloudPolicyOverridesPlatformPolicy());
  }

  ExpectDeviceManagementPolicyFetchRequest(test_server_.get(), kDMToken,
                                           omaha_settings);
  ExpectAppsUpdateSequence(
      UpdaterScope::kSystem, test_server_.get(),
      /*request_attributes=*/base::Value::Dict().Set("dlpref", "cacheable"),
      {
          AppUpdateExpectation(
              kApp1.GetInstallCommandLineArgs(/*install_v1=*/false),
              kApp1.appid, kApp1.v1, kApp1.v2,
              /*is_install=*/false,
              /*should_update=*/true, false, "", "beta_canary",
              GetInstallerPath(kApp1.v2_crx)),
          AppUpdateExpectation(
              kApp2.GetInstallCommandLineArgs(/*install_v1=*/false),
              kApp2.appid, kApp2.v1, kApp2.v1,
              /*is_install=*/false,
              /*should_update=*/false, false, "", "",
              GetInstallerPath(kApp2.v2_crx)),
          AppUpdateExpectation(
              kApp3.GetInstallCommandLineArgs(/*install_v1=*/false),
              kApp3.appid, kApp3.v1, kApp3.v2,
              /*is_install=*/false,
              /*should_update=*/true, false, "", "canary",
              GetInstallerPath(kApp3.v2_crx)),
      });
  ASSERT_NO_FATAL_FAILURE(RunWake(0));
  ASSERT_TRUE(WaitForUpdaterExit());
  ASSERT_NO_FATAL_FAILURE(ExpectAppInstalled(kApp1.appid, kApp1.v2));
  ASSERT_NO_FATAL_FAILURE(ExpectAppInstalled(kApp2.appid, kApp2.v1));
  ASSERT_NO_FATAL_FAILURE(ExpectAppInstalled(kApp3.appid, kApp3.v2));
  ASSERT_NO_FATAL_FAILURE(ExpectUninstallPing(test_server_.get()));
  ASSERT_NO_FATAL_FAILURE(UninstallApp(kApp1.appid));
  ASSERT_NO_FATAL_FAILURE(UninstallApp(kApp2.appid));
  ASSERT_NO_FATAL_FAILURE(UninstallApp(kApp3.appid));
  ASSERT_NO_FATAL_FAILURE(UninstallBrokenEnterpriseCompanionApp());
  ASSERT_NO_FATAL_FAILURE(Uninstall());
}

INSTANTIATE_TEST_SUITE_P(
    IntegrationTestCloudPolicyOverridesPlatformPolicyTestCases,
    IntegrationTestCloudPolicyOverridesPlatformPolicy,
    ::testing::Bool());

TEST_F(IntegrationTestDeviceManagement, RollbackToTargetVersion) {
  constexpr char kTargetVersionPrefix[] = "1.0.";

  ASSERT_NO_FATAL_FAILURE(Install());
  ASSERT_NO_FATAL_FAILURE(InstallTestApp(kApp1, /*install_v1=*/false));

  ASSERT_NO_FATAL_FAILURE(ExpectInstalled());
  ASSERT_NO_FATAL_FAILURE(ExpectAppInstalled(kApp1.appid, kApp1.v2));

  DMPushEnrollmentToken(kEnrollmentToken);
  ExpectDeviceManagementRegistrationRequestFromDefaultPolicyAgent(
      test_server_.get(), kEnrollmentToken, kDMToken);
  OmahaSettingsClientProto omaha_settings;
  ApplicationSettings app;
  app.set_app_guid(kApp1.appid);
  app.set_target_version_prefix(kTargetVersionPrefix);
  app.set_rollback_to_target_version(
      enterprise_management::ROLLBACK_TO_TARGET_VERSION_ENABLED);
  omaha_settings.mutable_application_settings()->Add(std::move(app));
  ExpectDeviceManagementPolicyFetchRequestFromDefaultPolicyAgent(
      test_server_.get(), kDMToken, omaha_settings);

  ExpectAppsUpdateSequence(
      UpdaterScope::kSystem, test_server_.get(),
      /*request_attributes=*/{},
      {AppUpdateExpectation(
          kApp1.GetInstallCommandLineArgs(/*install_v1=*/true), kApp1.appid,
          kApp1.v2, kApp1.v1,
          /*is_install=*/false,
          /*should_update=*/true, /*allow_rollback=*/true, kTargetVersionPrefix,
          "", GetInstallerPath(kApp1.v1_crx))});
  ASSERT_NO_FATAL_FAILURE(RunWake(0));
  ASSERT_TRUE(WaitForUpdaterExit());
  ASSERT_NO_FATAL_FAILURE(ExpectAppInstalled(kApp1.appid, kApp1.v1));

  ASSERT_NO_FATAL_FAILURE(ExpectUninstallPing(test_server_.get()));
  ASSERT_NO_FATAL_FAILURE(UninstallApp(kApp1.appid));
  ASSERT_NO_FATAL_FAILURE(Uninstall());
}

TEST_F(IntegrationTestDeviceManagement, DMTokenDeletion) {
  ASSERT_NO_FATAL_FAILURE(InstallBrokenEnterpriseCompanionApp());
  ASSERT_NO_FATAL_FAILURE(Install());
  ASSERT_NO_FATAL_FAILURE(InstallTestApp(kApp1, /*install_v1=*/true));
  ASSERT_NO_FATAL_FAILURE(ExpectInstalled());
  ASSERT_NO_FATAL_FAILURE(ExpectAppInstalled(kApp1.appid, kApp1.v1));

  // Do a policy fetch to update the DM token.
  DMPushEnrollmentToken(kEnrollmentToken);
  ExpectDeviceManagementRegistrationRequest(test_server_.get(),
                                            kEnrollmentToken, kDMToken);
  ExpectDeviceManagementPolicyFetchRequest(test_server_.get(), kDMToken, {});
  ASSERT_NO_FATAL_FAILURE(
      ExpectNoUpdateSequence(test_server_.get(), kApp1.appid));
  ASSERT_NO_FATAL_FAILURE(RunWake(0));
  ASSERT_TRUE(WaitForUpdaterExit());
  EXPECT_EQ(device_management_storage::GetDefaultDMStorage()->GetDmToken(),
            kDMToken);

  // Run a second policy fetch and delete the DM token.
  ExpectDeviceManagementTokenDeletionRequest(test_server_.get(), kDMToken,
                                             /*invalidate_token=*/false);
#if BUILDFLAG(IS_MAC)
  // A second response for fallback fetcher.
  ExpectDeviceManagementTokenDeletionRequest(test_server_.get(), kDMToken,
                                             /*invalidate_token=*/false);
#endif
  ASSERT_NO_FATAL_FAILURE(RunWake(0));
  ASSERT_TRUE(WaitForUpdaterExit());
  EXPECT_TRUE(
      device_management_storage::GetDefaultDMStorage()->GetDmToken().empty());

  ASSERT_NO_FATAL_FAILURE(ExpectUninstallPing(test_server_.get()));
  ASSERT_NO_FATAL_FAILURE(UninstallApp(kApp1.appid));
  ASSERT_NO_FATAL_FAILURE(UninstallBrokenEnterpriseCompanionApp());
  ASSERT_NO_FATAL_FAILURE(Uninstall());
}

TEST_F(IntegrationTestDeviceManagement, DMTokenInvalidation) {
  ASSERT_NO_FATAL_FAILURE(InstallBrokenEnterpriseCompanionApp());
  ASSERT_NO_FATAL_FAILURE(Install());
  ASSERT_NO_FATAL_FAILURE(InstallTestApp(kApp1, /*install_v1=*/true));
  ASSERT_NO_FATAL_FAILURE(ExpectInstalled());
  ASSERT_NO_FATAL_FAILURE(ExpectAppInstalled(kApp1.appid, kApp1.v1));

  // Do a policy fetch to update the DM token.
  DMPushEnrollmentToken(kEnrollmentToken);
  ExpectDeviceManagementRegistrationRequest(test_server_.get(),
                                            kEnrollmentToken, kDMToken);
  ExpectDeviceManagementPolicyFetchRequest(test_server_.get(), kDMToken, {});
  ASSERT_NO_FATAL_FAILURE(
      ExpectNoUpdateSequence(test_server_.get(), kApp1.appid));
  ASSERT_NO_FATAL_FAILURE(RunWake(0));
  ASSERT_TRUE(WaitForUpdaterExit());
  EXPECT_EQ(device_management_storage::GetDefaultDMStorage()->GetDmToken(),
            kDMToken);

  // Run a second policy fetch and invalidate the DM token.
  ExpectDeviceManagementTokenDeletionRequest(test_server_.get(), kDMToken,
                                             /*invalidate_token=*/true);
#if BUILDFLAG(IS_MAC)
  // A second response for fallback fetcher.
  ExpectDeviceManagementTokenDeletionRequest(test_server_.get(), kDMToken,
                                             /*invalidate_token=*/true);
#endif  // BUILDFLAG(IS_MAC)
  ASSERT_NO_FATAL_FAILURE(RunWake(0));
  ASSERT_TRUE(WaitForUpdaterExit());
  EXPECT_TRUE(
      device_management_storage::GetDefaultDMStorage()->IsDeviceDeregistered());

  ASSERT_NO_FATAL_FAILURE(ExpectUninstallPing(test_server_.get()));
  ASSERT_NO_FATAL_FAILURE(UninstallApp(kApp1.appid));
  ASSERT_NO_FATAL_FAILURE(UninstallBrokenEnterpriseCompanionApp());
  ASSERT_NO_FATAL_FAILURE(Uninstall());
}

TEST_F(IntegrationTestDeviceManagement, PublicKeyRotation) {
  ASSERT_NO_FATAL_FAILURE(InstallBrokenEnterpriseCompanionApp());
  ASSERT_NO_FATAL_FAILURE(Install());
  ASSERT_NO_FATAL_FAILURE(InstallTestApp(kApp1, /*install_v1=*/false));

  ASSERT_NO_FATAL_FAILURE(ExpectInstalled());
  ASSERT_NO_FATAL_FAILURE(ExpectAppInstalled(kApp1.appid, kApp1.v2));

  // Do a policy fetch to download the policy info with the initial public key.
  DMPushEnrollmentToken(kEnrollmentToken);
  ExpectDeviceManagementRegistrationRequest(test_server_.get(),
                                            kEnrollmentToken, kDMToken);
  OmahaSettingsClientProto omaha_settings;
  ExpectDeviceManagementPolicyFetchRequest(test_server_.get(), kDMToken,
                                           omaha_settings);
  ASSERT_NO_FATAL_FAILURE(
      ExpectNoUpdateSequence(test_server_.get(), kApp1.appid));
  ASSERT_NO_FATAL_FAILURE(RunWake(0));
  ASSERT_TRUE(WaitForUpdaterExit());

  scoped_refptr<device_management_storage::DMStorage> dm_storage =
      device_management_storage::GetDefaultDMStorage();
  std::unique_ptr<device_management_storage::CachedPolicyInfo> cached_info =
      dm_storage->GetCachedPolicyInfo();
  ASSERT_NE(cached_info, nullptr);
  int64_t initial_key_timestamp = cached_info->timestamp();

  // Run a second policy fetch and verify the cached policy info is updated
  // to the one with the new public key.
  ApplicationSettings app;
  app.set_app_guid(kApp1.appid);
  app.set_target_version_prefix("1.0");
  app.set_rollback_to_target_version(
      enterprise_management::ROLLBACK_TO_TARGET_VERSION_ENABLED);
  omaha_settings.mutable_application_settings()->Add(std::move(app));
  ExpectDeviceManagementPolicyFetchWithNewPublicKeyRequest(
      test_server_.get(), kDMToken, omaha_settings);
  ASSERT_NO_FATAL_FAILURE(RunWake(0));
  ASSERT_TRUE(WaitForUpdaterExit());
  cached_info = dm_storage->GetCachedPolicyInfo();
  ASSERT_NE(cached_info, nullptr);
  ASSERT_GT(cached_info->timestamp(), initial_key_timestamp);

  ASSERT_NO_FATAL_FAILURE(ExpectUninstallPing(test_server_.get()));
  ASSERT_NO_FATAL_FAILURE(UninstallApp(kApp1.appid));
  ASSERT_NO_FATAL_FAILURE(UninstallBrokenEnterpriseCompanionApp());
  ASSERT_NO_FATAL_FAILURE(Uninstall());
}

// Tests that interact with state in both system and user updater configuration
// are run as part of the system-scope tests.
class IntegrationTestUserInSystem : public IntegrationTest {
 public:
  ~IntegrationTestUserInSystem() override = default;

 protected:
  void SetUp() override {
    if (SkipTest()) {
      GTEST_SKIP() << "The test is skipped in this configuration";
    }

    IntegrationTest::SetUp();
    if (IsSkipped()) {
      return;
    }
    test_server_ = std::make_unique<ScopedServer>();
    test_server_->ConfigureTestMode(user_test_commands_.get());
    test_server_->ConfigureTestMode(test_commands_.get());
  }

  void TearDown() override {
    if (!SkipTest()) {
      IntegrationTest::TearDown();
    }
  }

  void InstallUserUpdater(const base::Value::List& switches = {}) {
    user_test_commands_->Install(switches);
  }

  void UninstallUserUpdater() {
    ASSERT_TRUE(WaitForUpdaterExit());
    ExpectNoCrashes();
    PrintUserLog();
    CopyUserLog();
    user_test_commands_->Uninstall();
    ASSERT_TRUE(WaitForUpdaterExit());
  }

  void ExpectUserUpdaterInstalled() { user_test_commands_->ExpectInstalled(); }

  void InstallUserApp(const std::string& app_id, const base::Version& version) {
    user_test_commands_->InstallApp(app_id, version);
  }

  void ExpectUserAppVersion(const std::string& app_id,
                            const base::Version& version) {
    user_test_commands_->ExpectAppVersion(app_id, version);
  }

  void SetUserAppExistenceCheckerPath(const std::string& app_id,
                                      const base::FilePath& path) {
    user_test_commands_->SetExistenceCheckerPath(app_id, path);
  }

  void SetUserAppTag(const std::string& app_id, const std::string& tag) {
    user_test_commands_->SetAppTag(app_id, tag);
  }

  void ExpectUserAppTag(const std::string& app_id, const std::string& tag) {
    user_test_commands_->ExpectAppTag(app_id, tag);
  }

  void PrintUserLog() { user_test_commands_->PrintLog(); }

  void CopyUserLog() { user_test_commands_->CopyLog("user"); }

  void ExpectUserUninstallPing(ScopedServer* test_server,
                               std::optional<GURL> target_url = {}) {
    user_test_commands_->ExpectPing(
        test_server, update_client::protocol_request::kEventUninstall,
        target_url);
  }

  void ExpectUserInstallSequence(ScopedServer* test_server,
                                 const std::string& app_id,
                                 const std::string& install_data_index,
                                 UpdateService::Priority priority,
                                 const base::Version& from_version,
                                 const base::Version& to_version) {
    user_test_commands_->ExpectInstallSequence(test_server, app_id,
                                               install_data_index, priority,
                                               from_version, to_version,
                                               /*do_fault_injection=*/false);
  }

  void InstallUserUpdaterAndApp(
      const std::string& app_id,
      const bool is_silent_install,
      const std::string& tag,
      const std::string& child_window_text_to_find = {},
      const bool always_launch_cmd = false,
      const bool verify_app_logo_loaded = false) {
    user_test_commands_->InstallUpdaterAndApp(
        app_id, is_silent_install, tag, child_window_text_to_find,
        always_launch_cmd, verify_app_logo_loaded,
        /*expect_success=*/true, /*wait_for_the_installer=*/true);
  }

  scoped_refptr<IntegrationTestCommands> user_test_commands_ =
      CreateIntegrationTestCommandsUser(UpdaterScope::kUser);
  std::unique_ptr<ScopedServer> test_server_;

 private:
  // Even though the updater itself supports installing per-user applications at
  // high integrity, most of the tests in the `IntegrationTestUserInSystem` test
  // suite cannot run on Windows with UAC on, because the integration test
  // driver does not fully support installing per-user applications at high
  // integrity. For instance, it functions as a COM client running at high
  // integrity to create the user updater COM server, which is not supported on
  // Windows with UAC on.
  bool SkipTest() const {
    return !IsSystemInstall(GetUpdaterScopeForTesting()) ||
           (WrongUser(UpdaterScope::kUser) &&
            (GetTestName() !=
             "IntegrationTestUserInSystem.ElevatedInstallOfUserUpdaterAndApp"));
  }
};

// Tests the updater's functionality of installing per-user applications at high
// integrity. This test uses integration test driver APIs that support
// installing per-user applications at high integrity. For instance, it runs
// `UpdaterSetup --install --app-id=test` and `UpdaterSetup --uninstall`
// elevated via the command line, so that it directly uses the updater's
// functionality of de-elevating.
TEST_F(IntegrationTestUserInSystem, ElevatedInstallOfUserUpdaterAndApp) {
  const std::string kAppId("test");
  const base::Version v1("1");
  ASSERT_NO_FATAL_FAILURE(ExpectUserInstallSequence(
      test_server_.get(), kAppId, "", UpdateService::Priority::kForeground,
      base::Version({0, 0, 0, 0}), v1));

  ASSERT_NO_FATAL_FAILURE(InstallUserUpdaterAndApp(
      kAppId, /*is_silent_install=*/true, "usagestats=1"));
  ASSERT_TRUE(WaitForUpdaterExit());

  ASSERT_NO_FATAL_FAILURE(ExpectUserAppVersion(kAppId, v1));

  ASSERT_NO_FATAL_FAILURE(ExpectUserUninstallPing(test_server_.get()));
  ASSERT_NO_FATAL_FAILURE(UninstallUserUpdater());
}

TEST_F(IntegrationTestUserInSystem, TagNonInterference) {
  ASSERT_NO_FATAL_FAILURE(InstallUserUpdater());
  ASSERT_NO_FATAL_FAILURE(Install());
  ASSERT_NO_FATAL_FAILURE(ExpectInstalled());
  ASSERT_NO_FATAL_FAILURE(ExpectUserUpdaterInstalled());

  base::Version v("1.0.0.0");
  ASSERT_NO_FATAL_FAILURE(InstallApp("test_app", v));
  ExpectAppVersion("test_app", v);
  ExpectAppTag("test_app", "");
  ASSERT_NO_FATAL_FAILURE(InstallUserApp("test_app", v));
  ExpectUserAppVersion("test_app", v);
  ExpectUserAppTag("test_app", "");

  ASSERT_NO_FATAL_FAILURE(SetAppTag("test_app", "system"));
  ExpectAppTag("test_app", "system");
  ExpectUserAppTag("test_app", "");
  ASSERT_NO_FATAL_FAILURE(SetUserAppTag("test_app", "user"));
  ExpectUserAppTag("test_app", "user");
  ExpectAppTag("test_app", "system");

  ExpectUninstallPing(test_server_.get());
  Uninstall();
  ExpectUserUninstallPing(test_server_.get());
  UninstallUserUpdater();
}

// macOS specific tests.
#if BUILDFLAG(IS_MAC)

// The CRURegistration library exists only on macOS. It runs ksadmin. It should
// not find ksadmin before the updater is installed or after it is uninstalled,
// but should find the scope-suitable ksadmin while the updater is installed.
TEST_F(IntegrationTest, CRURegistrationFindKSAdmin) {
  EXPECT_NO_FATAL_FAILURE(ExpectCRURegistrationCannotFindKSAdmin())
      << "ksadmin found before first installation.";
  ASSERT_NO_FATAL_FAILURE(Install());
  ASSERT_TRUE(WaitForUpdaterExit());
  EXPECT_NO_FATAL_FAILURE(
      ExpectCRURegistrationFindsKSAdmin(GetUpdaterScopeForTesting()));
  ASSERT_NO_FATAL_FAILURE(Uninstall());
  EXPECT_NO_FATAL_FAILURE(ExpectCRURegistrationCannotFindKSAdmin())
      << "ksadmin found after uninstall.";
}

TEST_F(IntegrationTest, CRURegistrationCannotGetTagWithoutUpdater) {
  base::ScopedTempFile xc_path;
  ASSERT_TRUE(xc_path.Create());
  EXPECT_NO_FATAL_FAILURE(
      ExpectCRURegistrationCannotFetchTag(kApp1.appid, xc_path.path()));
}

TEST_F(IntegrationTest, CRURegistrationCannotGetTagWithoutApp) {
  ASSERT_NO_FATAL_FAILURE(Install());
  ASSERT_TRUE(WaitForUpdaterExit());

  base::ScopedTempFile xc_path;
  ASSERT_TRUE(xc_path.Create());
  EXPECT_NO_FATAL_FAILURE(
      ExpectCRURegistrationCannotFetchTag(kApp1.appid, xc_path.path()));
  ASSERT_NO_FATAL_FAILURE(Uninstall());
}

#if !defined(ADDRESS_SANITIZER)
TEST_F(IntegrationTest, CRURegistrationFindsBlankTag) {
  ASSERT_NO_FATAL_FAILURE(Install());
  ASSERT_TRUE(WaitForUpdaterExit());

  base::ScopedTempFile xc_path;
  ASSERT_TRUE(xc_path.Create());
  ASSERT_NO_FATAL_FAILURE(InstallApp(kApp1.appid));
  ASSERT_NO_FATAL_FAILURE(SetExistenceCheckerPath(kApp1.appid, xc_path.path()));

  EXPECT_NO_FATAL_FAILURE(
      ExpectCRURegistrationFetchesTag(kApp1.appid, xc_path.path(), ""));
  ASSERT_NO_FATAL_FAILURE(Uninstall());
}

TEST_F(IntegrationTest, CRURegistrationFindsTag) {
  ScopedServer test_server(test_commands_);
  const std::string kAppId("test");
  const base::Version v1("1");
  base::ScopedTempFile xc_path;
  ASSERT_TRUE(xc_path.Create());

  ASSERT_NO_FATAL_FAILURE(ExpectInstallSequence(
      &test_server, kAppId, "", UpdateService::Priority::kForeground,
      base::Version({0, 0, 0, 0}), v1));
  ASSERT_NO_FATAL_FAILURE(InstallUpdaterAndApp(
      kAppId, /*is_silent_install=*/true,
      base::StrCat({"appguid=", kAppId, "&ap=tagvalue&usagestats=1"})));
  ASSERT_TRUE(WaitForUpdaterExit());
  ASSERT_NO_FATAL_FAILURE(SetExistenceCheckerPath(kAppId, xc_path.path()));

  EXPECT_NO_FATAL_FAILURE(
      ExpectCRURegistrationFetchesTag(kAppId, xc_path.path(), "tagvalue"));

  ASSERT_NO_FATAL_FAILURE(ExpectUninstallPing(&test_server));
  ASSERT_NO_FATAL_FAILURE(Uninstall());
}
#endif  // !defined(ADDRESS_SANITIZER)

// App ownership feature only exists on macOS.
TEST_F(IntegrationTest, UnregisterUnownedApp) {
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  ASSERT_NO_FATAL_FAILURE(Install());
  ASSERT_NO_FATAL_FAILURE(ExpectInstalled());
  ASSERT_NO_FATAL_FAILURE(ExpectVersionActive(kUpdaterVersion));

  ASSERT_NO_FATAL_FAILURE(InstallApp("test1"));
  ASSERT_NO_FATAL_FAILURE(InstallApp("test2"));
  ASSERT_TRUE(WaitForUpdaterExit());

  ASSERT_NO_FATAL_FAILURE(SetExistenceCheckerPath(
      "test1", IsSystemInstall(GetUpdaterScopeForTesting())
                   ? temp_dir.GetPath()
                   : GetDifferentUserPath()));

  ASSERT_NO_FATAL_FAILURE(RunWake(0));
  ASSERT_TRUE(WaitForUpdaterExit());

  // Since the updater may have chowned the temp dir, we may need to elevate to
  // delete it.
  ASSERT_NO_FATAL_FAILURE(DeleteFile(temp_dir.GetPath()));

  if (IsSystemInstall(GetUpdaterScopeForTesting())) {
    ASSERT_NO_FATAL_FAILURE(ExpectRegistered("test1"));
  } else {
    ASSERT_NO_FATAL_FAILURE(ExpectNotRegistered("test1"));
  }

  ASSERT_NO_FATAL_FAILURE(ExpectRegistered("test2"));

  ASSERT_NO_FATAL_FAILURE(Uninstall());
}

// The updater shims are only repaired by the server on macOS.
TEST_F(IntegrationTest, RepairUpdater) {
  ASSERT_NO_FATAL_FAILURE(Install());
  ASSERT_TRUE(WaitForUpdaterExit());
  ASSERT_NO_FATAL_FAILURE(DeleteLegacyUpdater());
  std::optional<base::FilePath> ksadmin_path =
      GetKSAdminPath(GetUpdaterScopeForTesting());
  ASSERT_TRUE(ksadmin_path.has_value());
  ASSERT_FALSE(base::PathExists(*ksadmin_path));
  ASSERT_NO_FATAL_FAILURE(RunWake(0));
  ASSERT_TRUE(WaitForUpdaterExit());
  ASSERT_TRUE(base::PathExists(*ksadmin_path));
  ASSERT_NO_FATAL_FAILURE(ExpectInstalled());
  ASSERT_NO_FATAL_FAILURE(Uninstall());
}

// Only macOS software needs to try to suppress user-visible Gatekeeper popups.
TEST_F(IntegrationTest, SmokeTestPrepareToRunBundle) {
  ASSERT_NO_FATAL_FAILURE(Install());
  ASSERT_NO_FATAL_FAILURE(ExpectInstalled());
  ASSERT_NO_FATAL_FAILURE(ExpectVersionActive(kUpdaterVersion));
  ASSERT_TRUE(WaitForUpdaterExit());

  std::optional<base::FilePath> updater_path =
      GetUpdaterAppBundlePath(GetUpdaterScopeForTesting());
  ASSERT_TRUE(updater_path);
  ASSERT_NO_FATAL_FAILURE(ExpectPrepareToRunBundleSuccess(*updater_path));

  ASSERT_NO_FATAL_FAILURE(Uninstall());
}

// The privileged helper only exists on macOS. This does not test installation
// of the helper itself, but is meant to cover its core functionality.
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
TEST_F(IntegrationTest, PrivilegedHelperInstall) {
  if (GetUpdaterScopeForTesting() != UpdaterScope::kSystem) {
    return;  // Test is only applicable to system scope.
  }
  ASSERT_NO_FATAL_FAILURE(PrivilegedHelperInstall());
  ASSERT_TRUE(WaitForUpdaterExit());
  ASSERT_NO_FATAL_FAILURE(ExpectRegistered("test1"));
  ASSERT_NO_FATAL_FAILURE(ExpectAppVersion("test1", base::Version("1.2.3.4")));
  ASSERT_NO_FATAL_FAILURE(Uninstall());
}
#endif  // BUILDFLAG(GOOGLE_CHROME_BRANDING)

TEST_F(IntegrationTest, FallbackToOutOfProcessFetcher) {
  const std::string kAppId1("test1");
  const base::Version v1("1");
  // Injects an HTTP error before each network fetch to activate the fallback
  // fetcher. The installation should still succeed.
  ScopedServer test_server(test_commands_);
  ASSERT_NO_FATAL_FAILURE(ExpectInstallSequence(
      &test_server, kAppId1, "", UpdateService::Priority::kForeground,
      base::Version({0, 0, 0, 0}), v1, /*do_fault_injection=*/true));
  ASSERT_NO_FATAL_FAILURE(InstallUpdaterAndApp(
      kAppId1, /*is_silent_install=*/true,
      base::StrCat({"appguid=", kAppId1, "&ap=foo&usagestats=1"})));
  ASSERT_TRUE(WaitForUpdaterExit());
  ASSERT_NO_FATAL_FAILURE(ExpectAppVersion(kAppId1, v1));
  ASSERT_NO_FATAL_FAILURE(ExpectAppTag(kAppId1, "foo"));

  const std::string kAppId2("test2");
  const base::Version v2("2.0");
  // Consecutive HTTP errors should fail the installation, given the fact that
  // updater has only one fallback for each network task.
  test_server.ExpectOnce({}, "", net::HTTP_INTERNAL_SERVER_ERROR);
  test_server.ExpectOnce({}, "", net::HTTP_GONE);
  ASSERT_NO_FATAL_FAILURE(InstallUpdaterAndApp(
      kAppId2, /*is_silent_install=*/true,
      base::StrCat({"appguid=", kAppId2, "&ap=foo2&usagestats=1"}),
      /*child_window_text_to_find=*/{}, /*always_launch_cmd=*/false,
      /*verify_app_logo_loaded=*/false,
      /*expect_success=*/false));
  ASSERT_TRUE(WaitForUpdaterExit());
  ASSERT_NO_FATAL_FAILURE(ExpectAppVersion(kAppId2, base::Version()));
  ASSERT_NO_FATAL_FAILURE(ExpectAppTag(kAppId2, ""));
  ASSERT_NO_FATAL_FAILURE(ExpectUninstallPing(&test_server));
  ASSERT_NO_FATAL_FAILURE(Uninstall());
}

TEST_F(IntegrationTest, KSAdminNoAppNoTag) {
#if defined(ADDRESS_SANITIZER)
  if (IsSystemInstall(GetUpdaterScopeForTesting())) {
    GTEST_SKIP() << "User->System launcher can't load macOS ASAN dylib.";
    // Actually, since this test expects ksadmin to fail, it passes under these
    // conditions, but for the wrong reason.
  }
#else
  ASSERT_NO_FATAL_FAILURE(Install());
  ASSERT_TRUE(WaitForUpdaterExit());
  ExpectKSAdminFetchTag(false, "no.such.app", {}, {}, {});
  ASSERT_NO_FATAL_FAILURE(Uninstall());
#endif  // defined(ADDRESS_SANITIZER)
}

TEST_F(IntegrationTest, KSAdminUntaggedApp) {
#if defined(ADDRESS_SANITIZER)
  if (IsSystemInstall(GetUpdaterScopeForTesting())) {
    GTEST_SKIP() << "User->System launcher can't load macOS ASAN dylib.";
  }
#else
  ASSERT_NO_FATAL_FAILURE(Install());
  ASSERT_TRUE(WaitForUpdaterExit());
  ASSERT_NO_FATAL_FAILURE(InstallApp("org.chromium.testapp"));
  ExpectKSAdminFetchTag(false, "org.chromium.testapp", {}, {}, "");
  ASSERT_NO_FATAL_FAILURE(UninstallApp("org.chromium.testapp"));
  ASSERT_NO_FATAL_FAILURE(Uninstall());
#endif  // defined(ADDRESS_SANITIZER)
}

TEST_F(IntegrationTest, KSAdminTaggedApp) {
#if defined(ADDRESS_SANITIZER)
  if (IsSystemInstall(GetUpdaterScopeForTesting())) {
    GTEST_SKIP() << "User->System launcher can't load macOS ASAN dylib.";
  }
#else
  ASSERT_NO_FATAL_FAILURE(Install());
  ASSERT_TRUE(WaitForUpdaterExit());
  ASSERT_NO_FATAL_FAILURE(InstallApp("org.chromium.testapp"));
  ASSERT_NO_FATAL_FAILURE(SetAppTag("org.chromium.testapp", "some-tag"));
  ExpectKSAdminFetchTag(false, "org.chromium.testapp", {}, {}, "some-tag");
  ASSERT_NO_FATAL_FAILURE(UninstallApp("org.chromium.testapp"));
  ASSERT_NO_FATAL_FAILURE(Uninstall());
#endif  // defined(ADDRESS_SANITIZER)
}

TEST_F(IntegrationTest, CRURegistrationInstallsUpdater) {
  if (IsSystemInstall(GetUpdaterScopeForTesting())) {
    GTEST_SKIP();
  }
  ScopedServer test_server(test_commands_);
  ASSERT_NO_FATAL_FAILURE(ExpectRegistrationTestAppUserUpdaterInstallSuccess());
  ASSERT_NO_FATAL_FAILURE(ExpectInstalled());

  ExpectUninstallPing(&test_server);
  ASSERT_NO_FATAL_FAILURE(Uninstall());
}

TEST_F(IntegrationTest, CRURegistrationIdempotentInstallSuccess) {
  if (IsSystemInstall(GetUpdaterScopeForTesting())) {
    GTEST_SKIP();
  }
  ScopedServer test_server(test_commands_);
  ASSERT_NO_FATAL_FAILURE(Install());
  ASSERT_NO_FATAL_FAILURE(ExpectInstalled());

  ASSERT_NO_FATAL_FAILURE(ExpectRegistrationTestAppUserUpdaterInstallSuccess());
  ExpectInstalled();

  ExpectUninstallPing(&test_server);
  ASSERT_NO_FATAL_FAILURE(Uninstall());
}

TEST_F(IntegrationTest, CRURegistrationRegister) {
  if (IsSystemInstall(GetUpdaterScopeForTesting())) {
    GTEST_SKIP();
  }
  ScopedServer test_server(test_commands_);
  ASSERT_NO_FATAL_FAILURE(Install());
  ASSERT_NO_FATAL_FAILURE(ExpectInstalled());

  ASSERT_NO_FATAL_FAILURE(ExpectRegistrationTestAppRegisterSuccess());
  ExpectAppVersion("org.chromium.CRURegistration.testing.RegisterMe",
                   base::Version({1, 0, 0, 0}));

  ExpectUninstallPing(&test_server);
  ASSERT_NO_FATAL_FAILURE(Uninstall());
}

TEST_F(IntegrationTest, CRURegistrationInstallAndRegister) {
  if (IsSystemInstall(GetUpdaterScopeForTesting())) {
    GTEST_SKIP();
  }
  ScopedServer test_server(test_commands_);
  ASSERT_NO_FATAL_FAILURE(ExpectRegistrationTestAppInstallAndRegisterSuccess());
  ExpectInstalled();
  ExpectAppVersion("org.chromium.CRURegistration.testing.RegisterMe",
                   base::Version({2, 0, 0, 0}));

  ExpectUninstallPing(&test_server);
  ASSERT_NO_FATAL_FAILURE(Uninstall());
}

// This is a copy of ReportsActive, but it uses CRURegistration to mark the
// app active. If both this test and ReportsActive fail, suspect an issue with
// actives reporting; if only this test fails, suspect CRURegistration.
TEST_F(IntegrationTest, CRURegistrationReportsActive) {
  if (IsSystemInstall(GetUpdaterScopeForTesting())) {
    GTEST_SKIP();
  }
  // A longer than usual timeout is needed for this test because the macOS
  // UpdateServiceInternal server takes at least 10 seconds to shut down after
  // Install, and InstallApp cannot make progress until it shut downs and
  // releases the global prefs lock.
  ASSERT_GE(TestTimeouts::action_timeout(), base::Seconds(18));
  base::test::ScopedRunLoopTimeout timeout(FROM_HERE,
                                           TestTimeouts::action_timeout());

  ScopedServer test_server(test_commands_);
  ASSERT_NO_FATAL_FAILURE(Install());
  ASSERT_NO_FATAL_FAILURE(ExpectInstalled());

  // Register apps test1 and test2. Expect pings for each.
  ASSERT_NO_FATAL_FAILURE(InstallApp("test1"));
  ASSERT_NO_FATAL_FAILURE(InstallApp("test2"));

  // Set test1 to be active via CRURegistration and do a background updatecheck.
  ASSERT_NO_FATAL_FAILURE(ExpectCRURegistrationMarksActive("test1"));
  ASSERT_NO_FATAL_FAILURE(ExpectActive("test1"));
  ASSERT_NO_FATAL_FAILURE(ExpectNotActive("test2"));
  test_server.ExpectOnce(
      {request::GetUpdaterUserAgentMatcher(),
       request::GetContentMatcher(
           {R"(.*"appid":"test1","enabled":true,"installdate":-1,)",
            R"("ping":{"ad":-1,.*)"})},
      R"()]}')"
      "\n"
      R"({"response":{"protocol":"3.1","daystart":{"elapsed_)"
      R"(days":5098}},"app":[{"appid":"test1","status":"ok",)"
      R"("updatecheck":{"status":"noupdate"}},{"appid":"test2",)"
      R"("status":"ok","updatecheck":{"status":"noupdate"}}]})");
  ASSERT_NO_FATAL_FAILURE(RunWake(0));

  // The updater has cleared the active bits.
  ASSERT_NO_FATAL_FAILURE(ExpectNotActive("test1"));
  ASSERT_NO_FATAL_FAILURE(ExpectNotActive("test2"));

  ASSERT_NO_FATAL_FAILURE(ExpectUninstallPing(&test_server));
  ASSERT_NO_FATAL_FAILURE(Uninstall());
}

#if !defined(ADDRESS_SANITIZER)

TEST_F(IntegrationTestUserInSystem, CRURegistrationRegistersApp) {
  ASSERT_NO_FATAL_FAILURE(Install());
  ASSERT_NO_FATAL_FAILURE(InstallUserUpdater());
  ASSERT_TRUE(WaitForUpdaterExit());
  ASSERT_NO_FATAL_FAILURE(ExpectInstalled());
  ASSERT_NO_FATAL_FAILURE(ExpectUserUpdaterInstalled());
  base::ScopedTempFile xc_file;
  ASSERT_TRUE(xc_file.Create());

  ExpectCRURegistrationRegisters("test", xc_file.path(), "0.0.0.1");
  ExpectUserAppVersion("test", base::Version({0, 0, 0, 1}));
  ExpectNotRegistered("test");

  ExpectUserUninstallPing(test_server_.get());
  ASSERT_NO_FATAL_FAILURE(UninstallUserUpdater());
  ExpectUninstallPing(test_server_.get());
  ASSERT_NO_FATAL_FAILURE(Uninstall());
}

TEST_F(IntegrationTestUserInSystem, CRURegistrationUpdatesVersion) {
  ASSERT_NO_FATAL_FAILURE(Install());
  ASSERT_NO_FATAL_FAILURE(InstallUserUpdater());
  ASSERT_TRUE(WaitForUpdaterExit());
  ASSERT_NO_FATAL_FAILURE(ExpectInstalled());
  ASSERT_NO_FATAL_FAILURE(ExpectUserUpdaterInstalled());
  base::ScopedTempFile xc_file;
  ASSERT_TRUE(xc_file.Create());

  InstallUserApp("test", base::Version({0, 0, 0, 1}));
  ExpectCRURegistrationRegisters("test", xc_file.path(), "0.0.0.2");
  ExpectUserAppVersion("test", base::Version({0, 0, 0, 2}));
  ExpectNotRegistered("test");

  ExpectUserUninstallPing(test_server_.get());
  ASSERT_NO_FATAL_FAILURE(UninstallUserUpdater());
  ExpectUninstallPing(test_server_.get());
  ASSERT_NO_FATAL_FAILURE(Uninstall());
}

TEST_F(IntegrationTestUserInSystem, CRURegistrationCannotRegisterMissingAppID) {
  ASSERT_NO_FATAL_FAILURE(Install());
  ASSERT_NO_FATAL_FAILURE(InstallUserUpdater());
  ASSERT_TRUE(WaitForUpdaterExit());
  ASSERT_NO_FATAL_FAILURE(ExpectInstalled());
  ASSERT_NO_FATAL_FAILURE(ExpectUserUpdaterInstalled());
  base::ScopedTempFile xc_file;
  ASSERT_TRUE(xc_file.Create());

  ExpectCRURegistrationCannotRegister("", xc_file.path(), "0.0.0.1");

  ExpectUserUninstallPing(test_server_.get());
  ASSERT_NO_FATAL_FAILURE(UninstallUserUpdater());
  ExpectUninstallPing(test_server_.get());
  ASSERT_NO_FATAL_FAILURE(Uninstall());
}

TEST_F(IntegrationTestUserInSystem, CRURegistrationNeedsUpdater) {
  base::ScopedTempFile xc_file;
  ASSERT_TRUE(xc_file.Create());

  ExpectCRURegistrationCannotRegister("test", xc_file.path(), "0.0.0.1");
}

class IntegrationTestKSAdminUserInSystem : public IntegrationTestUserInSystem {
 protected:
  void ExpectUserKSAdminFetchTag(bool elevate,
                                 const std::string& product_id,
                                 const base::FilePath& xc_path,
                                 std::optional<UpdaterScope> store_flag,
                                 std::optional<std::string> want_tag) {
    user_test_commands_->ExpectKSAdminFetchTag(elevate, product_id, xc_path,
                                               store_flag, want_tag);
  }

  void ExpectBothKSAdminFetchTag(bool elevate,
                                 const std::string& product_id,
                                 const base::FilePath xc_path,
                                 std::optional<UpdaterScope> store_flag,
                                 std::optional<std::string> want_tag) {
    ExpectUserKSAdminFetchTag(elevate, product_id, xc_path, store_flag,
                              want_tag);
    ExpectKSAdminFetchTag(elevate, product_id, xc_path, store_flag, want_tag);
  }
};

TEST_F(IntegrationTestKSAdminUserInSystem, KSAdminNoAppNoTagNoMatterWhat) {
  ASSERT_NO_FATAL_FAILURE(Install());
  ASSERT_NO_FATAL_FAILURE(InstallUserUpdater());
  ASSERT_TRUE(WaitForUpdaterExit());
  ASSERT_NO_FATAL_FAILURE(ExpectInstalled());
  ASSERT_NO_FATAL_FAILURE(ExpectUserUpdaterInstalled());

  ExpectBothKSAdminFetchTag(false, "no.such.app", {}, {}, {});
  ExpectBothKSAdminFetchTag(true, "no.such.app", {}, {}, {});

  ExpectUserUninstallPing(test_server_.get());
  ASSERT_NO_FATAL_FAILURE(UninstallUserUpdater());
  ExpectUninstallPing(test_server_.get());
  ASSERT_NO_FATAL_FAILURE(Uninstall());
}

// A set of KSAdmin tests that require apps to be installed in a specific way:
//
// * product ID `system-app`, tag `system-tag`, installed at system scope
// * product ID `user-app`, tag `user-tag`, installed at user scope
// * product ID `repeat-app`, tag `repeat-system-tag`, installed at system scope
// * product ID `repeat-app`, tag `repeat-user-tag`, installed at user scope
//
// Each installation has a unique existence checker path referring to a temp
// file created during test setup and deleted during teardown. Test setup and
// teardown also installs and uninstalls updaters at both user and system scope.
//
// Tests may also rely on `nonexistent-app` to test product IDs not registered
// with any updater. The class also provides an extra temp file that is not
// the existence checker path of anything, for similar reasons.
class IntegrationTestKSAdminFourApps
    : public IntegrationTestKSAdminUserInSystem {
 protected:
  void SetUp() override {
    IntegrationTestKSAdminUserInSystem::SetUp();
    if (IsSkipped() || HasFailure()) {
      // If the test should not run, stop without installing the updater.
      return;
    }

    ASSERT_NO_FATAL_FAILURE(Install());
    ASSERT_NO_FATAL_FAILURE(InstallUserUpdater());
    ASSERT_TRUE(WaitForUpdaterExit());
    ASSERT_NO_FATAL_FAILURE(ExpectInstalled());
    ASSERT_NO_FATAL_FAILURE(ExpectUserUpdaterInstalled());

    base::Version v("1.0.0.0");

    ASSERT_NO_FATAL_FAILURE(InstallApp(kSystemAppID, v));
    ASSERT_NO_FATAL_FAILURE(SetAppTag(kSystemAppID, kSystemAppTag));
    ASSERT_TRUE(system_app_xcfile_.Create());
    ASSERT_NO_FATAL_FAILURE(
        SetExistenceCheckerPath(kSystemAppID, system_app_xcfile_.path()));

    ASSERT_NO_FATAL_FAILURE(InstallApp(kRepeatAppID, v));
    ASSERT_NO_FATAL_FAILURE(SetAppTag(kRepeatAppID, kRepeatAppSystemTag));
    ASSERT_TRUE(repeat_app_system_xcfile_.Create());
    ASSERT_NO_FATAL_FAILURE(SetExistenceCheckerPath(
        kRepeatAppID, repeat_app_system_xcfile_.path()));

    ASSERT_NO_FATAL_FAILURE(InstallUserApp(kUserAppID, v));
    ASSERT_NO_FATAL_FAILURE(SetUserAppTag(kUserAppID, kUserAppTag));
    ASSERT_TRUE(user_app_xcfile_.Create());
    ASSERT_NO_FATAL_FAILURE(
        SetUserAppExistenceCheckerPath(kUserAppID, user_app_xcfile_.path()));

    ASSERT_NO_FATAL_FAILURE(InstallUserApp(kRepeatAppID, v));
    ASSERT_NO_FATAL_FAILURE(SetUserAppTag(kRepeatAppID, kRepeatAppUserTag));
    ASSERT_TRUE(repeat_app_user_xcfile_.Create());
    ASSERT_NO_FATAL_FAILURE(SetUserAppExistenceCheckerPath(
        kRepeatAppID, repeat_app_user_xcfile_.path()));

    ASSERT_TRUE(no_app_xcfile_.Create());
  }

  void TearDown() override {
    if (IsSkipped()) {
      // Did not set up; no setup actions to reverse.
      return;
    }
    if (test_server_) {
      ExpectUserUninstallPing(test_server_.get());
    }
    ASSERT_NO_FATAL_FAILURE(UninstallUserUpdater());
    if (test_server_) {
      ExpectUninstallPing(test_server_.get());
    }
    ASSERT_NO_FATAL_FAILURE(Uninstall());

    IntegrationTestKSAdminUserInSystem::TearDown();
  }

  static constexpr char kSystemAppID[] = "system-app";
  static constexpr char kSystemAppTag[] = "system-tag";
  base::ScopedTempFile system_app_xcfile_;

  static constexpr char kRepeatAppID[] = "repeat-app";
  static constexpr char kRepeatAppSystemTag[] = "repeat-system-tag";
  base::ScopedTempFile repeat_app_system_xcfile_;
  static constexpr char kRepeatAppUserTag[] = "repeat-user-tag";
  base::ScopedTempFile repeat_app_user_xcfile_;

  static constexpr char kUserAppID[] = "user-app";
  static constexpr char kUserAppTag[] = "user-tag";
  base::ScopedTempFile user_app_xcfile_;

  static constexpr char kNonexistentAppID[] = "nonexistent-app";
  base::ScopedTempFile no_app_xcfile_;
};

TEST_F(IntegrationTestKSAdminFourApps, ServiceTagSmokeTest) {
  ExpectAppTag(kSystemAppID, kSystemAppTag);
  ExpectAppTag(kRepeatAppID, kRepeatAppSystemTag);
  ExpectUserAppTag(kUserAppID, kUserAppTag);
  ExpectUserAppTag(kRepeatAppID, kRepeatAppUserTag);
}

TEST_F(IntegrationTestKSAdminFourApps, UserLookupNoHints) {
  ExpectBothKSAdminFetchTag(false, kSystemAppID, {}, {}, kSystemAppTag);
  ExpectBothKSAdminFetchTag(false, kRepeatAppID, {}, {}, kRepeatAppSystemTag);
  ExpectBothKSAdminFetchTag(false, kUserAppID, {}, {}, kUserAppTag);
  ExpectBothKSAdminFetchTag(false, kNonexistentAppID, {}, {}, {});
}

TEST_F(IntegrationTestKSAdminFourApps, ElevatedLookupNoHints) {
  ExpectBothKSAdminFetchTag(true, kSystemAppID, {}, {}, kSystemAppTag);
  ExpectBothKSAdminFetchTag(true, kRepeatAppID, {}, {}, kRepeatAppSystemTag);
  ExpectBothKSAdminFetchTag(true, kUserAppID, {}, {}, {});
  ExpectBothKSAdminFetchTag(true, kNonexistentAppID, {}, {}, {});
}

TEST_F(IntegrationTestKSAdminFourApps, UserStoreFlag) {
  // When running elevated, ksadmin refuses to use a user store.
  ExpectBothKSAdminFetchTag(true, kSystemAppID, {}, UpdaterScope::kUser, {});
  ExpectBothKSAdminFetchTag(true, kRepeatAppID, {}, UpdaterScope::kUser, {});
  ExpectBothKSAdminFetchTag(true, kUserAppID, {}, UpdaterScope::kUser, {});
  ExpectBothKSAdminFetchTag(true, kNonexistentAppID, {}, UpdaterScope::kUser,
                            {});

  // In the presence of a user store flag, only search the user store.
  ExpectBothKSAdminFetchTag(false, kSystemAppID, {}, UpdaterScope::kUser, {});
  ExpectBothKSAdminFetchTag(false, kRepeatAppID, {}, UpdaterScope::kUser,
                            kRepeatAppUserTag);
  ExpectBothKSAdminFetchTag(false, kUserAppID, {}, UpdaterScope::kUser,
                            kUserAppTag);
  ExpectBothKSAdminFetchTag(false, kNonexistentAppID, {}, UpdaterScope::kUser,
                            {});

  // Existence checker path hinting does not alter any part of this result.
  ExpectBothKSAdminFetchTag(false, kRepeatAppID, repeat_app_user_xcfile_.path(),
                            UpdaterScope::kUser, kRepeatAppUserTag);
  ExpectBothKSAdminFetchTag(false, kRepeatAppID,
                            repeat_app_system_xcfile_.path(),
                            UpdaterScope::kUser, kRepeatAppUserTag);
  ExpectBothKSAdminFetchTag(false, kRepeatAppID, no_app_xcfile_.path(),
                            UpdaterScope::kUser, kRepeatAppUserTag);
  ExpectBothKSAdminFetchTag(true, kRepeatAppID,
                            repeat_app_system_xcfile_.path(),
                            UpdaterScope::kUser, {});
  ExpectBothKSAdminFetchTag(true, kRepeatAppID, repeat_app_user_xcfile_.path(),
                            UpdaterScope::kUser, {});
  ExpectBothKSAdminFetchTag(true, kRepeatAppID, no_app_xcfile_.path(),
                            UpdaterScope::kUser, {});
}

TEST_F(IntegrationTestKSAdminFourApps, SystemStoreFlag) {
  // In the presence of a system store flag, only search the system store.
  ExpectBothKSAdminFetchTag(false, kSystemAppID, {}, UpdaterScope::kSystem,
                            kSystemAppTag);
  ExpectBothKSAdminFetchTag(false, kRepeatAppID, {}, UpdaterScope::kSystem,
                            kRepeatAppSystemTag);
  ExpectBothKSAdminFetchTag(false, kUserAppID, {}, UpdaterScope::kSystem, {});
  ExpectBothKSAdminFetchTag(false, kNonexistentAppID, {}, UpdaterScope::kUser,
                            {});
  ExpectBothKSAdminFetchTag(true, kSystemAppID, {}, UpdaterScope::kSystem,
                            kSystemAppTag);
  ExpectBothKSAdminFetchTag(true, kRepeatAppID, {}, UpdaterScope::kSystem,
                            kRepeatAppSystemTag);
  ExpectBothKSAdminFetchTag(true, kUserAppID, {}, UpdaterScope::kSystem, {});
  ExpectBothKSAdminFetchTag(true, kNonexistentAppID, {}, UpdaterScope::kUser,
                            {});

  // Existence checker path hinting does not alter elevated results.
  ExpectBothKSAdminFetchTag(true, kRepeatAppID,
                            repeat_app_system_xcfile_.path(),
                            UpdaterScope::kSystem, kRepeatAppSystemTag);
  ExpectBothKSAdminFetchTag(true, kRepeatAppID, repeat_app_user_xcfile_.path(),
                            UpdaterScope::kSystem, kRepeatAppSystemTag);
  ExpectBothKSAdminFetchTag(true, kRepeatAppID, no_app_xcfile_.path(),
                            UpdaterScope::kSystem, kRepeatAppSystemTag);
}

// TODO: crbug/355246092 - Fix ksadmin's handling of this scenario and enable
//     this test. Currently, ksadmin will see the `--system-store` switch and
//     retrieve the registration from the system store, but not check further
//     to verify the existence checker path match.
TEST_F(IntegrationTestKSAdminFourApps,
       DISABLED_SystemStoreFlagXCPathMismatchAsUser) {
  // Because a non-elevated user can't "fix" a mismatched path for a system
  // app registration, a mismatching existence checker path causes lookup
  // to fail; because the store was explicitly specified, ksadmin will not
  // consider the user store.
  ExpectBothKSAdminFetchTag(false, kRepeatAppID,
                            repeat_app_system_xcfile_.path(),
                            UpdaterScope::kSystem, kRepeatAppSystemTag);
  ExpectBothKSAdminFetchTag(false, kRepeatAppID, repeat_app_user_xcfile_.path(),
                            UpdaterScope::kSystem, {});
  ExpectBothKSAdminFetchTag(false, kRepeatAppID, no_app_xcfile_.path(),
                            UpdaterScope::kSystem, {});
}

TEST_F(IntegrationTestKSAdminFourApps, XCPathMatch) {
  ExpectBothKSAdminFetchTag(true, kSystemAppID, system_app_xcfile_.path(), {},
                            kSystemAppTag);
  ExpectBothKSAdminFetchTag(false, kSystemAppID, system_app_xcfile_.path(), {},
                            kSystemAppTag);

  // Root can't see user stores.
  ExpectBothKSAdminFetchTag(true, kUserAppID, user_app_xcfile_.path(), {}, {});
  ExpectBothKSAdminFetchTag(false, kUserAppID, user_app_xcfile_.path(), {},
                            kUserAppTag);

  // When running as user, XC path disambiguates.
  ExpectBothKSAdminFetchTag(false, kRepeatAppID, repeat_app_user_xcfile_.path(),
                            {}, kRepeatAppUserTag);
  ExpectBothKSAdminFetchTag(false, kRepeatAppID,
                            repeat_app_system_xcfile_.path(), {},
                            kRepeatAppSystemTag);

  // Root can't see user stores, but it doesn't see the mismatching XC path
  // as a reason not to retrieve the entry in the system store, because -- since
  // the user is root -- the user would be able to fix this registration.
  ExpectBothKSAdminFetchTag(true, kRepeatAppID,
                            repeat_app_system_xcfile_.path(), {},
                            kRepeatAppSystemTag);
  ExpectBothKSAdminFetchTag(true, kRepeatAppID, repeat_app_user_xcfile_.path(),
                            {}, kRepeatAppSystemTag);
}

TEST_F(IntegrationTestKSAdminFourApps, XCPathMismatchElevated) {
  // When running as root, ksadmin only considers the system store, and doesn't
  // consider existence checking path mismatches to stop retrieval.
  ExpectBothKSAdminFetchTag(true, kSystemAppID, no_app_xcfile_.path(), {},
                            kSystemAppTag);
  ExpectBothKSAdminFetchTag(true, kUserAppID, no_app_xcfile_.path(), {}, {});
  ExpectBothKSAdminFetchTag(true, kRepeatAppID, no_app_xcfile_.path(), {},
                            kRepeatAppSystemTag);
  ExpectBothKSAdminFetchTag(true, kNonexistentAppID, no_app_xcfile_.path(), {},
                            {});
}

TEST_F(IntegrationTestKSAdminFourApps, XCPathMismatchUser) {
  // ksadmin knows a user can "fix" the existence checker path in a user
  // registration (and attempting to re-register the app will overwrite that
  // registration), but cannot "fix" (and therefore does not match) a system
  // registration with a different existence checking path.
  ExpectBothKSAdminFetchTag(false, kSystemAppID, no_app_xcfile_.path(), {}, {});
  ExpectBothKSAdminFetchTag(false, kUserAppID, no_app_xcfile_.path(), {},
                            kUserAppTag);
  ExpectBothKSAdminFetchTag(false, kRepeatAppID, no_app_xcfile_.path(), {},
                            kRepeatAppUserTag);
  ExpectBothKSAdminFetchTag(false, kNonexistentAppID, no_app_xcfile_.path(), {},
                            {});
}

TEST_F(IntegrationTestKSAdminFourApps, CRURegistrationFetchTag) {
  // Direct, unambiguous matches (or nothing matching).
  ExpectCRURegistrationFetchesTag(kSystemAppID, system_app_xcfile_.path(),
                                  kSystemAppTag);
  ExpectCRURegistrationFetchesTag(kUserAppID, user_app_xcfile_.path(),
                                  kUserAppTag);
  ExpectCRURegistrationCannotFetchTag(kNonexistentAppID, no_app_xcfile_.path());

  // Ambiguous app ID, direct XCFile path matches.
  ExpectCRURegistrationFetchesTag(
      kRepeatAppID, repeat_app_system_xcfile_.path(), kRepeatAppSystemTag);
  ExpectCRURegistrationFetchesTag(kRepeatAppID, repeat_app_user_xcfile_.path(),
                                  kRepeatAppUserTag);

  // Non-matching XCFile path can still match user apps, but only user apps.
  ExpectCRURegistrationFetchesTag(kUserAppID, no_app_xcfile_.path(),
                                  kUserAppTag);
  ExpectCRURegistrationFetchesTag(kRepeatAppID, no_app_xcfile_.path(),
                                  kRepeatAppUserTag);
  ExpectCRURegistrationCannotFetchTag(kSystemAppID, no_app_xcfile_.path());
}
#endif  // !defined(ADDRESS_SANITIZER)
#endif  // BUILDFLAG(IS_MAC)

// Windows specific tests.
#if BUILDFLAG(IS_WIN)
namespace {

void SetAuditMode() {
  ASSERT_EQ(base::win::RegKey(HKEY_LOCAL_MACHINE, kSetupStateKey, KEY_SET_VALUE)
                .WriteValue(L"ImageState", L"IMAGE_STATE_UNDEPLOYABLE"),
            ERROR_SUCCESS);
}

void ResetOemMode() {
  ASSERT_TRUE(ResetOemInstallState());
  ASSERT_EQ(base::win::RegKey(HKEY_LOCAL_MACHINE, kSetupStateKey, KEY_SET_VALUE)
                .DeleteValue(L"ImageState"),
            ERROR_SUCCESS);
}

void RewindOemState72PlusHours() {
  DWORD oem_install_time_minutes = 0;
  ASSERT_EQ(
      base::win::RegKey(HKEY_LOCAL_MACHINE, CLIENTS_KEY,
                        Wow6432(KEY_QUERY_VALUE))
          .ReadValueDW(kRegValueOemInstallTimeMin, &oem_install_time_minutes),
      ERROR_SUCCESS);

  // Rewind to 72 hours and 2 minutes before now.
  ASSERT_EQ(
      base::win::RegKey(HKEY_LOCAL_MACHINE, CLIENTS_KEY, Wow6432(KEY_SET_VALUE))
          .WriteValue(
              kRegValueOemInstallTimeMin,
              (base::Minutes(oem_install_time_minutes - 2) - kMinOemModeTime)
                  .InMinutes()),
      ERROR_SUCCESS);
}

}  // namespace

TEST_F(IntegrationTest, NoSelfUpdateIfOemMode) {
  if (!IsSystemInstall(GetUpdaterScopeForTesting())) {
    GTEST_SKIP();
  }
  ASSERT_NO_FATAL_FAILURE(SetAuditMode());
  absl::Cleanup reset_oem_mode = [] {
    ASSERT_NO_FATAL_FAILURE(ResetOemMode());
  };

  ScopedServer test_server(test_commands_);
  ASSERT_NO_FATAL_FAILURE(Install(base::Value::List().Append(kOemSwitch)));
  ASSERT_NO_FATAL_FAILURE(RunWake(0));
  ASSERT_TRUE(WaitForUpdaterExit());
  ASSERT_NO_FATAL_FAILURE(
      ExpectAppVersion(kUpdaterAppId, base::Version(kUpdaterVersion)));
  ASSERT_NO_FATAL_FAILURE(ExpectUninstallPing(&test_server));
  ASSERT_NO_FATAL_FAILURE(Uninstall());
}

TEST_F(IntegrationTest, SelfUpdateIfNoAuditModeWithOemSwitch) {
  if (!IsSystemInstall(GetUpdaterScopeForTesting())) {
    GTEST_SKIP();
  }
  ScopedServer test_server(test_commands_);
  ASSERT_NO_FATAL_FAILURE(Install(base::Value::List().Append(kOemSwitch)));
  base::Version next_version(base::StringPrintf("%s1", kUpdaterVersion));
  ASSERT_NO_FATAL_FAILURE(ExpectUpdateSequence(
      &test_server, kUpdaterAppId, "", UpdateService::Priority::kBackground,
      base::Version(kUpdaterVersion), next_version));
  ASSERT_NO_FATAL_FAILURE(RunWake(0));
  ASSERT_TRUE(WaitForUpdaterExit());
  ASSERT_NO_FATAL_FAILURE(ExpectAppVersion(kUpdaterAppId, next_version));
  ASSERT_NO_FATAL_FAILURE(ExpectUninstallPing(&test_server));
  ASSERT_NO_FATAL_FAILURE(Uninstall());
}

TEST_F(IntegrationTest, SelfUpdateIfOemModeMoreThan72Hours) {
  if (!IsSystemInstall(GetUpdaterScopeForTesting())) {
    GTEST_SKIP();
  }
  ASSERT_NO_FATAL_FAILURE(SetAuditMode());
  absl::Cleanup reset_oem_mode = [] {
    ASSERT_NO_FATAL_FAILURE(ResetOemMode());
  };

  ScopedServer test_server(test_commands_);
  ASSERT_NO_FATAL_FAILURE(Install(base::Value::List().Append(kOemSwitch)));
  ASSERT_NO_FATAL_FAILURE(RewindOemState72PlusHours());
  base::Version next_version(base::StringPrintf("%s1", kUpdaterVersion));
  ASSERT_NO_FATAL_FAILURE(ExpectUpdateSequence(
      &test_server, kUpdaterAppId, "", UpdateService::Priority::kBackground,
      base::Version(kUpdaterVersion), next_version));
  ASSERT_NO_FATAL_FAILURE(RunWake(0));
  ASSERT_TRUE(WaitForUpdaterExit());
  ASSERT_NO_FATAL_FAILURE(ExpectAppVersion(kUpdaterAppId, next_version));
  ASSERT_NO_FATAL_FAILURE(ExpectUninstallPing(&test_server));
  ASSERT_NO_FATAL_FAILURE(Uninstall());
}

TEST_F(IntegrationTest,
       NoSelfUpdateIfOemModeMoreThan72HoursButEulaNotAccepted) {
  if (!IsSystemInstall(GetUpdaterScopeForTesting())) {
    GTEST_SKIP();
  }
  ASSERT_NO_FATAL_FAILURE(SetAuditMode());
  absl::Cleanup reset_oem_mode = [] {
    ASSERT_NO_FATAL_FAILURE(ResetOemMode());
  };

  ScopedServer test_server(test_commands_);
  ASSERT_NO_FATAL_FAILURE(Install(
      base::Value::List().Append(kOemSwitch).Append(kEulaRequiredSwitch)));
  ASSERT_NO_FATAL_FAILURE(RewindOemState72PlusHours());
  ASSERT_NO_FATAL_FAILURE(RunWake(0));
  ASSERT_TRUE(WaitForUpdaterExit());
  ASSERT_NO_FATAL_FAILURE(
      ExpectAppVersion(kUpdaterAppId, base::Version(kUpdaterVersion)));
  ASSERT_NO_FATAL_FAILURE(Uninstall());
}

TEST_F(IntegrationTest, Handoff) {
  ScopedServer test_server(test_commands_);
  ASSERT_NO_FATAL_FAILURE(Install());

  const std::string kAppId("test");
  const base::Version v1("1");
  ASSERT_NO_FATAL_FAILURE(ExpectInstallSequence(
      &test_server, kAppId, "", UpdateService::Priority::kForeground,
      base::Version({0, 0, 0, 0}), v1));
  ASSERT_NO_FATAL_FAILURE(RunHandoff(kAppId));
  ASSERT_TRUE(WaitForUpdaterExit());
  ASSERT_NO_FATAL_FAILURE(ExpectAppVersion(kAppId, v1));

  ASSERT_NO_FATAL_FAILURE(ExpectUninstallPing(&test_server));
  ASSERT_NO_FATAL_FAILURE(Uninstall());
}

TEST_F(IntegrationTest, ForceInstallApp) {
  ScopedServer test_server(test_commands_);
  ASSERT_NO_FATAL_FAILURE(Install());

  base::Value::Dict group_policies;
  group_policies.Set("installtest1",
                     IsSystemInstall(GetUpdaterScopeForTesting())
                         ? kPolicyForceInstallMachine
                         : kPolicyForceInstallUser);
  ASSERT_NO_FATAL_FAILURE(SetGroupPolicies(group_policies));

  ExpectUpdateCheckRequest(&test_server);

  const std::string kAppId("test1");
  base::Version v0point1("0.1");
  ASSERT_NO_FATAL_FAILURE(ExpectInstallSequence(
      &test_server, kAppId, "", UpdateService::Priority::kForeground,
      base::Version("0.0.0.0"), v0point1));
  ASSERT_NO_FATAL_FAILURE(RunWake(0));

  ASSERT_TRUE(WaitForUpdaterExit());
  ASSERT_NO_FATAL_FAILURE(ExpectAppVersion(kAppId, v0point1));

  ASSERT_NO_FATAL_FAILURE(ExpectUninstallPing(&test_server));
  ASSERT_NO_FATAL_FAILURE(Uninstall());
}

TEST_F(IntegrationTest, NeedsAdminPrefers) {
  if (::IsUserAnAdmin() && !IsSystemInstall(GetUpdaterScopeForTesting())) {
    GTEST_SKIP();
  }

  ScopedServer test_server(test_commands_);
  const std::string kAppId("test");
  const base::Version v1("1");
  ASSERT_NO_FATAL_FAILURE(ExpectInstallSequence(
      &test_server, kAppId, "", UpdateService::Priority::kForeground,
      base::Version({0, 0, 0, 0}), v1));

  ASSERT_NO_FATAL_FAILURE(InstallUpdaterAndApp(
      {}, /*is_silent_install=*/true,
      base::StrCat({"appguid=", kAppId, "&needsadmin=Prefers&usagestats=1"})));
  ASSERT_TRUE(WaitForUpdaterExit());

  ASSERT_NO_FATAL_FAILURE(ExpectAppVersion(kAppId, v1));

  ASSERT_NO_FATAL_FAILURE(ExpectUninstallPing(&test_server));
  ASSERT_NO_FATAL_FAILURE(Uninstall());
}

TEST_F(IntegrationTest, MarshalInterface) {
  ASSERT_NO_FATAL_FAILURE(Install());
  ASSERT_NO_FATAL_FAILURE(ExpectMarshalInterfaceSucceeds());
  ASSERT_NO_FATAL_FAILURE(Uninstall());
}

TEST_F(IntegrationTest, LegacyProcessLauncher) {
  if (!IsSystemInstall(GetUpdaterScopeForTesting())) {
    GTEST_SKIP() << "Process launcher is only registered for system installs.";
  }
  ScopedServer test_server(test_commands_);

  ASSERT_NO_FATAL_FAILURE(Install());

  // `ExpectLegacyProcessLauncherSucceeds` runs the process launcher once with
  // usagestats enabled, and twice without, so only a single ping is expected.
  ASSERT_NO_FATAL_FAILURE(ExpectAppCommandPing(
      &test_server, "{831EF4D0-B729-4F61-AA34-91526481799D}", "cmd", 5420, 1,
      update_client::protocol_request::kEventAppCommandComplete, {}));
  ASSERT_NO_FATAL_FAILURE(ExpectLegacyProcessLauncherSucceeds());

  ASSERT_NO_FATAL_FAILURE(ExpectUninstallPing(&test_server));
  ASSERT_NO_FATAL_FAILURE(Uninstall());
}

TEST_F(IntegrationTest, LegacyAppCommandWeb_NoUsageStats_NoPing) {
  ScopedServer test_server(test_commands_);
  ASSERT_NO_FATAL_FAILURE(Install());

  const char kAppId[] = "test1";
  ASSERT_NO_FATAL_FAILURE(InstallApp(kAppId));

  base::Value::List parameters;
  parameters.Append("5432");
  ASSERT_NO_FATAL_FAILURE(
      ExpectLegacyAppCommandWebSucceeds(kAppId, "command1", parameters, 5432));

  ASSERT_NO_FATAL_FAILURE(ExpectUninstallPing(&test_server));
  ASSERT_NO_FATAL_FAILURE(Uninstall());
}

TEST_F(IntegrationTest, LegacyAppCommandWeb_UsageStatsEnabled_ExpectPing) {
  ScopedServer test_server(test_commands_);
  ASSERT_NO_FATAL_FAILURE(Install());

  const std::string kAppId("test");
  // Enable usagestats.
  InstallApp(kAppId, base::Version("0.1"));
  ASSERT_EQ(
      base::win::RegKey(
          UpdaterScopeToHKeyRoot(GetUpdaterScopeForTesting()),
          base::StrCat({CLIENT_STATE_KEY, base::UTF8ToWide(kAppId)}).c_str(),
          Wow6432(KEY_WRITE))
          .WriteValue(L"usagestats", 1),
      ERROR_SUCCESS);

  base::Version v1("1");
  ASSERT_NO_FATAL_FAILURE(ExpectUpdateSequence(
      &test_server, kAppId, "", UpdateService::Priority::kBackground,
      base::Version("0.1"), v1));

  // Run wake to pick up the usage stats.
  ASSERT_NO_FATAL_FAILURE(RunWake(0));
  ASSERT_NO_FATAL_FAILURE(ExpectAppVersion(kAppId, v1));

  // The test runs the appcommand twice, so two pings of
  // `kEventAppCommandComplete`.
  for (int i = 0; i <= 1; ++i) {
    ASSERT_NO_FATAL_FAILURE(ExpectAppCommandPing(
        &test_server, kAppId, "command1", 5432, 1,
        update_client::protocol_request::kEventAppCommandComplete, v1));
  }

  base::Value::List parameters;
  parameters.Append("5432");
  ASSERT_NO_FATAL_FAILURE(
      ExpectLegacyAppCommandWebSucceeds(kAppId, "command1", parameters, 5432));

  ASSERT_NO_FATAL_FAILURE(ExpectUninstallPing(&test_server));
  ASSERT_NO_FATAL_FAILURE(Uninstall());
}

TEST_F(IntegrationTest,
       LegacyAppCommandWeb_InstallUpdaterAndApp_UsageStatsEnabled_ExpectPings) {
  ScopedServer test_server(test_commands_);
  const std::string kAppId("test");
  const base::Version v1("1");
  ASSERT_NO_FATAL_FAILURE(ExpectInstallSequence(
      &test_server, kAppId, "", UpdateService::Priority::kForeground,
      base::Version({0, 0, 0, 0}), v1));

  ASSERT_NO_FATAL_FAILURE(
      InstallUpdaterAndApp(kAppId, /*is_silent_install=*/true, "usagestats=1"));
  ASSERT_TRUE(WaitForUpdaterExit());

  ASSERT_NO_FATAL_FAILURE(ExpectAppVersion(kAppId, v1));

  // The test runs the appcommand twice, so two pings of
  // `kEventAppCommandComplete`.
  for (int i = 0; i <= 1; ++i) {
    ASSERT_NO_FATAL_FAILURE(ExpectAppCommandPing(
        &test_server, kAppId, "command1", 5432, 1,
        update_client::protocol_request::kEventAppCommandComplete, v1));
  }

  base::Value::List parameters;
  parameters.Append("5432");
  ASSERT_NO_FATAL_FAILURE(
      ExpectLegacyAppCommandWebSucceeds(kAppId, "command1", parameters, 5432));

  ASSERT_NO_FATAL_FAILURE(ExpectUninstallPing(&test_server));
  ASSERT_NO_FATAL_FAILURE(Uninstall());
}

TEST_F(IntegrationTest, LegacyPolicyStatus) {
  ScopedServer test_server(test_commands_);
  ASSERT_NO_FATAL_FAILURE(Install());

  const std::string kAppId("test");
  ASSERT_NO_FATAL_FAILURE(InstallApp(kAppId));
  base::Version v1("1");
  ASSERT_NO_FATAL_FAILURE(ExpectUpdateSequence(
      &test_server, kAppId, "", UpdateService::Priority::kBackground,
      base::Version("0.1"), v1));
  ASSERT_NO_FATAL_FAILURE(RunWake(0));
  ASSERT_NO_FATAL_FAILURE(ExpectAppVersion(kAppId, v1));

  ASSERT_NO_FATAL_FAILURE(ExpectLegacyPolicyStatusSucceeds());

  ASSERT_NO_FATAL_FAILURE(ExpectUninstallPing(&test_server));
  ASSERT_NO_FATAL_FAILURE(Uninstall());
}

TEST_F(IntegrationTest, UninstallCmdLine) {
  ASSERT_NO_FATAL_FAILURE(Install());
  ASSERT_NO_FATAL_FAILURE(ExpectInstalled());
  ASSERT_NO_FATAL_FAILURE(ExpectVersionActive(kUpdaterVersion));

  // Running the uninstall command does not uninstall this instance of the
  // updater right after installing it (not enough server starts).
  ASSERT_NO_FATAL_FAILURE(RunUninstallCmdLine());
  ASSERT_TRUE(WaitForUpdaterExit());
  ASSERT_NO_FATAL_FAILURE(ExpectInstalled());

  ASSERT_NO_FATAL_FAILURE(SetServerStarts(24));

  // Uninstall the idle updater.
  ASSERT_NO_FATAL_FAILURE(RunUninstallCmdLine());
  ASSERT_TRUE(WaitForUpdaterExit());
}

TEST_F(IntegrationTest, LogFileInTmpAfterUninstall) {
  ASSERT_NO_FATAL_FAILURE(Install());
  ASSERT_NO_FATAL_FAILURE(ExpectInstalled());
  ASSERT_NO_FATAL_FAILURE(ExpectVersionActive(kUpdaterVersion));

  // Running the uninstall command does not uninstall this instance of the
  // updater right after installing it (not enough server starts).
  ASSERT_NO_FATAL_FAILURE(RunUninstallCmdLine());
  ASSERT_TRUE(WaitForUpdaterExit());
  ASSERT_NO_FATAL_FAILURE(ExpectInstalled());

  // Expect no updater logs in the temp dir.
  EXPECT_EQ(GetUpdaterLogFilesInTmp().size(), 0u);

  ASSERT_NO_FATAL_FAILURE(SetServerStarts(24));

  // Uninstall the idle updater.
  ASSERT_NO_FATAL_FAILURE(RunUninstallCmdLine());
  ASSERT_TRUE(WaitForUpdaterExit());

  // Expect a single updater log in the temp dir.
  int invocation_count = 0;
  for (const auto& file : GetUpdaterLogFilesInTmp()) {
    ++invocation_count;
    if (invocation_count == 1) {
      EXPECT_EQ(file.BaseName().value(), L"updater.log");
    } else {
      ADD_FAILURE() << "Unexpected, more than one updater log found: " << file;
    }
  }
  EXPECT_EQ(invocation_count, 1);
}

TEST_F(IntegrationTest, AppLogoUrl) {
  ScopedServer test_update_server(test_commands_);
  ScopedServer test_logo_server(test_commands_);
  EnterTestMode(test_update_server.update_url(),
                test_update_server.crash_upload_url(),
                test_update_server.device_management_url(),
                test_logo_server.app_logo_url(), base::Minutes(5));

  const std::string kAppId("googletest");
  const base::Version v1("1");
  ASSERT_NO_FATAL_FAILURE(ExpectInstallSequence(
      &test_update_server, kAppId, "", UpdateService::Priority::kForeground,
      base::Version({0, 0, 0, 0}), v1));

  std::string app_logo_bytes;
  ASSERT_TRUE(base::ReadFileToString(
      test::GetTestFilePath("app_logos")
          .AppendASCII(base::StringPrintf("%s.bmp", kAppId.c_str())),
      &app_logo_bytes));
  test_logo_server.ExpectOnce(
      {
          request::GetPathMatcher(base::StringPrintf(
              "%s%s.bmp\\?lang=%s", test_logo_server.app_logo_path().c_str(),
              kAppId.c_str(),
              base::WideToUTF8(GetPreferredLanguage()).c_str())),
      },
      app_logo_bytes);
  ASSERT_NO_FATAL_FAILURE(InstallUpdaterAndApp(
      kAppId, /*is_silent_install=*/false, "usagestats=1",
      base::WideToUTF8(
          GetLocalizedString(IDS_BUNDLE_INSTALLED_SUCCESSFULLY_BASE)),
      /*always_launch_cmd=*/false, /*verify_app_logo_loaded=*/true));
  ASSERT_TRUE(WaitForUpdaterExit());

  ASSERT_NO_FATAL_FAILURE(ExpectAppVersion(kAppId, v1));

  ASSERT_NO_FATAL_FAILURE(ExpectUninstallPing(&test_update_server));
  ASSERT_NO_FATAL_FAILURE(Uninstall());
}

TEST_F(IntegrationTest, BundleNameShowsUpInUI) {
  ScopedServer test_server(test_commands_);
  const std::string kAppId("test");
  const std::string kAppName("Test%20App");
  const base::Version v1("1");
  ASSERT_NO_FATAL_FAILURE(ExpectInstallSequence(
      &test_server, kAppId, "", UpdateService::Priority::kForeground,
      base::Version({0, 0, 0, 0}), v1));

  ASSERT_NO_FATAL_FAILURE(InstallUpdaterAndApp(
      /*app_id=*/{}, /*is_silent_install=*/false, /*tag=*/
      base::StrCat(
          {"appguid=", kAppId, "&appname=", kAppName, "&usagestats=1"}),
      base::WideToUTF8(
          GetLocalizedString(IDS_BUNDLE_INSTALLED_SUCCESSFULLY_BASE))));
  ASSERT_TRUE(WaitForUpdaterExit());

  ASSERT_NO_FATAL_FAILURE(ExpectAppVersion(kAppId, v1));

  ASSERT_NO_FATAL_FAILURE(ExpectUninstallPing(&test_server));
  ASSERT_NO_FATAL_FAILURE(Uninstall());
}

TEST_F(IntegrationTest, OfflineInstall) {
  ASSERT_NO_FATAL_FAILURE(Install());
  ASSERT_NO_FATAL_FAILURE(ExpectInstalled());
  ASSERT_NO_FATAL_FAILURE(RunOfflineInstall(/*is_legacy_install=*/false,
                                            /*is_silent_install=*/false));
  ASSERT_NO_FATAL_FAILURE(Uninstall());
}

TEST_F(IntegrationTest, OfflineInstallOsNotSupported) {
  ASSERT_NO_FATAL_FAILURE(Install());
  ASSERT_NO_FATAL_FAILURE(ExpectInstalled());
  ASSERT_NO_FATAL_FAILURE(
      RunOfflineInstallOsNotSupported(/*is_legacy_install=*/false,
                                      /*is_silent_install=*/false));
  ASSERT_NO_FATAL_FAILURE(Uninstall());
}

TEST_F(IntegrationTest, OfflineInstallSilent) {
  ASSERT_NO_FATAL_FAILURE(Install());
  ASSERT_NO_FATAL_FAILURE(ExpectInstalled());
  ASSERT_NO_FATAL_FAILURE(RunOfflineInstall(/*is_legacy_install=*/false,
                                            /*is_silent_install=*/true));
  ASSERT_NO_FATAL_FAILURE(Uninstall());
}

TEST_F(IntegrationTest, OfflineInstallOsNotSupportedSilent) {
  ASSERT_NO_FATAL_FAILURE(Install());
  ASSERT_NO_FATAL_FAILURE(ExpectInstalled());
  ASSERT_NO_FATAL_FAILURE(
      RunOfflineInstallOsNotSupported(/*is_legacy_install=*/false,
                                      /*is_silent_install=*/true));
  ASSERT_NO_FATAL_FAILURE(Uninstall());
}

TEST_F(IntegrationTest, OfflineInstallSilentLegacy) {
  ASSERT_NO_FATAL_FAILURE(Install());
  ASSERT_NO_FATAL_FAILURE(ExpectInstalled());
  ASSERT_NO_FATAL_FAILURE(RunOfflineInstall(/*is_legacy_install=*/true,
                                            /*is_silent_install=*/true));
  ASSERT_NO_FATAL_FAILURE(Uninstall());
}

TEST_F(IntegrationTest, OfflineInstallOsNotSupportedSilentLegacy) {
  ASSERT_NO_FATAL_FAILURE(Install());
  ASSERT_NO_FATAL_FAILURE(ExpectInstalled());
  ASSERT_NO_FATAL_FAILURE(
      RunOfflineInstallOsNotSupported(/*is_legacy_install=*/true,
                                      /*is_silent_install=*/true));
  ASSERT_NO_FATAL_FAILURE(Uninstall());
}

TEST_F(IntegrationTest, OfflineInstallEulaRequired) {
  ASSERT_NO_FATAL_FAILURE(
      Install(base::Value::List().Append(kEulaRequiredSwitch)));
  ASSERT_NO_FATAL_FAILURE(ExpectInstalled());
  ASSERT_NO_FATAL_FAILURE(RunOfflineInstall(/*is_legacy_install=*/false,
                                            /*is_silent_install=*/false));
  ASSERT_NO_FATAL_FAILURE(Uninstall());
}

TEST_F(IntegrationTest, OfflineInstallOemMode) {
  if (!IsSystemInstall(GetUpdaterScopeForTesting())) {
    GTEST_SKIP();
  }
  ASSERT_NO_FATAL_FAILURE(SetAuditMode());
  absl::Cleanup reset_oem_mode = [] {
    ASSERT_NO_FATAL_FAILURE(ResetOemMode());
  };

  ASSERT_NO_FATAL_FAILURE(Install(base::Value::List().Append(kOemSwitch)));
  ASSERT_NO_FATAL_FAILURE(ExpectInstalled());
  ASSERT_NO_FATAL_FAILURE(RunOfflineInstall(/*is_legacy_install=*/false,
                                            /*is_silent_install=*/false));
  ASSERT_NO_FATAL_FAILURE(Uninstall());
}

TEST_F(IntegrationTest, ExpectErrorUIWhenGetSetupLockFails) {
  ScopedServer test_update_server(test_commands_);
  const std::string kAppId("googletest");
  const base::Version v1("1");
  ASSERT_NO_FATAL_FAILURE(ExpectInstallSequence(
      &test_update_server, kAppId, "", UpdateService::Priority::kForeground,
      base::Version({0, 0, 0, 0}), v1));

  // The test runs the installer twice. One installer succeeds, and the other
  // installer times out on the setup lock.
  for (int i = 0; i <= 1; ++i) {
    ASSERT_NO_FATAL_FAILURE(InstallUpdaterAndApp(
        kAppId, /*is_silent_install=*/false, "usagestats=1",
        /*child_window_text_to_find=*/{},
        /*always_launch_cmd=*/false,
        /*verify_app_logo_loaded=*/false,
        /*expect_success=*/true,
        /*wait_for_the_installer=*/false));
    base::PlatformThread::Sleep(base::Seconds(1));
  }

  // Dismiss the setup lock error dialog, and then the success dialog.
  for (const auto message_id : {IDS_UNABLE_TO_GET_SETUP_LOCK_BASE,
                                IDS_BUNDLE_INSTALLED_SUCCESSFULLY_BASE}) {
    ASSERT_NO_FATAL_FAILURE(
        CloseInstallCompleteDialog({}, GetLocalizedString(message_id)));
  }

  ASSERT_TRUE(WaitForUpdaterExit());

  ASSERT_NO_FATAL_FAILURE(ExpectAppVersion(kAppId, v1));

  ASSERT_NO_FATAL_FAILURE(ExpectUninstallPing(&test_update_server));
  ASSERT_NO_FATAL_FAILURE(Uninstall());
}

class IntegrationTestLegacyUpdate3WebNewInstall : public IntegrationTest {
 public:
  IntegrationTestLegacyUpdate3WebNewInstall() = default;
  ~IntegrationTestLegacyUpdate3WebNewInstall() override = default;

 protected:
  void SetUp() override {
    if (!::IsUserAnAdmin() && IsSystemInstall(GetUpdaterScopeForTesting())) {
      GTEST_SKIP();
    }

    IntegrationTest::SetUp();
    if (IsSkipped()) {
      return;
    }

    test_server_ = std::make_unique<ScopedServer>(test_commands_);
    ASSERT_NO_FATAL_FAILURE(Install());
  }

  void TearDown() override {
    if (IsSkipped()) {
      return;
    }
    ASSERT_NO_FATAL_FAILURE(ExpectUninstallPing(test_server_.get()));
    ASSERT_NO_FATAL_FAILURE(Uninstall());

    IntegrationTest::TearDown();
  }

  std::unique_ptr<ScopedServer> test_server_;
  static constexpr char kAppId[] = "test1";
};

TEST_F(IntegrationTestLegacyUpdate3WebNewInstall, CheckForInstall) {
  ASSERT_NO_FATAL_FAILURE(ExpectUpdateCheckSequence(
      test_server_.get(), kAppId, UpdateService::Priority::kForeground,
      base::Version(kNullVersion), base::Version("0.1")));
  ASSERT_NO_FATAL_FAILURE(
      ExpectLegacyUpdate3WebSucceeds(kAppId, AppBundleWebCreateMode::kCreateApp,
                                     STATE_UPDATE_AVAILABLE, S_OK));
}

TEST_F(IntegrationTestLegacyUpdate3WebNewInstall, Install) {
  const base::Version v1("0.1");
  ASSERT_NO_FATAL_FAILURE(ExpectUpdateCheckSequence(
      test_server_.get(), kAppId, UpdateService::Priority::kForeground,
      base::Version(kNullVersion), v1));

  // "expected_install_data_index" is set in `integration_tests_win.cc`,
  // `DoUpdate`.
  ASSERT_NO_FATAL_FAILURE(ExpectInstallSequence(
      test_server_.get(), kAppId, "expected_install_data_index",
      UpdateService::Priority::kForeground, base::Version(kNullVersion), v1));

  ASSERT_NO_FATAL_FAILURE(
      ExpectLegacyUpdate3WebSucceeds(kAppId, AppBundleWebCreateMode::kCreateApp,
                                     STATE_INSTALL_COMPLETE, S_OK));
  base::Value::Dict expected_app_state;
  expected_app_state.Set("app_id", kAppId);
  expected_app_state.Set("version", v1.GetString());

  // These values are set in `integration_tests_win.cc`, `DoUpdate`, in the call
  // to `createApp`:
  expected_app_state.Set("ap", "DoUpdateAP");
  expected_app_state.Set("brand_code", "BRND");

  expected_app_state.Set("brand_path", "");
  expected_app_state.Set("ecp", "");
  base::Value::Dict expected_app_states;
  expected_app_states.Set(kAppId, std::move(expected_app_state));

  ASSERT_NO_FATAL_FAILURE(GetAppStates(expected_app_states));
}

class IntegrationTestLegacyUpdate3Web : public IntegrationTest {
 public:
  IntegrationTestLegacyUpdate3Web() = default;
  ~IntegrationTestLegacyUpdate3Web() override = default;

 protected:
  void SetUp() override {
    IntegrationTest::SetUp();
    if (IsSkipped()) {
      return;
    }

    test_server_ = std::make_unique<ScopedServer>(test_commands_);
    ASSERT_NO_FATAL_FAILURE(Install());
    ASSERT_NO_FATAL_FAILURE(InstallApp(kAppId));
  }

  void TearDown() override {
    if (IsSkipped()) {
      return;
    }
    ASSERT_NO_FATAL_FAILURE(ExpectUninstallPing(test_server_.get()));
    ASSERT_NO_FATAL_FAILURE(Uninstall());

    IntegrationTest::TearDown();
  }

  std::unique_ptr<ScopedServer> test_server_;
  static constexpr char kAppId[] = "test1";
};

TEST_F(IntegrationTestLegacyUpdate3Web, NoUpdate) {
  ASSERT_NO_FATAL_FAILURE(ExpectNoUpdateSequence(test_server_.get(), kAppId));
  ASSERT_NO_FATAL_FAILURE(ExpectLegacyUpdate3WebSucceeds(
      kAppId, AppBundleWebCreateMode::kCreateInstalledApp, STATE_NO_UPDATE,
      S_OK));
}

TEST_F(IntegrationTestLegacyUpdate3Web, DisabledPolicyManual) {
  ASSERT_TRUE(WaitForUpdaterExit());
  base::Value::Dict group_policies;
  group_policies.Set("updatetest1", kPolicyAutomaticUpdatesOnly);
  ASSERT_NO_FATAL_FAILURE(SetGroupPolicies(group_policies));
  ASSERT_NO_FATAL_FAILURE(ExpectLegacyUpdate3WebSucceeds(
      kAppId, AppBundleWebCreateMode::kCreateInstalledApp, STATE_ERROR,
      GOOPDATE_E_APP_UPDATE_DISABLED_BY_POLICY_MANUAL));
}

TEST_F(IntegrationTestLegacyUpdate3Web, DisabledPolicy) {
  ASSERT_TRUE(WaitForUpdaterExit());
  base::Value::Dict group_policies;
  group_policies.Set("updatetest1", kPolicyDisabled);
  ASSERT_NO_FATAL_FAILURE(SetGroupPolicies(group_policies));
  ExpectLegacyUpdate3WebSucceeds(
      kAppId, AppBundleWebCreateMode::kCreateInstalledApp, STATE_ERROR,
      GOOPDATE_E_APP_UPDATE_DISABLED_BY_POLICY);
}

TEST_F(IntegrationTestLegacyUpdate3Web, CheckForUpdate) {
  ASSERT_NO_FATAL_FAILURE(ExpectUpdateCheckSequence(
      test_server_.get(), kAppId, UpdateService::Priority::kForeground,
      base::Version("0.1"), base::Version("0.2")));
  ASSERT_NO_FATAL_FAILURE(ExpectLegacyUpdate3WebSucceeds(
      kAppId, AppBundleWebCreateMode::kCreateInstalledApp,
      STATE_UPDATE_AVAILABLE, S_OK));
}

TEST_F(IntegrationTestLegacyUpdate3Web, Update) {
  ASSERT_NO_FATAL_FAILURE(ExpectUpdateCheckSequence(
      test_server_.get(), kAppId, UpdateService::Priority::kForeground,
      base::Version("0.1"), base::Version("0.2")));
  ASSERT_NO_FATAL_FAILURE(ExpectUpdateSequence(
      test_server_.get(), kAppId, "", UpdateService::Priority::kForeground,
      base::Version("0.1"), base::Version("0.2")));
  ASSERT_NO_FATAL_FAILURE(ExpectLegacyUpdate3WebSucceeds(
      kAppId, AppBundleWebCreateMode::kCreateInstalledApp,
      STATE_INSTALL_COMPLETE, S_OK));
}

TEST_F(IntegrationTestLegacyUpdate3Web, CheckForInstall) {
  ASSERT_NO_FATAL_FAILURE(ExpectUpdateCheckSequence(
      test_server_.get(), kAppId, UpdateService::Priority::kForeground,
      base::Version("0.1"), base::Version("0.1")));
  ASSERT_NO_FATAL_FAILURE(
      ExpectLegacyUpdate3WebSucceeds(kAppId, AppBundleWebCreateMode::kCreateApp,
                                     STATE_UPDATE_AVAILABLE, S_OK));
}

TEST_F(IntegrationTestLegacyUpdate3Web, Install) {
  ASSERT_NO_FATAL_FAILURE(ExpectUpdateCheckSequence(
      test_server_.get(), kAppId, UpdateService::Priority::kForeground,
      base::Version("0.1"), base::Version("0.1")));
  ASSERT_NO_FATAL_FAILURE(ExpectInstallSequence(
      test_server_.get(), kAppId, "", UpdateService::Priority::kForeground,
      base::Version("0.1"), base::Version("0.1")));
  ASSERT_NO_FATAL_FAILURE(
      ExpectLegacyUpdate3WebSucceeds(kAppId, AppBundleWebCreateMode::kCreateApp,
                                     STATE_INSTALL_COMPLETE, S_OK));
}
class IntegrationTestMsi : public IntegrationTest {
 public:
  IntegrationTestMsi() = default;
  ~IntegrationTestMsi() override = default;

  static constexpr char kMsiAppId[] = "{c28fcf72-bcf2-45c5-8def-31a74ac02012}";

 protected:
  void SetUp() override {
    if (!IsSystemInstall(GetUpdaterScopeForTesting())) {
      GTEST_SKIP();
    }
    IntegrationTest::SetUp();
    if (IsSkipped()) {
      return;
    }
    test_server_ = std::make_unique<ScopedServer>(test_commands_);
    ASSERT_NO_FATAL_FAILURE(RemoveMsiProductData(kMsiProductIdInitialVersion));
    ASSERT_NO_FATAL_FAILURE(RemoveMsiProductData(kMsiProductIdUpdatedVersion));
  }

  void TearDown() override {
    if (IsSkipped()) {
      return;
    }
    ASSERT_NO_FATAL_FAILURE(RemoveMsiProductData(kMsiProductIdInitialVersion));
    ASSERT_NO_FATAL_FAILURE(RemoveMsiProductData(kMsiProductIdUpdatedVersion));
    IntegrationTest::TearDown();
  }

  void InstallMsiWithVersion(const base::Version& version) {
    InstallApp(kMsiAppId, version);
    base::FilePath msi_path;
    ASSERT_TRUE(base::PathService::Get(base::DIR_EXE, &msi_path));
    msi_path = msi_path.Append(
        GetInstallerPath(base::StrCat({kMsiAppId, ".", version.GetString()}))
            .AppendASCII(kMsiCrx)
            .RemoveExtension());
    const std::wstring command = BuildMsiCommandLine({}, {}, msi_path);
    base::Process process = base::LaunchProcess(command, {});
    if (!process.IsValid()) {
      LOG(ERROR) << "Invalid process launching command: " << command;
    }
    int exit_code = -1;
    EXPECT_TRUE(process.WaitForExitWithTimeout(TestTimeouts::action_timeout(),
                                               &exit_code));
    EXPECT_EQ(0, exit_code);

    ExpectAppInstalled(kMsiAppId, version);
  }

  void RemoveMsiProductData(const std::wstring& msi_product_id) {
    ASSERT_FALSE(msi_product_id.empty());
    for (const auto& [root, key] :
         {std::make_pair(HKEY_LOCAL_MACHINE,
                         L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Instal"
                         L"ler\\UserData\\S-1-5-18\\Products"),
          std::make_pair(HKEY_CLASSES_ROOT, L"Installer\\Products")}) {
      for (const auto& access_mask : {KEY_WOW64_32KEY, KEY_WOW64_64KEY}) {
        base::win::RegKey(root, key, DELETE | access_mask)
            .DeleteKey(msi_product_id.c_str());
      }
    }
  }

  std::unique_ptr<ScopedServer> test_server_;
  static constexpr char kMsiCrx[] = "TestSystemMsiInstaller.msi.crx3";
  static constexpr wchar_t kMsiProductIdInitialVersion[] =
      L"40C670A26D240095081B31C3EDEF2BD2";
  static constexpr wchar_t kMsiProductIdUpdatedVersion[] =
      L"D2B2AC298EFCE2757A975961532CDE7D";
  const base::Version kMsiInitialVersion = base::Version("1.0.0.0");
  const base::Version kMsiUpdatedVersion = base::Version("2.0.0.0");
};

TEST_F(IntegrationTestMsi, Install) {
  ASSERT_NO_FATAL_FAILURE(Install());
  ASSERT_NO_FATAL_FAILURE(ExpectInstalled());

  const base::FilePath crx_path = GetInstallerPath(kMsiCrx);
  ExpectAppsUpdateSequence(
      UpdaterScope::kSystem, test_server_.get(),
      /*request_attributes=*/{},
      {
          AppUpdateExpectation({}, kMsiAppId, base::Version({0, 0, 0, 0}),
                               kMsiUpdatedVersion,
                               /*is_install=*/true,
                               /*should_update=*/true, false, "", "", crx_path),
      });

  ASSERT_NO_FATAL_FAILURE(InstallAppViaService(kMsiAppId));
  ExpectAppInstalled(kMsiAppId, kMsiUpdatedVersion);
  ASSERT_NO_FATAL_FAILURE(ExpectUninstallPing(test_server_.get()));
  ASSERT_NO_FATAL_FAILURE(Uninstall());
}

TEST_F(IntegrationTestMsi, InstallViaCommandLine) {
  const base::FilePath crx_path = GetInstallerPath(kMsiCrx);
  ExpectAppsUpdateSequence(
      UpdaterScope::kSystem, test_server_.get(),
      /*request_attributes=*/{},
      {
          AppUpdateExpectation({}, kMsiAppId, base::Version({0, 0, 0, 0}),
                               kMsiUpdatedVersion,
                               /*is_install=*/true,
                               /*should_update=*/true, false, "", "", crx_path),
      });

  ASSERT_NO_FATAL_FAILURE(InstallUpdaterAndApp(
      kMsiAppId, /*is_silent_install=*/true, "usagestats=1"));
  ASSERT_TRUE(WaitForUpdaterExit());

  ExpectAppInstalled(kMsiAppId, kMsiUpdatedVersion);

  ASSERT_NO_FATAL_FAILURE(ExpectUninstallPing(test_server_.get()));
  ASSERT_NO_FATAL_FAILURE(Uninstall());
}

TEST_F(IntegrationTestMsi, Upgrade) {
  ASSERT_NO_FATAL_FAILURE(Install());
  ASSERT_NO_FATAL_FAILURE(ExpectInstalled());
  InstallMsiWithVersion(kMsiInitialVersion);

  const base::FilePath crx_path = GetInstallerPath(kMsiCrx);
  ExpectAppsUpdateSequence(
      UpdaterScope::kSystem, test_server_.get(),
      /*request_attributes=*/{},
      {
          AppUpdateExpectation({}, kMsiAppId, kMsiInitialVersion,
                               kMsiUpdatedVersion,
                               /*is_install=*/false,
                               /*should_update=*/true, false, "", "", crx_path),
      });
  ASSERT_NO_FATAL_FAILURE(RunWake(0));
  ASSERT_TRUE(WaitForUpdaterExit());
  ASSERT_NO_FATAL_FAILURE(ExpectAppInstalled(kMsiAppId, kMsiUpdatedVersion));
  ASSERT_NO_FATAL_FAILURE(ExpectUninstallPing(test_server_.get()));
  ASSERT_NO_FATAL_FAILURE(Uninstall());
}

struct IntegrationInstallerResultsTestCase {
  const bool interactive_install;
  const std::string command_line_args;
  const UpdateService::ErrorCategory error_category;
  const int error_code;
  const std::string installer_text;
  const std::string installer_cmd_line;
  const std::string custom_app_response;
  const std::optional<bool> always_launch_cmd;
};

class IntegrationInstallerResultsTest
    : public ::testing::WithParamInterface<IntegrationInstallerResultsTestCase>,
      public IntegrationTestMsi {};

INSTANTIATE_TEST_SUITE_P(
    IntegrationInstallerResultsTestCases,
    IntegrationInstallerResultsTest,
    ::testing::ValuesIn(std::vector<IntegrationInstallerResultsTestCase>{
        // InstallerResult::kMsiError, explicit error code.
        {
            false,
            "INSTALLER_RESULT=2 INSTALLER_ERROR=1603",
            UpdateService::ErrorCategory::kInstaller,
            1603,
            "Installer error: Fatal error during installation. ",
            {},
            {},
        },

        // `InstallerResult::kCustomError`, implicit error code
        // `kErrorApplicationInstallerFailed`.
        {
            false,
            "INSTALLER_RESULT=1 INSTALLER_RESULT_UI_STRING=TestUIString",
            UpdateService::ErrorCategory::kInstaller,
            kErrorApplicationInstallerFailed,
            "TestUIString",
            {},
            {},
        },

        // InstallerResult::kSystemError, explicit error code.
        {
            false,
            "INSTALLER_RESULT=3 INSTALLER_ERROR=99",
            UpdateService::ErrorCategory::kInstaller,
            99,
            "Installer error: 0x63",
            {},
            {},
        },

        // InstallerResult::kSuccess.
        {
            false,
            "INSTALLER_RESULT=0",
            UpdateService::ErrorCategory::kNone,
            0,
            {},
            {},
            {},
        },

        // Silent install with a launch command, InstallerResult::kSuccess, will
        // not run `more.com` since silent install.
        {
            false,
            "INSTALLER_RESULT=0 "
            "REGISTER_LAUNCH_COMMAND=more.com",
            UpdateService::ErrorCategory::kNone,
            0,
            {},
            "more.com",
            {},
        },

        // InstallerResult::kMsiError, `ERROR_SUCCESS_REBOOT_REQUIRED`.
        {
            false,
            base::StrCat({"INSTALLER_RESULT=2 INSTALLER_ERROR=",
                          base::NumberToString(ERROR_SUCCESS_REBOOT_REQUIRED)}),
            UpdateService::ErrorCategory::kInstaller,
            ERROR_SUCCESS_REBOOT_REQUIRED,
            "Reboot required: The requested operation is successful. Changes "
            "will not be effective until the system is rebooted. ",
            {},
            {},
        },

        // InstallerResult::kMsiError, `ERROR_INSTALL_ALREADY_RUNNING`.
        {
            false,
            base::StrCat({"INSTALLER_RESULT=2 INSTALLER_ERROR=",
                          base::NumberToString(ERROR_INSTALL_ALREADY_RUNNING)}),
            UpdateService::ErrorCategory::kInstall,
            GOOPDATEINSTALL_E_INSTALL_ALREADY_RUNNING,
            "Installer error: Another installation is already in progress. "
            "Complete that installation before proceeding with this install. ",
            {},
            {},
        },

        // Interactive install via the command line with a launch command,
        // InstallerResult::kSuccess, will run `more.com` since interactive
        // install.
        {
            true,
            "INSTALLER_RESULT=0 "
            "REGISTER_LAUNCH_COMMAND=more.com",
            UpdateService::ErrorCategory::kNone,
            0,
            {},
            "more.com",
            {},
        },

        // Silent install with a launch command, with `always_launch_cmd` set to
        // `true`, InstallerResult::kSuccess, will run `more.com` even for
        // silent install.
        {
            false,
            "INSTALLER_RESULT=0 "
            "REGISTER_LAUNCH_COMMAND=more.com",
            UpdateService::ErrorCategory::kNone,
            0,
            {},
            "more.com",
            {},
            true,
        },

        // InstallerResult::kMsiError, `ERROR_SUCCESS_REBOOT_REQUIRED`.
        {
            true,
            base::StrCat({"INSTALLER_RESULT=2 INSTALLER_ERROR=",
                          base::NumberToString(ERROR_SUCCESS_REBOOT_REQUIRED)}),
            UpdateService::ErrorCategory::kInstaller,
            ERROR_SUCCESS_REBOOT_REQUIRED,
            base::WideToUTF8(GetLocalizedStringF(IDS_TEXT_RESTART_COMPUTER_BASE,
                                                 L"")),
            {},
            {},
        },

        // Interactive install via the command line,
        // `update_client::ProtocolError::UNKNOWN_APPLICATION` error.
        {
            true,
            "INSTALLER_RESULT=0",
            UpdateService::ErrorCategory::kInstall,
            static_cast<int>(update_client::ProtocolError::UNKNOWN_APPLICATION),
            base::WideToUTF8(GetLocalizedString(IDS_UNKNOWN_APPLICATION_BASE)),
            {},
            base::StrCat({"{\"appid\":\"", IntegrationTestMsi::kMsiAppId,
                          "\",\"status\":\"error-unknownApplication\"}"}),
        },

        // Interactive install via the command line,
        // `update_client::ProtocolError::OS_NOT_SUPPORTED` error.
        {
            true,
            "INSTALLER_RESULT=0",
            UpdateService::ErrorCategory::kInstall,
            static_cast<int>(update_client::ProtocolError::OS_NOT_SUPPORTED),
            base::WideToUTF8(GetLocalizedString(IDS_OS_NOT_SUPPORTED_BASE)),
            {},
            base::StrCat({"{\"appid\":\"", IntegrationTestMsi::kMsiAppId,
                          "\",\"status\":\"error-osnotsupported\"}"}),
        },

        // Interactive install via the command line,
        // `update_client::ProtocolError::HW_NOT_SUPPORTED` error.
        {
            true,
            "INSTALLER_RESULT=0",
            UpdateService::ErrorCategory::kInstall,
            static_cast<int>(update_client::ProtocolError::HW_NOT_SUPPORTED),
            base::WideToUTF8(GetLocalizedString(IDS_HW_NOT_SUPPORTED_BASE)),
            {},
            base::StrCat({"{\"appid\":\"", IntegrationTestMsi::kMsiAppId,
                          "\",\"status\":\"error-hwnotsupported\"}"}),
        },

        // Interactive install via the command line,
        // `update_client::ProtocolError::NO_HASH` error.
        {
            true,
            "INSTALLER_RESULT=0",
            UpdateService::ErrorCategory::kInstall,
            static_cast<int>(update_client::ProtocolError::NO_HASH),
            base::WideToUTF8(GetLocalizedString(IDS_NO_HASH_BASE)),
            {},
            base::StrCat({"{\"appid\":\"", IntegrationTestMsi::kMsiAppId,
                          "\",\"status\":\"error-hash\"}"}),
        },

        // Interactive install via the command line,
        // `update_client::ProtocolError::UNSUPPORTED_PROTOCOL` error.
        {
            true,
            "INSTALLER_RESULT=0",
            UpdateService::ErrorCategory::kInstall,
            static_cast<int>(
                update_client::ProtocolError::UNSUPPORTED_PROTOCOL),
            base::WideToUTF8(GetLocalizedString(IDS_UNSUPPORTED_PROTOCOL_BASE)),
            {},
            base::StrCat({"{\"appid\":\"", IntegrationTestMsi::kMsiAppId,
                          "\",\"status\":\"error-unsupportedprotocol\"}"}),
        },

        // Interactive install via the command line,
        // `update_client::ProtocolError::INTERNAL` error.
        {
            true,
            "INSTALLER_RESULT=0",
            UpdateService::ErrorCategory::kInstall,
            static_cast<int>(update_client::ProtocolError::INTERNAL),
            base::WideToUTF8(GetLocalizedString(IDS_INTERNAL_BASE)),
            {},
            base::StrCat({"{\"appid\":\"", IntegrationTestMsi::kMsiAppId,
                          "\",\"status\":\"error-internal\"}"}),
        },
    }));

TEST_P(IntegrationInstallerResultsTest, TestCases) {
  const base::FilePath crx_relative_path = GetInstallerPath(kMsiCrx);
  const bool should_install_successfully =
      !GetParam().error_code ||
      GetParam().error_code == ERROR_SUCCESS_REBOOT_REQUIRED;
  const bool always_launch_cmd = GetParam().always_launch_cmd.value_or(false);

  if (!GetParam().interactive_install && !always_launch_cmd) {
    ASSERT_NO_FATAL_FAILURE(Install());
    ASSERT_NO_FATAL_FAILURE(ExpectInstalled());
  }

  ExpectAppsUpdateSequence(
      UpdaterScope::kSystem, test_server_.get(),
      /*request_attributes=*/{},
      {
          AppUpdateExpectation(
              GetParam().command_line_args, kMsiAppId,
              base::Version({0, 0, 0, 0}), kMsiUpdatedVersion,
              /*is_install=*/true, should_install_successfully, false, "", "",
              crx_relative_path,
              /*always_serve_crx=*/GetParam().custom_app_response.empty(),
              GetParam().error_category, GetParam().error_code,
              /*EVENT_INSTALL_COMPLETE=*/2, GetParam().custom_app_response),
      });
  ASSERT_NO_FATAL_FAILURE(ExpectUninstallPing(test_server_.get()));

  if (GetParam().interactive_install || always_launch_cmd) {
    ASSERT_NO_FATAL_FAILURE(InstallUpdaterAndApp(
        kMsiAppId, !GetParam().interactive_install, "usagestats=1",
        GetParam().installer_text, always_launch_cmd));
    ASSERT_TRUE(WaitForUpdaterExit());
  } else {
    int64_t crx_file_size = 0;
    {
      base::FilePath exe_path;
      ASSERT_TRUE(base::PathService::Get(base::DIR_EXE, &exe_path));
      const base::FilePath crx_path = exe_path.Append(crx_relative_path);
      EXPECT_TRUE(base::GetFileSize(crx_path, &crx_file_size));
    }
    ASSERT_NO_FATAL_FAILURE(InstallAppViaService(
        kMsiAppId,
        base::Value::Dict()
            .Set("expected_update_state",
                 base::Value::Dict()
                     .Set("app_id", kMsiAppId)
                     .Set("state",
                          static_cast<int>(
                              should_install_successfully
                                  ? UpdateService::UpdateState::State::kUpdated
                                  : UpdateService::UpdateState::State::
                                        kUpdateError))
                     .Set("next_version", kMsiUpdatedVersion.GetString())
                     .Set("downloaded_bytes", static_cast<int>(crx_file_size))
                     .Set("total_bytes", static_cast<int>(crx_file_size))
                     .Set("install_progress", -1)
                     .Set("error_category",
                          should_install_successfully
                              ? 0
                              : static_cast<int>(GetParam().error_category))
                     .Set("error_code", GetParam().error_code)
                     .Set("extra_code1", 0)
                     .Set("installer_text", GetParam().installer_text)
                     .Set("installer_cmd_line", GetParam().installer_cmd_line))
            .Set("expected_result", 0)));
  }

  if (should_install_successfully) {
    ExpectAppInstalled(kMsiAppId, kMsiUpdatedVersion);
    if (!GetParam().installer_cmd_line.empty()) {
      const std::wstring post_install_launch_command_line =
          base::ASCIIToWide(GetParam().installer_cmd_line);
      EXPECT_EQ(test::IsProcessRunning(post_install_launch_command_line),
                GetParam().interactive_install || always_launch_cmd);
      EXPECT_TRUE(test::KillProcesses(post_install_launch_command_line, 0));
    }
    ASSERT_NO_FATAL_FAILURE(Uninstall());
  } else {
    ASSERT_NO_FATAL_FAILURE(ExpectNotRegistered(kMsiAppId));

    // Wait for the updater to uninstall itself automatically since the app
    // failed to install, and there are now no apps to manage.
    ASSERT_TRUE(WaitForUpdaterExit());
  }
}

TEST_P(IntegrationInstallerResultsTest, OnDemandTestCases) {
  if (GetParam().interactive_install) {
    GTEST_SKIP();
  }

  const base::FilePath crx_relative_path = GetInstallerPath(kMsiCrx);
  const bool should_install_successfully =
      !GetParam().error_code ||
      GetParam().error_code == ERROR_SUCCESS_REBOOT_REQUIRED;

  ASSERT_NO_FATAL_FAILURE(Install());
  ASSERT_NO_FATAL_FAILURE(InstallApp(kMsiAppId, base::Version({0, 0, 0, 0})));

  ASSERT_NO_FATAL_FAILURE(ExpectUpdateCheckSequence(
      test_server_.get(), kMsiAppId, UpdateService::Priority::kForeground,
      base::Version({0, 0, 0, 0}), kMsiUpdatedVersion));

  ExpectAppsUpdateSequence(
      UpdaterScope::kSystem, test_server_.get(),
      /*request_attributes=*/{},
      {
          AppUpdateExpectation(
              GetParam().command_line_args, kMsiAppId,
              base::Version({0, 0, 0, 0}), kMsiUpdatedVersion,
              /*is_install=*/false, should_install_successfully, false, "", "",
              crx_relative_path,
              /*always_serve_crx=*/GetParam().custom_app_response.empty(),
              GetParam().error_category, GetParam().error_code,
              /*EVENT_UPDATE_COMPLETE=*/3, GetParam().custom_app_response),
      });
  ASSERT_NO_FATAL_FAILURE(ExpectUninstallPing(test_server_.get()));

  ASSERT_NO_FATAL_FAILURE(ExpectLegacyUpdate3WebSucceeds(
      kMsiAppId, AppBundleWebCreateMode::kCreateInstalledApp,
      should_install_successfully ? STATE_INSTALL_COMPLETE : STATE_ERROR,
      GetParam().error_code));
  ASSERT_NO_FATAL_FAILURE(Uninstall());
}

class IntegrationInstallerResultsTestNewInstalls : public IntegrationTestMsi {};

TEST_F(IntegrationInstallerResultsTestNewInstalls, OnDemandCancel) {
  // Delay download a bit to allow cancellation.
  test_server_->set_download_delay(base::Seconds(5));

  const base::FilePath crx_relative_path = GetInstallerPath(kMsiCrx);

  ASSERT_NO_FATAL_FAILURE(Install());
  ASSERT_NO_FATAL_FAILURE(InstallApp(kMsiAppId, base::Version({0, 0, 0, 0})));

  ASSERT_NO_FATAL_FAILURE(ExpectUpdateCheckSequence(
      test_server_.get(), kMsiAppId, UpdateService::Priority::kForeground,
      base::Version({0, 0, 0, 0}), kMsiUpdatedVersion));

  ExpectAppsUpdateSequence(
      UpdaterScope::kSystem, test_server_.get(),
      /*request_attributes=*/{},
      {
          AppUpdateExpectation(
              "INSTALLER_RESULT=0", kMsiAppId, base::Version({0, 0, 0, 0}),
              kMsiUpdatedVersion,
              /*is_install=*/false, /*should_update=*/false,
              /*allow_rollback=*/false,
              /*target_version_prefix=*/{}, /*target_channel=*/{},
              crx_relative_path,
              /*always_serve_crx=*/true, UpdateService::ErrorCategory::kService,
              static_cast<int>(update_client::ServiceError::CANCELLED),
              /*EVENT_INSTALL_COMPLETE=*/2, {}),
      });
  ASSERT_NO_FATAL_FAILURE(ExpectUninstallPing(test_server_.get()));

  ASSERT_NO_FATAL_FAILURE(ExpectLegacyUpdate3WebSucceeds(
      kMsiAppId, AppBundleWebCreateMode::kCreateApp, STATE_ERROR,
      static_cast<int>(update_client::ServiceError::CANCELLED),
      /*cancel_when_downloading=*/true));
  ASSERT_NO_FATAL_FAILURE(Uninstall());
}

TEST_F(IntegrationTestDeviceManagement, NamedProxy) {
  ASSERT_NO_FATAL_FAILURE(InstallBrokenEnterpriseCompanionApp());
  ASSERT_NO_FATAL_FAILURE(Install());
  ASSERT_NO_FATAL_FAILURE(InstallTestApp(kApp1, /*install_v1=*/false));

  ASSERT_NO_FATAL_FAILURE(ExpectInstalled());
  ASSERT_NO_FATAL_FAILURE(ExpectAppInstalled(kApp1.appid, kApp1.v2));

  // Fetch proxy settings policy.
  DMPushEnrollmentToken(kEnrollmentToken);
  ExpectDeviceManagementRegistrationRequest(test_server_.get(),
                                            kEnrollmentToken, kDMToken);
  OmahaSettingsClientProto omaha_settings;
  omaha_settings.set_proxy_mode("fixed_servers");
  omaha_settings.set_proxy_server(
      base::StrCat({test_server_->proxy_url_no_path(), ";DIRECT"}));
  ExpectDeviceManagementPolicyFetchRequest(test_server_.get(), kDMToken,
                                           omaha_settings);
  ASSERT_NO_FATAL_FAILURE(
      ExpectNoUpdateSequence(test_server_.get(), kApp1.appid));
  ASSERT_NO_FATAL_FAILURE(RunWake(0));
  ASSERT_TRUE(WaitForUpdaterExit());

  // Redirect network traffic to remote hosts to engage the proxy.
  const GURL update_check_url = GURL("http://update.server.not_exist/update");
  const GURL dm_server_url = GURL("http://dm.server.not_exist/dmapi");
  EnterTestMode(update_check_url, test_server_->crash_upload_url(),
                dm_server_url, {}, base::Minutes(5));
  ExpectDeviceManagementPolicyFetchRequest(test_server_.get(), kDMToken,
                                           omaha_settings, false, false,
                                           dm_server_url);
  ASSERT_NO_FATAL_FAILURE(RunWake(0));
  ASSERT_TRUE(WaitForUpdaterExit());
  ASSERT_NO_FATAL_FAILURE(
      ExpectUninstallPing(test_server_.get(), update_check_url));
  ASSERT_NO_FATAL_FAILURE(UninstallApp(kApp1.appid));
  ASSERT_NO_FATAL_FAILURE(UninstallBrokenEnterpriseCompanionApp());
  ASSERT_NO_FATAL_FAILURE(Uninstall());
}

TEST_F(IntegrationTestDeviceManagement, PacScript) {
  ASSERT_NO_FATAL_FAILURE(InstallBrokenEnterpriseCompanionApp());
  ASSERT_NO_FATAL_FAILURE(Install());
  ASSERT_NO_FATAL_FAILURE(InstallTestApp(kApp1, /*install_v1=*/false));

  ASSERT_NO_FATAL_FAILURE(ExpectInstalled());
  ASSERT_NO_FATAL_FAILURE(ExpectAppInstalled(kApp1.appid, kApp1.v2));

  // Fetch proxy settings policy.
  DMPushEnrollmentToken(kEnrollmentToken);
  ExpectDeviceManagementRegistrationRequest(test_server_.get(),
                                            kEnrollmentToken, kDMToken);
  OmahaSettingsClientProto omaha_settings;
  omaha_settings.set_proxy_mode("pac_script");
  omaha_settings.set_proxy_pac_url(test_server_->proxy_pac_url().spec());
  ExpectDeviceManagementPolicyFetchRequest(test_server_.get(), kDMToken,
                                           omaha_settings);
  ExpectProxyPacScriptRequest(test_server_.get());
  ASSERT_NO_FATAL_FAILURE(
      ExpectNoUpdateSequence(test_server_.get(), kApp1.appid));
  ASSERT_NO_FATAL_FAILURE(RunWake(0));
  ASSERT_TRUE(WaitForUpdaterExit());

  // Redirect network traffic to remote hosts to engage the proxy.
  // Note the test server won't receive additional PAC script download requests
  // because Windows caches it.
  const GURL update_check_url = GURL("http://update.server.not_exist/update");
  const GURL dm_server_url = GURL("http://dm.server.not_exist2/dmapi");
  EnterTestMode(update_check_url, test_server_->crash_upload_url(),
                dm_server_url, {}, base::Minutes(5));
  ExpectDeviceManagementPolicyFetchRequest(test_server_.get(), kDMToken,
                                           omaha_settings, false, false,
                                           dm_server_url);
  ASSERT_NO_FATAL_FAILURE(RunWake(0));
  ASSERT_TRUE(WaitForUpdaterExit());
  ASSERT_NO_FATAL_FAILURE(ExpectUninstallPing(test_server_.get()));
  ASSERT_NO_FATAL_FAILURE(UninstallApp(kApp1.appid));
  ASSERT_NO_FATAL_FAILURE(UninstallBrokenEnterpriseCompanionApp());
  ASSERT_NO_FATAL_FAILURE(Uninstall());
}
#endif  // BUILDFLAG(IS_WIN)

}  // namespace updater::test
