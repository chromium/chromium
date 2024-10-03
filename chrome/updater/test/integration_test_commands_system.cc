// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <cstdlib>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "base/base_paths.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/json/json_writer.h"
#include "base/memory/scoped_refptr.h"
#include "base/notreached.h"
#include "base/path_service.h"
#include "base/strings/string_number_conversions.h"
#include "base/test/test_switches.h"
#include "base/time/time.h"
#include "base/values.h"
#include "base/version.h"
#include "build/build_config.h"
#include "chrome/updater/constants.h"
#include "chrome/updater/persisted_data.h"
#include "chrome/updater/prefs.h"
#include "chrome/updater/registration_data.h"
#include "chrome/updater/test/integration_test_commands.h"
#include "chrome/updater/test/integration_tests_impl.h"
#include "chrome/updater/update_service.h"
#include "chrome/updater/updater_scope.h"
#include "chrome/updater/util/util.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

#if BUILDFLAG(IS_WIN)
#include "base/strings/utf_string_conversions.h"
#endif  // BUILDFLAG(IS_WIN)

namespace updater::test {

namespace {

std::string StringFromValue(const base::Value& value) {
  std::string value_string;
  EXPECT_TRUE(base::JSONWriter::Write(value, &value_string));
  return value_string;
}

std::string BoolToString(const bool value) {
  return value ? "true" : "false";
}

std::string RegistrationRequestToString(
    const RegistrationRequest& registration) {
  base::Value::Dict value;
  value.Set("app_id", registration.app_id);
  value.Set("brand_code", registration.brand_code);
  value.Set("brand_path", registration.brand_path.MaybeAsASCII());
  value.Set("ap", registration.ap);
  value.Set("ap_path", registration.ap_path.MaybeAsASCII());
  value.Set("ap_key", registration.ap_key);
  value.Set("version", registration.version.GetString());
  value.Set("version_path", registration.version_path.MaybeAsASCII());
  value.Set("version_key", registration.version_key);
  value.Set("existence_checker_path",
            registration.existence_checker_path.MaybeAsASCII());
  value.Set("cohort", registration.cohort);
  value.Set("cohort_name", registration.cohort_name);
  value.Set("cohort_hint", registration.cohort_hint);
  return StringFromValue(base::Value(value.Clone()));
}

}  // namespace

class IntegrationTestCommandsSystem : public IntegrationTestCommands {
 public:
  explicit IntegrationTestCommandsSystem(UpdaterScope scope)
      : updater_scope_(scope) {}

  void ExpectNoCrashes() const override {
    updater::test::ExpectNoCrashes(updater_scope_);
  }

  void PrintLog() const override { RunCommand("print_log"); }

  void CopyLog(const std::string& infix) const override {
    const std::optional<base::FilePath> path =
        GetInstallDirectory(updater_scope_);
    ASSERT_TRUE(path);
    if (path) {
      updater::test::CopyLog(*path, infix);
    }
  }

  void Clean() const override {
    RunCommand("clean");
    updater::test::Clean(UpdaterScope::kUser);
  }

  void ExpectClean() const override {
    RunCommand("expect_clean");
    updater::test::ExpectClean(UpdaterScope::kUser);
  }

  void Install(const base::Value::List& switches) const override {
    RunCommand(
        "install",
        {Param("switches", StringFromValue(base::Value(switches.Clone())))});
  }

  void InstallUpdaterAndApp(const std::string& app_id,
                            const bool is_silent_install,
                            const std::string& tag,
                            const std::string& child_window_text_to_find,
                            const bool always_launch_cmd,
                            const bool verify_app_logo_loaded,
                            const bool expect_success,
                            const bool wait_for_the_installer) const override {
    RunCommand(
        "install_updater_and_app",
        {
            Param("app_id", app_id),
            Param("is_silent_install", BoolToString(is_silent_install)),
            Param("tag", tag),
            Param("child_window_text_to_find", child_window_text_to_find),
            Param("always_launch_cmd", BoolToString(always_launch_cmd)),
            Param("verify_app_logo_loaded",
                  BoolToString(verify_app_logo_loaded)),
            Param("expect_success", BoolToString(expect_success)),
            Param("wait_for_the_installer",
                  BoolToString(wait_for_the_installer)),
        });
  }

  void ExpectInstalled() const override { RunCommand("expect_installed"); }

  void Uninstall() const override { RunCommand("uninstall"); }

  void ExpectCandidateUninstalled() const override {
    RunCommand("expect_candidate_uninstalled");
  }

