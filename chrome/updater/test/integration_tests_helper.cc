// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <iostream>
#include <map>
#include <optional>
#include <string>
#include <utility>

#include "base/at_exit.h"
#include "base/check.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/json/json_reader.h"
#include "base/logging.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/bind.h"
#include "base/test/launcher/unit_test_launcher.h"
#include "base/test/task_environment.h"
#include "base/test/test_suite.h"
#include "base/threading/platform_thread.h"
#include "base/threading/thread_restrictions.h"
#include "base/values.h"
#include "base/version.h"
#include "build/build_config.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/updater/app/app.h"
#include "chrome/updater/constants.h"
#include "chrome/updater/ipc/ipc_support.h"
#include "chrome/updater/test/integration_tests_impl.h"
#include "chrome/updater/test/unit_test_util.h"
#include "chrome/updater/updater_scope.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

#if BUILDFLAG(IS_WIN)
#include "base/win/scoped_com_initializer.h"
#include "chrome/updater/util/win_util.h"
#endif

namespace updater::test {
namespace {

using ::testing::EmptyTestEventListener;
using ::testing::Test;
using ::testing::TestEventListeners;
using ::testing::TestInfo;
using ::testing::TestPartResult;
using ::testing::UnitTest;

constexpr int kSuccess = 0;
constexpr int kUnknownSwitch = 101;
constexpr int kBadCommand = 102;

base::Value ValueFromString(const std::string& values) {
  std::optional<base::Value> results_value = base::JSONReader::Read(values);
  EXPECT_TRUE(results_value);
  return results_value->Clone();
}

template <typename... Args>
base::RepeatingCallback<bool(Args...)> WithSwitch(
    const std::string& flag,
    base::RepeatingCallback<bool(const std::string&, Args...)> callback) {
  return base::BindLambdaForTesting([=](Args... args) {
    const base::CommandLine* command_line =
        base::CommandLine::ForCurrentProcess();
    if (command_line->HasSwitch(flag)) {
      return callback.Run(command_line->GetSwitchValueASCII(flag),
                          std::move(args)...);
    }
    LOG(ERROR) << "Missing switch: " << flag;
    return false;
  });
}

// Overload for bool switches, represented by literals "false" and "true".
template <typename... Args>
base::RepeatingCallback<bool(Args...)> WithSwitch(
    const std::string& flag,
    base::RepeatingCallback<bool(bool, Args...)> callback) {
  return WithSwitch(
      flag,
      base::BindLambdaForTesting([=](const std::string& flag, Args... args) {
        if (flag == "false" || flag == "true") {
          return callback.Run(flag == "true", std::move(args)...);
        }
        return false;
      }));
}

// Overload for int switches.
template <typename... Args>
base::RepeatingCallback<bool(Args...)> WithSwitch(
    const std::string& flag,
    base::RepeatingCallback<bool(int, Args...)> callback) {
  return WithSwitch(
      flag,
      base::BindLambdaForTesting([=](const std::string& flag, Args... args) {
        int flag_int = -1;
        if (base::StringToInt(flag, &flag_int)) {
          return callback.Run(flag_int, std::move(args)...);
        }
        return false;
      }));
}

// Overload for GURL switches.
template <typename... Args>
base::RepeatingCallback<bool(Args...)> WithSwitch(
    const std::string& flag,
    base::RepeatingCallback<bool(const GURL&, Args...)> callback) {
  return WithSwitch(
      flag,
      base::BindLambdaForTesting([=](const std::string& flag, Args... args) {
        return callback.Run(GURL(flag), std::move(args)...);
      }));
}

// Overload for FilePath switches.
template <typename... Args>
base::RepeatingCallback<bool(Args...)> WithSwitch(
    const std::string& flag,
    base::RepeatingCallback<bool(const base::FilePath&, Args...)> callback) {
  return WithSwitch(
      flag,
      base::BindLambdaForTesting([=](const std::string& flag, Args... args) {
        return callback.Run(base::FilePath::FromUTF8Unsafe(flag),
                            std::move(args)...);
      }));
}

// Overload for Version switches.
template <typename... Args>
base::RepeatingCallback<bool(Args...)> WithSwitch(
    const std::string& flag,
    base::RepeatingCallback<bool(const base::Version&, Args...)> callback) {
  return WithSwitch(
      flag,
      base::BindLambdaForTesting([=](const std::string& flag, Args... args) {
        return callback.Run(base::Version(flag), std::move(args)...);
      }));
}

// Overload for Time switches.
template <typename... Args>
base::RepeatingCallback<bool(Args...)> WithSwitch(
    const std::string& flag,
    base::RepeatingCallback<bool(const base::Time&, Args...)> callback) {
  return WithSwitch(
      flag,
      base::BindLambdaForTesting([=](const std::string& flag, Args... args) {
        double flag_value;
        if (base::StringToDouble(flag, &flag_value)) {
          return callback.Run(
              base::Time::FromMillisecondsSinceUnixEpoch(flag_value),
              std::move(args)...);
        }
        return false;
      }));
}

// Overload for TimeDelta switches.
template <typename... Args>
base::RepeatingCallback<bool(Args...)> WithSwitch(
    const std::string& flag,
    base::RepeatingCallback<bool(base::TimeDelta, Args...)> callback) {
  return WithSwitch(
      flag,
      base::BindLambdaForTesting([=](const std::string& flag, Args... args) {
        int flag_value;
        if (base::StringToInt(flag, &flag_value)) {
          return callback.Run(base::Seconds(flag_value), std::move(args)...);
        }
        return false;
      }));
}

// Overload for base::Value::Dict switches.
template <typename... Args>
base::RepeatingCallback<bool(Args...)> WithSwitch(
    const std::string& flag,
    base::RepeatingCallback<bool(const base::Value::Dict&, Args...)> callback) {
  return WithSwitch(
      flag,
      base::BindLambdaForTesting([=](const std::string& flag, Args... args) {
        return callback.Run(std::move(ValueFromString(flag).GetDict()),
                            std::move(args)...);
      }));
}

// Overload for base::Value::List switches.
template <typename... Args>
base::RepeatingCallback<bool(Args...)> WithSwitch(
    const std::string& flag,
    base::RepeatingCallback<bool(const base::Value::List&, Args...)> callback) {
  return WithSwitch(
      flag,
      base::BindLambdaForTesting([=](const std::string& flag, Args... args) {
        return callback.Run(std::move(ValueFromString(flag).GetList()),
                            std::move(args)...);
      }));
}

// Overload for `AppBundleWebCreateMode` switches, represented by ints.
template <typename... Args>
base::RepeatingCallback<bool(Args...)> WithSwitch(
    const std::string& flag,
    base::RepeatingCallback<bool(AppBundleWebCreateMode, Args...)> callback) {
  return WithSwitch(
      flag,
      base::BindLambdaForTesting([=](const std::string& flag, Args... args) {
        int flag_app_bundle_web_create_mode = -1;
        if (base::StringToInt(flag, &flag_app_bundle_web_create_mode) &&
            flag_app_bundle_web_create_mode >=
                static_cast<int>(AppBundleWebCreateMode::kCreateApp) &&
            flag_app_bundle_web_create_mode <=
                static_cast<int>(AppBundleWebCreateMode::kCreateInstalledApp)) {
          return callback.Run(static_cast<AppBundleWebCreateMode>(
                                  flag_app_bundle_web_create_mode),
                              std::move(args)...);
        }
        return false;
      }));
}

template <typename Arg, typename... RemainingArgs>
base::RepeatingCallback<bool(RemainingArgs...)> WithArg(
    Arg arg,
    base::RepeatingCallback<bool(Arg, RemainingArgs...)> callback) {
  return base::BindRepeating(callback, arg);
}

// Adapts the input callback to take a shutdown callback as the final parameter.
template <typename... Args>
base::RepeatingCallback<bool(Args..., base::OnceCallback<void(int)>)>
WithShutdown(base::RepeatingCallback<int(Args...)> callback) {
  return base::BindLambdaForTesting(
      [=](Args... args, base::OnceCallback<void(int)> shutdown) {
        std::move(shutdown).Run(callback.Run(args...));
        return true;
      });
}

// Short-named wrapper around BindOnce.
template <typename... Args, typename... ProvidedArgs>
base::RepeatingCallback<bool(Args..., base::OnceCallback<void(int)>)> Wrap(
    int (*func)(Args...),
    ProvidedArgs... provided_args) {
  return WithShutdown(base::BindRepeating(func, provided_args...));
}

// Overload of Wrap for functions that return void. (Returns kSuccess.)
template <typename... Args>
base::RepeatingCallback<bool(Args..., base::OnceCallback<void(int)>)> Wrap(
    void (*func)(Args...)) {
  return WithShutdown(base::BindLambdaForTesting([=](Args... args) {
    func(args...);
    return kSuccess;
  }));
}

// Helper to shorten lines below.
template <typename... Args>
base::RepeatingCallback<bool(Args...)> WithSystemScope(
    base::RepeatingCallback<bool(UpdaterScope, Args...)> callback) {
  return WithArg(UpdaterScope::kSystem, callback);
}

class AppTestHelper : public App {
 private:
  ~AppTestHelper() override = default;
  void FirstTaskRun() override;
};

void AppTestHelper::FirstTaskRun() {
  std::map<std::string,
           base::RepeatingCallback<bool(base::OnceCallback<void(int)>)>>
      commands = {
          // To add additional commands, first Wrap a pointer to the target
          // function (which should be declared in integration_tests_impl.h),
          // and then use the With* helper functions to provide its arguments.
          {"clean", WithSystemScope(Wrap(&Clean))},
          {"enter_test_mode",
           WithSwitch(
               "ceca_connection_timeout",
               WithSwitch(
                   "server_keep_alive_time",
                   WithSwitch(
                       "idle_timeout",
                       WithSwitch(
                           "app_logo_url",
                           WithSwitch(
                               "device_management_url",
                               WithSwitch(
                                   "crash_upload_url",
                                   WithSwitch("update_url",
                                              Wrap(&EnterTestMode))))))))},
          {"exit_test_mode", WithSystemScope(Wrap(&ExitTestMode))},
          {"set_group_policies", WithSwitch("values", Wrap(&SetGroupPolicies))},
          {"set_platform_policies",
           WithSwitch("values", Wrap(&SetPlatformPolicies))},
          {"set_machine_managed",
           WithSwitch("managed", Wrap(&SetMachineManaged))},
          {"fill_log", WithSystemScope(Wrap(&FillLog))},
          {"expect_log_rotated", WithSystemScope(Wrap(&ExpectLogRotated))},
          {"expect_registered",
           WithSwitch("app_id", WithSystemScope(Wrap(&ExpectRegistered)))},
          {"expect_not_registered",
           WithSwitch("app_id", WithSystemScope(Wrap(&ExpectNotRegistered)))},
          {"expect_app_tag",
           WithSwitch("tag", WithSwitch("app_id",
                                        WithSystemScope(Wrap(&ExpectAppTag))))},
          {"set_app_tag",
           WithSwitch("tag",
                      WithSwitch("app_id", WithSystemScope(Wrap(&SetAppTag))))},
          {"expect_app_version",
           WithSwitch(
               "app_version",
               WithSwitch("app_id", WithSystemScope(Wrap(&ExpectAppVersion))))},
          {"expect_candidate_uninstalled",
           WithSystemScope(Wrap(&ExpectCandidateUninstalled))},
          {"expect_clean", WithSystemScope(Wrap(&ExpectClean))},
          {"expect_installed", WithSystemScope(Wrap(&ExpectInstalled))},
#if BUILDFLAG(IS_WIN)
          {"expect_interfaces_registered",
           WithSystemScope(Wrap(&ExpectInterfacesRegistered))},
          {"expect_marshal_interface_succeeds",
           WithSystemScope(Wrap(&ExpectMarshalInterfaceSucceeds))},
          {"expect_legacy_update3web_succeeds",
           WithSwitch(
               "cancel_when_downloading",
               WithSwitch(
                   "expected_error_code",
                   WithSwitch(
                       "expected_final_state",
                       WithSwitch(
                           "app_bundle_web_create_mode",
                           WithSwitch(
                               "app_id",
                               WithSystemScope(
                                   Wrap(&ExpectLegacyUpdate3WebSucceeds)))))))},
          {"expect_legacy_process_launcher_succeeds",
           WithSystemScope(Wrap(&ExpectLegacyProcessLauncherSucceeds))},
          {"expect_legacy_app_command_web_succeeds",
           WithSwitch(
               "expected_exit_code",
               WithSwitch(
                   "parameters",
                   WithSwitch(
                       "command_id",
                       WithSwitch("app_id",
                                  WithSystemScope(Wrap(
                                      &ExpectLegacyAppCommandWebSucceeds))))))},
          {"expect_legacy_policy_status_succeeds",
           WithSystemScope(Wrap(&ExpectLegacyPolicyStatusSucceeds))},
          {"run_uninstall_cmd_line",
           WithSystemScope(Wrap(&RunUninstallCmdLine))},
          {"run_handoff",
           WithSwitch("app_id", WithSystemScope(Wrap(&RunHandoff)))},
#endif  // BUILDFLAG(IS_WIN)
          {"expect_version_active",
           WithSwitch("updater_version",
                      WithSystemScope(Wrap(&ExpectVersionActive)))},
          {"expect_version_not_active",
           WithSwitch("updater_version",
                      WithSystemScope(Wrap(&ExpectVersionNotActive)))},
          {"install", WithSwitch("switches", WithSystemScope(Wrap(&Install)))},
          {"install_updater_and_app",
           WithSwitch(
               "wait_for_the_installer",
               WithSwitch(
                   "expect_success",
                   WithSwitch(
                       "verify_app_logo_loaded",
                       WithSwitch(
                           "always_launch_cmd",
                           WithSwitch(
                               "child_window_text_to_find",
                               WithSwitch(
                                   "tag",
                                   WithSwitch(
                                       "is_silent_install",
                                       WithSwitch(
                                           "app_id",
                                           WithSystemScope(Wrap(
                                               &InstallUpdaterAndApp))))))))))},
          {"print_log", WithSystemScope(Wrap(&PrintLog))},
          {"run_wake",
           WithSwitch("exit_code", WithSystemScope(Wrap(&RunWake)))},
          {"run_wake_all", WithSystemScope(Wrap(&RunWakeAll))},
          {"run_wake_active",
           WithSwitch("exit_code", WithSystemScope(Wrap(&RunWakeActive)))},
          {"run_crash_me", WithSystemScope(Wrap(&RunCrashMe))},
          {"run_server",
           WithSwitch("internal", WithSwitch("exit_code", WithSystemScope(Wrap(
                                                              &RunServer))))},
          {"update",
           WithSwitch("install_data_index",
                      (WithSwitch("app_id", WithSystemScope(Wrap(&Update)))))},
          {"register_app",
           WithSwitch("registration",
                      WithSystemScope(Wrap(&RegisterAppByValue)))},
          {"check_for_update",
           (WithSwitch("app_id", WithSystemScope(Wrap(&CheckForUpdate))))},
          {"update_all", WithSystemScope(Wrap(&UpdateAll))},
          {"get_app_states", WithSwitch("expected_app_states",
                                        WithSystemScope(Wrap(&GetAppStates)))},
          {"delete_updater_directory",
           WithSystemScope(Wrap(&DeleteUpdaterDirectory))},
          {"delete_active_updater_executable",
           WithSystemScope(Wrap(&DeleteActiveUpdaterExecutable))},
          {"delete_file",
           (WithSwitch("path", WithSystemScope(Wrap(&DeleteFile))))},
          {"install_app",
           WithSwitch("app_version", WithSwitch("app_id", WithSystemScope(Wrap(
                                                              &InstallApp))))},
          {"install_app_via_service",
           WithSwitch("expected_final_values",
                      WithSwitch("app_id", WithSystemScope(
                                               Wrap(&InstallAppViaService))))},
          {"uninstall_app",
           WithSwitch("app_id", WithSystemScope(Wrap(&UninstallApp)))},
          {"set_existence_checker_path",
           WithSwitch("path",
                      (WithSwitch("app_id", WithSystemScope(Wrap(
                                                &SetExistenceCheckerPath)))))},
          {"setup_fake_updater_higher_version",
           WithSystemScope(Wrap(&SetupFakeUpdaterHigherVersion))},
          {"setup_fake_updater_lower_version",
           WithSystemScope(Wrap(&SetupFakeUpdaterLowerVersion))},
          {"setup_real_updater_lower_version",
           WithSystemScope(Wrap(&SetupRealUpdaterLowerVersion))},
          {"set_first_registration_counter",
           WithSwitch("value", WithSystemScope(Wrap(&SetServerStarts)))},
          {"stress_update_service",
           WithSystemScope(Wrap(&StressUpdateService))},
          {"uninstall", WithSystemScope(Wrap(&Uninstall))},
          {"call_service_update",
           WithSwitch(
               "same_version_update_allowed",
               WithSwitch("install_data_index",
                          WithSwitch("app_id", WithSystemScope(Wrap(
                                                   &CallServiceUpdate)))))},
          {"setup_fake_legacy_updater",
           WithSystemScope(Wrap(&SetupFakeLegacyUpdater))},
#if BUILDFLAG(IS_WIN)
          {"run_fake_legacy_updater",
           WithSystemScope(Wrap(&RunFakeLegacyUpdater))},
#endif  // BUILDFLAG(IS_WIN)
#if BUILDFLAG(IS_MAC)
          {"privileged_helper_install",
           WithSystemScope(Wrap(&PrivilegedHelperInstall))},
          {"delete_legacy_updater",
           WithSystemScope(Wrap(&DeleteLegacyUpdater))},
          {"expect_prepare_to_run_bundle_success",
           WithSwitch("bundle_path", Wrap(&ExpectPrepareToRunBundleSuccess))},
#endif  // BUILDFLAG(IS_MAC)
          {"expect_legacy_updater_migrated",
           WithSystemScope(Wrap(&ExpectLegacyUpdaterMigrated))},
          {"run_recovery_component",
           WithSwitch("browser_version",
                      WithSwitch("app_id", WithSystemScope(
                                               Wrap(&RunRecoveryComponent))))},
          {"set_last_checked",
           WithSwitch("time", WithSystemScope(Wrap(&SetLastChecked)))},
          {"expect_last_checked", WithSystemScope(Wrap(&ExpectLastChecked))},
          {"expect_last_started", WithSystemScope(Wrap(&ExpectLastStarted))},
          {"run_offline_install",
           WithSwitch("silent",
                      WithSwitch("legacy_install",
                                 WithSystemScope(Wrap(&RunOfflineInstall))))},
          {"run_offline_install_os_not_supported",
           WithSwitch("silent",
                      WithSwitch("legacy_install",
                                 WithSystemScope(
                                     Wrap(&RunOfflineInstallOsNotSupported))))},
          {"dm_push_enrollment_token",
           WithSwitch("enrollment_token", Wrap(DMPushEnrollmentToken))},
          {"dm_deregister_device", WithSystemScope(Wrap(&DMDeregisterDevice))},
          {"dm_cleanup", WithSystemScope(Wrap(&DMCleanup))},
          {"install_enterprise_companion_app",
           Wrap(&InstallEnterpriseCompanionApp)},
          {"install_broken_enterprise_companion_app",
           Wrap(&InstallBrokenEnterpriseCompanionApp)},
          {"uninstall_broken_enterprise_companion_app",
           Wrap(&UninstallBrokenEnterpriseCompanionApp)},
          {"install_enterprise_companion_app_overrides",
           WithSwitch("external_overrides",
                      Wrap(&InstallEnterpriseCompanionAppOverrides))},
          {"expect_enterprise_companion_app_not_installed",
           Wrap(&ExpectEnterpriseCompanionAppNotInstalled)},
          {"uninstall_enterprise_companion_app",
           Wrap(&UninstallEnterpriseCompanionApp)},
      };

  const base::CommandLine* command_line =
      base::CommandLine::ForCurrentProcess();
  for (const auto& [command, callback] : commands) {
    if (command_line->HasSwitch(command)) {
      base::ScopedAllowBlockingForTesting allow_blocking;
      if (!callback.Run(base::BindOnce(&AppTestHelper::Shutdown, this))) {
        Shutdown(kBadCommand);
      }
      return;
    }
  }

  LOG(ERROR) << "No supported switch provided. Command: "
             << command_line->GetCommandLineString();
  Shutdown(kUnknownSwitch);
}

scoped_refptr<App> MakeAppTestHelper() {
  return base::MakeRefCounted<AppTestHelper>();
}

// Provides custom formatting for the unit test output.
class TersePrinter : public EmptyTestEventListener {
 private:
  // Called before any test activity starts.
  void OnTestProgramStart(const UnitTest& /*unit_test*/) override {}

