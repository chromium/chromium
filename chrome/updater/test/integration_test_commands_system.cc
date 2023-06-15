// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <cstdlib>
#include <memory>
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
#include "base/values.h"
#include "base/version.h"
#include "build/build_config.h"
#include "chrome/updater/constants.h"
#include "chrome/updater/persisted_data.h"
#include "chrome/updater/prefs.h"
#include "chrome/updater/test/integration_test_commands.h"
#include "chrome/updater/test/integration_tests_impl.h"
#include "chrome/updater/test_scope.h"
#include "chrome/updater/update_service.h"
#include "chrome/updater/updater_scope.h"
#include "chrome/updater/util/util.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "url/gurl.h"

#if BUILDFLAG(IS_WIN)
#include "base/strings/utf_string_conversions.h"
#endif  // BUILDFLAG(IS_WIN)

namespace updater {
namespace test {

namespace {

std::string StringFromValue(const base::Value& value) {
  std::string value_string;
  EXPECT_TRUE(base::JSONWriter::Write(value, &value_string));
  return value_string;
}

}  // namespace

class IntegrationTestCommandsSystem : public IntegrationTestCommands {
 public:
  IntegrationTestCommandsSystem() = default;

  void ExpectNoCrashes() const override {
    updater::test::ExpectNoCrashes(updater_scope_);
  }

  void PrintLog() const override { RunCommand("print_log"); }

  void CopyLog() const override {
    const absl::optional<base::FilePath> path =
        GetInstallDirectory(updater_scope_);
    ASSERT_TRUE(path);
    if (path)
      updater::test::CopyLog(*path);
  }

  void Clean() const override { RunCommand("clean"); }

  void ExpectClean() const override { RunCommand("expect_clean"); }

  void Install() const override { RunCommand("install"); }

  void InstallUpdaterAndApp(const std::string& app_id) const override {
    RunCommand("install_updater_and_app", {Param("app_id", app_id)});
  }

  void ExpectInstalled() const override { RunCommand("expect_installed"); }

  void Uninstall() const override { RunCommand("uninstall"); }

  void ExpectCandidateUninstalled() const override {
    RunCommand("expect_candidate_uninstalled");
  }

  void EnterTestMode(const GURL& update_url,
                     const GURL& crash_upload_url,
                     const GURL& device_management_url,
                     const base::TimeDelta& idle_timeout) const override {
    RunCommand("enter_test_mode",
               {Param("update_url", update_url.spec()),
                Param("crash_upload_url", crash_upload_url.spec()),
                Param("device_management_url", device_management_url.spec()),
                Param("idle_timeout",
                      base::NumberToString(idle_timeout.InSeconds()))});
  }

  void ExitTestMode() const override { RunCommand("exit_test_mode"); }

  void SetGroupPolicies(const base::Value::Dict& values) const override {
    RunCommand("set_group_policies",
               {Param("values", StringFromValue(base::Value(values.Clone())))});
  }

  void ExpectSelfUpdateSequence(ScopedServer* test_server) const override {
    updater::test::ExpectSelfUpdateSequence(updater_scope_, test_server);
  }

  void ExpectUninstallPing(ScopedServer* test_server) const override {
    updater::test::ExpectUninstallPing(updater_scope_, test_server);
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
                            const base::Version& to_version) const override {
    updater::test::ExpectUpdateSequence(updater_scope_, test_server, app_id,
                                        install_data_index, priority,
                                        from_version, to_version);
  }

  void ExpectInstallSequence(ScopedServer* test_server,
                             const std::string& app_id,
                             const std::string& install_data_index,
                             UpdateService::Priority priority,
                             const base::Version& from_version,
                             const base::Version& to_version) const override {
    updater::test::ExpectInstallSequence(updater_scope_, test_server, app_id,
                                         install_data_index, priority,
                                         from_version, to_version);
  }

  void ExpectVersionActive(const std::string& version) const override {
    RunCommand("expect_version_active", {Param("version", version)});
  }

  void ExpectVersionNotActive(const std::string& version) const override {
    RunCommand("expect_version_not_active", {Param("version", version)});
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

  void ExpectAppVersion(const std::string& app_id,
                        const base::Version& version) const override {
    RunCommand("expect_app_version", {Param("app_id", app_id),
                                      Param("version", version.GetString())});
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
               {Param("internal", internal ? "true" : "false"),
                Param("exit_code", base::NumberToString(expected_exit_code))});
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

  void DeleteFile(const base::FilePath& path) const override {
    RunCommand("delete_file", {Param("path", path.MaybeAsASCII())});
  }

  void InstallApp(const std::string& app_id) const override {
    RunCommand("install_app", {Param("app_id", app_id)});
  }

  bool WaitForUpdaterExit() const override {
    return updater::test::WaitForUpdaterExit(updater_scope_);
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
      int expected_error_code) const override {
    RunCommand("expect_legacy_update3web_succeeds",
               {Param("app_id", app_id),
                Param("app_bundle_web_create_mode",
                      base::NumberToString(
                          static_cast<int>(app_bundle_web_create_mode))),
                Param("expected_final_state",
                      base::NumberToString(expected_final_state)),
                Param("expected_error_code",
                      base::NumberToString(expected_error_code))});
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

  base::FilePath GetDifferentUserPath() const override {
    // On POSIX, the path may be chowned; so do not use a file not owned by the
    // test, nor the test executable itself.
    NOTREACHED() << __func__ << ": not implemented.";
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
                      policy_same_version_update ==
                              UpdateService::PolicySameVersionUpdate::kAllowed
                          ? "true"
                          : "false")});
  }

  void SetupFakeLegacyUpdater() const override {
    RunCommand("setup_fake_legacy_updater");
  }

#if BUILDFLAG(IS_WIN)
  void RunFakeLegacyUpdater() const override {
    RunCommand("run_fake_legacy_updater");
  }
#endif  // BUILDFLAG(IS_WIN)

  void ExpectLegacyUpdaterMigrated() const override {
    RunCommand("expect_legacy_updater_migrated");
  }

  void RunRecoveryComponent(const std::string& app_id,
                            const base::Version& version) const override {
    RunCommand(
        "run_recovery_component",
        {Param("app_id", app_id), Param("version", version.GetString())});
  }

  void ExpectLastChecked() const override { RunCommand("expect_last_checked"); }

  void ExpectLastStarted() const override { RunCommand("expect_last_started"); }

  void UninstallApp(const std::string& app_id) const override {
    RunCommand("uninstall_app", {Param("app_id", app_id)});
  }

  void RunOfflineInstall(bool is_legacy_install,
                         bool is_silent_install) override {
    RunCommand("run_offline_install",
               {Param("legacy_install", is_legacy_install ? "true" : "false"),
                Param("silent", is_silent_install ? "true" : "false")});
  }

  void RunOfflineInstallOsNotSupported(bool is_legacy_install,
                                       bool is_silent_install) override {
    RunCommand("run_offline_install_os_not_supported",
               {Param("legacy_install", is_legacy_install ? "true" : "false"),
                Param("silent", is_silent_install ? "true" : "false")});
  }

  void DMDeregisterDevice() override { RunCommand("dm_deregister_device"); }
  void DMCleanup() override { RunCommand("dm_cleanup"); }

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

  static const UpdaterScope updater_scope_;
};

const UpdaterScope IntegrationTestCommandsSystem::updater_scope_ =
    GetTestScope();

scoped_refptr<IntegrationTestCommands> CreateIntegrationTestCommandsSystem() {
  return base::MakeRefCounted<IntegrationTestCommandsSystem>();
}

}  // namespace test
}  // namespace updater