  void EnterTestMode(const GURL& update_url,
                     const GURL& crash_upload_url,
                     const GURL& device_management_url,
                     const GURL& app_logo_url,
                     base::TimeDelta idle_timeout,
                     base::TimeDelta server_keep_alive_time,
                     base::TimeDelta ceca_connection_timeout) const override {
    RunCommand(
        "enter_test_mode",
        {Param("update_url", update_url.spec()),
         Param("crash_upload_url", crash_upload_url.spec()),
         Param("device_management_url", device_management_url.spec()),
         Param("app_logo_url", app_logo_url.spec()),
         Param("idle_timeout", base::NumberToString(idle_timeout.InSeconds())),
         Param("server_keep_alive_time",
               base::NumberToString(server_keep_alive_time.InSeconds())),
         Param("ceca_connection_timeout",
               base::NumberToString(ceca_connection_timeout.InSeconds()))});
  }

  void ExitTestMode() const override { RunCommand("exit_test_mode"); }

  void SetGroupPolicies(const base::Value::Dict& values) const override {
    RunCommand("set_group_policies",
               {Param("values", StringFromValue(base::Value(values.Clone())))});
  }

  void SetPlatformPolicies(const base::Value::Dict& values) const override {
    RunCommand("set_platform_policies",
               {Param("values", StringFromValue(base::Value(values.Clone())))});
  }

  void SetMachineManaged(bool is_managed_device) const override {
    RunCommand("set_machine_managed",
               {Param("managed", BoolToString(is_managed_device))});
  }

  void ExpectSelfUpdateSequence(ScopedServer* test_server) const override {
    updater::test::ExpectSelfUpdateSequence(updater_scope_, test_server);
  }

  void ExpectPing(ScopedServer* test_server,
                  int event_type,
                  std::optional<GURL> target_url) const override {
    updater::test::ExpectPing(updater_scope_, test_server, event_type,
                              target_url);
  }

  void ExpectAppCommandPing(ScopedServer* test_server,
                            const std::string& appid,
                            const std::string& appcommandid,
                            int errorcode,
                            int eventresult,
                            int event_type,
                            const base::Version& version) const override {
    updater::test::ExpectAppCommandPing(updater_scope_, test_server, appid,
                                        appcommandid, errorcode, eventresult,
                                        event_type, version);
  }

  void ExpectUpdateCheckRequest(ScopedServer* test_server) const override {
    updater::test::ExpectUpdateCheckRequest(updater_scope_, test_server);
  }

  void ExpectUpdateCheckSequence(
      ScopedServer* test_server,
      const std::string& app_id,
      UpdateService::Priority priority,
      const base::Version& from_version,
      const base::Version& to_version) const override {
    updater::test::ExpectUpdateCheckSequence(updater_scope_, test_server,
                                             app_id, priority, from_version,
                                             to_version);
  }

  void ExpectUpdateSequence(ScopedServer* test_server,
                            const std::string& app_id,
                            const std::string& install_data_index,
                            UpdateService::Priority priority,
                            const base::Version& from_version,
                            const base::Version& to_version,
                            bool do_fault_injection) const override {
    updater::test::ExpectUpdateSequence(
        updater_scope_, test_server, app_id, install_data_index, priority,
        from_version, to_version, do_fault_injection);
  }

  void ExpectUpdateSequenceBadHash(
      ScopedServer* test_server,
      const std::string& app_id,
      const std::string& install_data_index,
      UpdateService::Priority priority,
      const base::Version& from_version,
      const base::Version& to_version) const override {
    updater::test::ExpectUpdateSequenceBadHash(
        updater_scope_, test_server, app_id, install_data_index, priority,
        from_version, to_version);
  }

  void ExpectInstallSequence(ScopedServer* test_server,
                             const std::string& app_id,
                             const std::string& install_data_index,
                             UpdateService::Priority priority,
                             const base::Version& from_version,
                             const base::Version& to_version,
                             bool do_fault_injection) const override {
    updater::test::ExpectInstallSequence(
        updater_scope_, test_server, app_id, install_data_index, priority,
        from_version, to_version, do_fault_injection);
  }

  void ExpectVersionActive(const std::string& version) const override {
    RunCommand("expect_version_active", {Param("updater_version", version)});
  }

  void ExpectVersionNotActive(const std::string& version) const override {
    RunCommand("expect_version_not_active",
               {Param("updater_version", version)});
  }

  void ExpectActive(const std::string& app_id) const override {
    updater::test::ExpectActive(updater_scope_, app_id);
  }

  void ExpectNotActive(const std::string& app_id) const override {
    updater::test::ExpectNotActive(updater_scope_, app_id);
  }

  void SetupFakeUpdaterHigherVersion() const override {
    RunCommand("setup_fake_updater_higher_version");
  }