  // Called after all test activities have ended.
  void OnTestProgramEnd(const UnitTest& unit_test) override {
    VLOG(0) << "Command " << (unit_test.Passed() ? "SUCCEEDED" : "FAILED")
            << ".";
  }

  // Called before a test starts.
  void OnTestStart(const TestInfo& /*test_info*/) override {}

  // Called after a failed assertion or a SUCCEED() invocation. Prints a
  // backtrace showing the failure.
  void OnTestPartResult(const TestPartResult& result) override {
    if (!result.failed()) {
      return;
    }
    logging::LogMessage(result.file_name(), result.line_number(),
                        logging::LOGGING_ERROR)
            .stream()
        << "*** Failure" << std::endl
        << result.message();
  }

  // Called after a test ends.
  void OnTestEnd(const TestInfo& /*test_info*/) override {}
};

int IntegrationTestsHelperMain(int argc, char** argv) {
  base::PlatformThread::SetName("IntegrationTestsHelperMain");
  base::CommandLine::Init(argc, argv);

  // Use the ${ISOLATED_OUTDIR} as a log destination. `test_suite` must be
  // defined before setting log items. The integration test helper always
  // logs into the same file as the `updater_tests_system` because the programs
  // are used together.
  base::TestSuite test_suite(argc, argv);
  updater::test::InitLoggingForUnitTest(
      base::FilePath(FILE_PATH_LITERAL("updater_test_system.log")));
#if BUILDFLAG(IS_WIN)
  auto scoped_com_initializer =
      std::make_unique<base::win::ScopedCOMInitializer>(
          base::win::ScopedCOMInitializer::kMTA);
  // Failing to disable COM exception handling is a critical error.
  CHECK(SUCCEEDED(DisableCOMExceptionHandling()))
      << "Failed to disable COM exception handling.";
#endif
  chrome::RegisterPathProvider();
  TestEventListeners& listeners = UnitTest::GetInstance()->listeners();
  delete listeners.Release(listeners.default_result_printer());
  listeners.Append(new TersePrinter);
  return base::LaunchUnitTestsSerially(
      argc, argv,
      base::BindOnce(&base::TestSuite::Run, base::Unretained(&test_suite)));
}

// Do not disable this test when encountering integration tests failures.
// This is not a unit test. It just wraps the execution of an integration test
// command, which is typical a step of an integration test.
TEST(TestHelperCommandRunner, Run) {
  base::test::TaskEnvironment environment;
  ScopedIPCSupportWrapper ipc_support_;
  ASSERT_EQ(MakeAppTestHelper()->Run(), 0);
}

}  // namespace
}  // namespace updater::test

// Wraps the execution of one integration test command in a unit test. The test
// commands contain gtest assertions, therefore the invocation of test commands
// must occur within the scope of a unit test of a gtest program. The test
// helper defines a unit test "TestHelperCommandRunner.Run", which runs the
// actual test command. Returns 0 if the test command succeeded.
int main(int argc, char** argv) {
  return updater::test::IntegrationTestsHelperMain(argc, argv);
}