  void SetupFakeUpdaterLowerVersion() const override {
    RunCommand("setup_fake_updater_lower_version");
  }

  void SetupRealUpdaterLowerVersion() const override {
    RunCommand("setup_real_updater_lower_version");
  }

  void SetExistenceCheckerPath(const std::string& app_id,
                               const base::FilePath& path) const override {
    RunCommand("set_existence_checker_path",
               {Param("app_id", app_id), Param("path", path.MaybeAsASCII())});
  }

  void SetServerStarts(int value) const override {
    RunCommand("set_first_registration_counter",
               {Param("value", base::NumberToString(value))});
  }

  void FillLog() const override { RunCommand("fill_log"); }

  void ExpectLogRotated() const override { RunCommand("expect_log_rotated"); }

  void ExpectRegistered(const std::string& app_id) const override {
    RunCommand("expect_registered", {Param("app_id", app_id)});
  }

  void ExpectNotRegistered(const std::string& app_id) const override {
    RunCommand("expect_not_registered", {Param("app_id", app_id)});
  }

  void ExpectAppTag(const std::string& app_id,
                    const std::string& tag) const override {
    RunCommand("expect_app_tag", {Param("app_id", app_id), Param("tag", tag)});
  }

  void SetAppTag(const std::string& app_id,
                 const std::string& tag) const override {
    RunCommand("set_app_tag", {Param("app_id", app_id), Param("tag", tag)});
  }

  void ExpectAppVersion(const std::string& app_id,
                        const base::Version& version) const override {
    RunCommand(
        "expect_app_version",
        {Param("app_id", app_id), Param("app_version", version.GetString())});
  }

  void SetActive(const std::string& app_id) const override {
    updater::test::SetActive(updater_scope_, app_id);
  }

  void RunWake(int expected_exit_code) const override {
    RunCommand("run_wake",
               {Param("exit_code", base::NumberToString(expected_exit_code))});
  }

  void RunWakeAll() const override { RunCommand("run_wake_all", {}); }

  void RunWakeActive(int expected_exit_code) const override {
    RunCommand("run_wake_active",
               {Param("exit_code", base::NumberToString(expected_exit_code))});
  }

  void RunCrashMe() const override { RunCommand("run_crash_me", {}); }

  void RunServer(int expected_exit_code, bool internal) const override {
    RunCommand("run_server",
               {Param("internal", BoolToString(internal)),
                Param("exit_code", base::NumberToString(expected_exit_code))});
  }

  void RegisterApp(const RegistrationRequest& registration) const override {
    RunCommand(
        "register_app",
        {Param("registration", RegistrationRequestToString(registration))});
  }

  void CheckForUpdate(const std::string& app_id) const override {
    RunCommand("check_for_update", {Param("app_id", app_id)});
  }

  void Update(const std::string& app_id,
              const std::string& install_data_index) const override {
    RunCommand("update", {Param("app_id", app_id),
                          Param("install_data_index", install_data_index)});
  }

  void UpdateAll() const override { RunCommand("update_all", {}); }

  void GetAppStates(
      const base::Value::Dict& expected_app_states) const override {
    RunCommand(
        "get_app_states",
        {Param("expected_app_states",
               StringFromValue(base::Value(expected_app_states.Clone())))});
  }

  void DeleteUpdaterDirectory() const override {
    RunCommand("delete_updater_directory", {});
  }

  void DeleteActiveUpdaterExecutable() const override {
    RunCommand("delete_active_updater_executable", {});
  }

  void DeleteFile(const base::FilePath& path) const override {
    RunCommand("delete_file", {Param("path", path.MaybeAsASCII())});
  }

  void InstallApp(const std::string& app_id,
                  const base::Version& version) const override {
    RunCommand("install_app", {Param("app_id", app_id),
                               Param("app_version", version.GetString())});
  }

#if BUILDFLAG(IS_WIN)
  void ExpectInterfacesRegistered() const override {
    RunCommand("expect_interfaces_registered");
  }

  void ExpectMarshalInterfaceSucceeds() const override {
    RunCommand("expect_marshal_interface_succeeds");
  }

  void ExpectLegacyUpdate3WebSucceeds(
      const std::string& app_id,
      AppBundleWebCreateMode app_bundle_web_create_mode,
      int expected_final_state,
      int expected_error_code,
      bool cancel_when_downloading) const override {
    RunCommand("expect_legacy_update3web_succeeds",
               {Param("app_id", app_id),
                Param("app_bundle_web_create_mode",
                      base::NumberToString(
                          static_cast<int>(app_bundle_web_create_mode))),
                Param("expected_final_state",
                      base::NumberToString(expected_final_state)),
                Param("expected_error_code",
                      base::NumberToString(expected_error_code)),
                Param("cancel_when_downloading",
                      BoolToString(cancel_when_downloading))});
  }

  void ExpectLegacyProcessLauncherSucceeds() const override {
    RunCommand("expect_legacy_process_launcher_succeeds");
  }

  void ExpectLegacyAppCommandWebSucceeds(
      const std::string& app_id,
      const std::string& command_id,
      const base::Value::List& parameters,
      int expected_exit_code) const override {
    RunCommand(
        "expect_legacy_app_command_web_succeeds",
        {Param("app_id", app_id), Param("command_id", command_id),
         Param("parameters", StringFromValue(base::Value(parameters.Clone()))),
         Param("expected_exit_code",
               base::NumberToString(expected_exit_code))});
  }

  void ExpectLegacyPolicyStatusSucceeds() const override {
    RunCommand("expect_legacy_policy_status_succeeds");
  }

  void RunUninstallCmdLine() const override {
    RunCommand("run_uninstall_cmd_line");
  }

  void RunHandoff(const std::string& app_id) const override {
    RunCommand("run_handoff", {Param("app_id", app_id)});
  }
#endif  // BUILDFLAG(IS_WIN)

  void InstallAppViaService(
      const std::string& app_id,
      const base::Value::Dict& expected_final_values) const override {
    RunCommand(
        "install_app_via_service",
        {Param("app_id", app_id),
         Param("expected_final_values",
               StringFromValue(base::Value(expected_final_values.Clone())))});
  }

  base::FilePath GetDifferentUserPath() const override {
    // On POSIX, the path may be chowned; so do not use a file not owned by the
    // test, nor the test executable itself.
    NOTREACHED_IN_MIGRATION() << __func__ << ": not implemented.";
    return base::FilePath();
  }

  void StressUpdateService() const override {
    RunCommand("stress_update_service");
  }

  void CallServiceUpdate(const std::string& app_id,
                         const std::string& install_data_index,
                         UpdateService::PolicySameVersionUpdate
                             policy_same_version_update) const override {
    RunCommand("call_service_update",
               {Param("app_id", app_id),
                Param("install_data_index", install_data_index),
                Param("same_version_update_allowed",
                      BoolToString(
                          policy_same_version_update ==
                          UpdateService::PolicySameVersionUpdate::kAllowed))});
  }

  void SetupFakeLegacyUpdater() const override {
    RunCommand("setup_fake_legacy_updater");
  }

#if BUILDFLAG(IS_WIN)
  void RunFakeLegacyUpdater() const override {
    RunCommand("run_fake_legacy_updater");
  }
#endif  // BUILDFLAG(IS_WIN)

#if BUILDFLAG(IS_MAC)
  void PrivilegedHelperInstall() const override {
    RunCommand("privileged_helper_install");
  }

  void DeleteLegacyUpdater() const override {
    RunCommand("delete_legacy_updater");
  }

  void ExpectPrepareToRunBundleSuccess(
      const base::FilePath& bundle_path) const override {
    RunCommand("expect_prepare_to_run_bundle_success",
               {Param("bundle_path", bundle_path.MaybeAsASCII())});
  }

  void ExpectKSAdminFetchTag(
      bool elevate,
      const std::string& product_id,
      const base::FilePath& xc_path,
      std::optional<UpdaterScope> store_flag,
      std::optional<std::string> want_tag) const override {
    updater::test::ExpectKSAdminFetchTag(updater_scope_, elevate, product_id,
                                         xc_path, store_flag, want_tag);
  }
#endif  // BUILDFLAG(IS_MAC)

  void ExpectLegacyUpdaterMigrated() const override {
    RunCommand("expect_legacy_updater_migrated");
  }

  void RunRecoveryComponent(const std::string& app_id,
                            const base::Version& version) const override {
    RunCommand("run_recovery_component",
               {Param("app_id", app_id),
                Param("browser_version", version.GetString())});
  }

  void SetLastChecked(const base::Time& time) const override {
    RunCommand(
        "set_last_checked",
        {Param("time", base::NumberToString(
                           time.InMillisecondsFSinceUnixEpochIgnoringNull()))});
  }

  void ExpectLastChecked() const override { RunCommand("expect_last_checked"); }

  void ExpectLastStarted() const override { RunCommand("expect_last_started"); }

  void UninstallApp(const std::string& app_id) const override {
    RunCommand("uninstall_app", {Param("app_id", app_id)});
  }

  void RunOfflineInstall(bool is_legacy_install,
                         bool is_silent_install) override {
    RunCommand("run_offline_install",
               {Param("legacy_install", BoolToString(is_legacy_install)),
                Param("silent", BoolToString(is_silent_install))});
  }

  void RunOfflineInstallOsNotSupported(bool is_legacy_install,
                                       bool is_silent_install) override {
    RunCommand("run_offline_install_os_not_supported",
               {Param("legacy_install", BoolToString(is_legacy_install)),
                Param("silent", BoolToString(is_silent_install))});
  }

  void DMPushEnrollmentToken(const std::string& enrollment_token) override {
    RunCommand("dm_push_enrollment_token",
               {Param("enrollment_token", enrollment_token)});
  }
  void DMDeregisterDevice() override { RunCommand("dm_deregister_device"); }
  void DMCleanup() override { RunCommand("dm_cleanup"); }
  void InstallEnterpriseCompanionApp() override {
    RunCommand("install_enterprise_companion_app");
  }
  void InstallBrokenEnterpriseCompanionApp() override {
    RunCommand("install_broken_enterprise_companion_app");
  }
  void UninstallBrokenEnterpriseCompanionApp() override {
    RunCommand("uninstall_broken_enterprise_companion_app");
  }
  void InstallEnterpriseCompanionAppOverrides(
      const base::Value::Dict& external_overrides) override {
    RunCommand(
        "install_enterprise_companion_app_overrides",
        {Param("external_overrides",
               StringFromValue(base::Value(external_overrides.Clone())))});
  }
  void ExpectEnterpriseCompanionAppNotInstalled() override {
    RunCommand("expect_enterprise_companion_app_not_installed");
  }
  void UninstallEnterpriseCompanionApp() override {
    RunCommand("uninstall_enterprise_companion_app");
  }

 private:
  ~IntegrationTestCommandsSystem() override = default;

  struct Param {
    Param(const std::string& name, const std::string& value)
        : name(name), value(value) {}
    std::string name;
    std::string value;
  };

  // Invokes the test helper command by running a unit test from the
  // "updater_integration_tests_helper" program. The program returns 0 if
  // the unit test passes.
  void RunCommand(const std::string& command_switch,
                  const std::vector<Param>& params) const {
    const base::CommandLine command_line =
        *base::CommandLine::ForCurrentProcess();
    base::FilePath path(command_line.GetProgram());
#if !BUILDFLAG(IS_WIN)
    // Check the presence of the program on non-Windows platform only, because
    // on Windows the program may run without extension.
    EXPECT_TRUE(base::PathExists(path));
#endif
    path = path.DirName();
    EXPECT_TRUE(base::PathExists(path));
    path = MakeAbsoluteFilePath(path);
    path = path.Append(FILE_PATH_LITERAL("updater_integration_tests_helper"));
#if BUILDFLAG(IS_WIN)
    path = path.AddExtension(L"exe");
#endif
    EXPECT_TRUE(base::PathExists(path));

    base::CommandLine helper_command(path);
    helper_command.AppendSwitch(command_switch);
    for (const Param& param : params) {
      helper_command.AppendSwitchASCII(param.name, param.value);
    }

    // Avoids the test runner banner about test debugging.
    helper_command.AppendSwitch("single-process-tests");
    helper_command.AppendSwitchASCII("gtest_filter",
                                     "TestHelperCommandRunner.Run");
    helper_command.AppendSwitchASCII("gtest_brief", "1");
    for (const std::string& s :
         {switches::kUiTestActionTimeout, switches::kUiTestActionMaxTimeout,
          switches::kTestTinyTimeout, switches::kTestLauncherTimeout}) {
      if (command_line.HasSwitch(s)) {
        helper_command.AppendSwitchNative(s,
                                          command_line.GetSwitchValueNative(s));
      }
    }

    int exit_code = -1;
    Run(updater_scope_, helper_command, &exit_code);

    // A failure here indicates that the integration test helper
    // process ran but the invocation of the test helper command was not
    // successful for a number of reasons.
    // If the `exit_code` is 1 then there were failed assertions in
    // the code invoked by the test command. This is the most common case.
    // Other exit codes mean that the helper command is not defined or the
    // helper command line syntax is wrong for some reason.
    ASSERT_EQ(exit_code, 0);
  }

  void RunCommand(const std::string& command_switch) const {
    RunCommand(command_switch, {});
  }

  const UpdaterScope updater_scope_;
};

scoped_refptr<IntegrationTestCommands> CreateIntegrationTestCommandsSystem(
    UpdaterScope scope) {
  return base::MakeRefCounted<IntegrationTestCommandsSystem>(scope);
}

}  // namespace updater::test
