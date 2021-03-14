// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/updater/test/integration_tests.h"

#include <cstdlib>
#include <memory>

#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/memory/scoped_refptr.h"
#include "base/numerics/checked_math.h"
#include "base/process/launch.h"
#include "base/process/process.h"
#include "base/strings/strcat.h"
#include "base/test/bind.h"
#include "base/test/scoped_run_loop_timeout.h"
#include "base/test/task_environment.h"
#include "base/test/test_timeouts.h"
#include "base/time/time.h"
#include "base/version.h"
#include "build/build_config.h"
#include "chrome/updater/constants.h"
#include "chrome/updater/persisted_data.h"
#include "chrome/updater/prefs.h"
#include "chrome/updater/registration_data.h"
#include "chrome/updater/test/server.h"
#include "chrome/updater/test/test_app/constants.h"
#include "chrome/updater/test/test_app/test_app_version.h"
#include "chrome/updater/update_service.h"
#include "chrome/updater/updater_scope.h"
#include "chrome/updater/updater_version.h"
#include "chrome/updater/util.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

#if defined(OS_WIN)
#include "base/strings/utf_string_conversions.h"
#endif  // OS_WIN

namespace updater {

namespace test {

// TODO(crbug.com/1096654): Move the IntegrationTestCommands* classes, along
// with all the updater::test::Foo(UpdaterScope) implementations to their
// own file.
class IntegrationTestCommandsUser : public IntegrationTestCommands {
 public:
  IntegrationTestCommandsUser() = default;

  static scoped_refptr<IntegrationTestCommands>
  CreateIntegrationTestCommands() {
    return base::MakeRefCounted<IntegrationTestCommandsUser>();
  }

  UpdaterScope scope() const override { return UpdaterScope::kUser; }

  void PrintLog() const override { updater::test::PrintLog(scope()); }

  void CopyLog() const override {
    base::Optional<base::FilePath> path = GetDataDirPath(scope());
    EXPECT_TRUE(path);
    if (path)
      updater::test::CopyLog(*path);
  }

  void Clean() const override { updater::test::Clean(scope()); }

  void ExpectClean() const override { updater::test::ExpectClean(scope()); }

  void Install() const override { updater::test::Install(scope()); }

  void ExpectInstalled() const override {
    updater::test::ExpectInstalled(scope());
  }

  void Uninstall() const override { updater::test::Uninstall(scope()); }

  void ExpectCandidateUninstalled() const override {
    updater::test::ExpectCandidateUninstalled(scope());
  }

  void EnterTestMode(const GURL& url) const override {
    updater::test::EnterTestMode(url);
  }

  void ExpectVersionActive(const std::string& version) const override {
    updater::test::ExpectVersionActive(version);
  }

  void ExpectVersionNotActive(const std::string& version) const override {
    updater::test::ExpectVersionNotActive(version);
  }

  void ExpectActiveUpdater() const override {
    updater::test::ExpectActiveUpdater(scope());
  }

  void SetupFakeUpdaterHigherVersion() const override {
    updater::test::SetupFakeUpdaterHigherVersion(scope());
  }

  void SetupFakeUpdaterLowerVersion() const override {
    updater::test::SetupFakeUpdaterLowerVersion(scope());
  }

  void SetFakeExistenceCheckerPath(const std::string& app_id) const override {
    updater::test::SetFakeExistenceCheckerPath(app_id);
  }

  void ExpectAppUnregisteredExistenceCheckerPath(
      const std::string& app_id) const override {
    updater::test::ExpectAppUnregisteredExistenceCheckerPath(app_id);
  }

  void SetActive(const std::string& app_id) const override {
    updater::test::SetActive(scope(), app_id);
  }

  void ExpectActive(const std::string& app_id) const override {
    updater::test::ExpectActive(scope(), app_id);
  }

  void ExpectNotActive(const std::string& app_id) const override {
    updater::test::ExpectNotActive(scope(), app_id);
  }

  void RunWake(int exit_code) const override {
    updater::test::RunWake(scope(), exit_code);
  }

  void RegisterApp(const std::string& app_id) const override {
    updater::test::RegisterApp(app_id);
  }

  void RegisterTestApp() const override {
    updater::test::RegisterTestApp(scope());
  }

 private:
  ~IntegrationTestCommandsUser() override = default;
};

class IntegrationTestCommandsSystem : public IntegrationTestCommands {
 public:
  IntegrationTestCommandsSystem() = default;

  static scoped_refptr<IntegrationTestCommands>
  CreateIntegrationTestCommands() {
    return base::MakeRefCounted<IntegrationTestCommandsSystem>();
  }

  UpdaterScope scope() const override { return UpdaterScope::kSystem; }

  void PrintLog() const override { RunHelperCommand("print_log"); }

  void CopyLog() const override {
    base::Optional<base::FilePath> path = GetDataDirPath(scope());
    ASSERT_TRUE(path);

#if defined(OS_WIN)
    RunHelperCommand("copy_log",
                     {HelperParam("path", base::WideToUTF8(path->value()))});
#else
    RunHelperCommand("copy_log", {HelperParam("path", path->value())});
#endif  // OS_WIN
  }

  void Clean() const override { RunHelperCommand("clean"); }

  void ExpectClean() const override { RunHelperCommand("expect_clean"); }

  void Install() const override { RunHelperCommand("install"); }

  void ExpectInstalled() const override {
    RunHelperCommand("expect_installed");
  }

  void Uninstall() const override { RunHelperCommand("uninstall"); }

  void ExpectCandidateUninstalled() const override {
    RunHelperCommand("expect_candidate_uninstalled");
  }

  void EnterTestMode(const GURL& url) const override {
    RunHelperCommand("enter_test_mode", {HelperParam("url", url.spec())});
  }

  void ExpectVersionActive(const std::string& version) const override {
    RunHelperCommand("expect_version_active",
                     {HelperParam("version", version)});
  }

  void ExpectVersionNotActive(const std::string& version) const override {
    RunHelperCommand("expect_version_not_active",
                     {HelperParam("version", version)});
  }

  void ExpectActiveUpdater() const override {
    RunHelperCommand("expect_active_updater");
  }

  void ExpectActive(const std::string& app_id) const override {
    RunHelperCommand("expect_active", {HelperParam("app_id", app_id)});
  }

  void ExpectNotActive(const std::string& app_id) const override {
    RunHelperCommand("expect_not_active", {HelperParam("app_id", app_id)});
  }

  void SetupFakeUpdaterHigherVersion() const override {
    RunHelperCommand("setup_fake_updater_higher_version");
  }

  void SetupFakeUpdaterLowerVersion() const override {
    RunHelperCommand("setup_fake_updater_lower_version");
  }

  void SetFakeExistenceCheckerPath(const std::string& app_id) const override {
    RunHelperCommand("set_fake_existence_checker_path",
                     {HelperParam("app_id", app_id)});
  }

  void ExpectAppUnregisteredExistenceCheckerPath(
      const std::string& app_id) const override {
    RunHelperCommand("expect_app_unregistered_existence_checker_path",
                     {HelperParam("app_id", app_id)});
  }

  void SetActive(const std::string& app_id) const override {
    RunHelperCommand("set_active", {HelperParam("app_id", app_id)});
  }

  void RunWake(int expected_exit_code) const override {
    RunHelperCommand(
        "run_wake",
        {HelperParam("exit_code", base::NumberToString(expected_exit_code))});
  }

  void RegisterApp(const std::string& app_id) const override {
    RunHelperCommand("register_app", {HelperParam("app_id", app_id)});
  }

  void RegisterTestApp() const override {
    RunHelperCommand("register_test_app");
  }

 private:
  ~IntegrationTestCommandsSystem() override = default;

  struct HelperParam {
    HelperParam(const std::string& param_name, const std::string& param_value)
        : param_name(param_name), param_value(param_value) {}
    std::string param_name;
    std::string param_value;
  };

  void RunHelperCommand(const std::string& command_switch,
                        const std::vector<HelperParam>& params) const {
    const base::CommandLine command_line =
        *base::CommandLine::ForCurrentProcess();
    base::FilePath path(command_line.GetProgram());
    EXPECT_TRUE(base::PathExists(path));
    path = path.DirName();
    EXPECT_TRUE(base::PathExists(path));
    path = MakeAbsoluteFilePath(path);
    path = path.Append(FILE_PATH_LITERAL("updater_integration_tests_helper"));
    EXPECT_TRUE(base::PathExists(path));

    base::CommandLine helper_command(path);
    helper_command.AppendSwitch(command_switch);

    for (const HelperParam& param : params) {
      helper_command.AppendSwitchASCII(param.param_name, param.param_value);
    }

    helper_command.AppendSwitch(kEnableLoggingSwitch);
    helper_command.AppendSwitchASCII(kLoggingModuleSwitch, "*/updater/*=2");

    int exit_code = -1;
    ASSERT_TRUE(Run(scope(), helper_command, &exit_code));
    EXPECT_EQ(exit_code, 0);
  }

  void RunHelperCommand(const std::string& command_switch) const {
    RunHelperCommand(command_switch, {});
  }
};

void RegisterApp(const std::string& app_id) {
  scoped_refptr<UpdateService> update_service = CreateUpdateService();
  RegistrationRequest registration;
  registration.app_id = app_id;
  registration.version = base::Version("0.1");
  base::RunLoop loop;
  update_service->RegisterApp(
      registration, base::BindOnce(base::BindLambdaForTesting(
                        [&loop](const RegistrationResponse& response) {
                          EXPECT_EQ(response.status_code, 0);
                          loop.Quit();
                        })));
  loop.Run();
}

void ExpectVersionActive(const std::string& version) {
  EXPECT_EQ(CreateGlobalPrefs()->GetActiveVersion(), version);
}

void ExpectVersionNotActive(const std::string& version) {
  EXPECT_NE(CreateGlobalPrefs()->GetActiveVersion(), version);
}

void PrintLog(UpdaterScope scope) {
  std::string contents;
  base::Optional<base::FilePath> path = GetDataDirPath(scope);
  EXPECT_TRUE(path);
  if (path &&
      base::ReadFileToString(path->AppendASCII("updater.log"), &contents)) {
    VLOG(0) << "Contents of updater.log:";
    VLOG(0) << contents;
  } else {
    VLOG(0) << "Failed to read updater.log file.";
  }
}

const testing::TestInfo* GetTestInfo() {
  return testing::UnitTest::GetInstance()->current_test_info();
}

base::FilePath GetLogDestinationDir() {
  // Fetch path to ${ISOLATED_OUTDIR} env var.
  // ResultDB reads logs and test artifacts info from there.
  return base::FilePath::FromUTF8Unsafe(std::getenv("ISOLATED_OUTDIR"));
}

void CopyLog(const base::FilePath& src_dir) {
  // TODO(crbug.com/1159189): copy other test artifacts.
  base::FilePath dest_dir = GetLogDestinationDir();
  if (base::PathExists(dest_dir) && base::PathExists(src_dir)) {
    base::FilePath test_name_path = dest_dir.AppendASCII(base::StrCat(
        {GetTestInfo()->test_suite_name(), ".", GetTestInfo()->name()}));
    EXPECT_TRUE(base::CreateDirectory(test_name_path));

    base::FilePath dest_file_path = test_name_path.AppendASCII("updater.log");
    base::FilePath log_path = src_dir.AppendASCII("updater.log");
    VLOG(0) << "Copying updater.log file. From: " << log_path
            << ". To: " << dest_file_path;
    EXPECT_TRUE(base::CopyFile(log_path, dest_file_path));
  }
}

void RunWake(UpdaterScope scope, int expected_exit_code) {
  const base::Optional<base::FilePath> installed_executable_path =
      GetInstalledExecutablePath(scope);
  ASSERT_TRUE(installed_executable_path);
  EXPECT_TRUE(base::PathExists(*installed_executable_path));
  base::CommandLine command_line(*installed_executable_path);
  command_line.AppendSwitch(kWakeSwitch);
  command_line.AppendSwitch(kEnableLoggingSwitch);
  command_line.AppendSwitchASCII(kLoggingModuleSwitch, "*/updater/*=2");
  int exit_code = -1;
  ASSERT_TRUE(Run(scope, command_line, &exit_code));
  EXPECT_EQ(exit_code, expected_exit_code);
}

void SetupFakeUpdaterPrefs(const base::Version& version) {
  std::unique_ptr<GlobalPrefs> global_prefs = CreateGlobalPrefs();
  global_prefs->SetActiveVersion(version.GetString());
  global_prefs->SetSwapping(false);
  PrefsCommitPendingWrites(global_prefs->GetPrefService());

  ASSERT_EQ(version.GetString(), global_prefs->GetActiveVersion());
}

void SetupFakeUpdaterInstallFolder(UpdaterScope scope,
                                   const base::Version& version) {
  const base::Optional<base::FilePath> folder_path =
      GetFakeUpdaterInstallFolderPath(scope, version);
  ASSERT_TRUE(folder_path);
  ASSERT_TRUE(base::CreateDirectory(*folder_path));
}

void SetupFakeUpdater(UpdaterScope scope, const base::Version& version) {
  SetupFakeUpdaterPrefs(version);
  SetupFakeUpdaterInstallFolder(scope, version);
}

void SetupFakeUpdaterVersion(UpdaterScope scope, int offset) {
  ASSERT_NE(offset, 0);
  std::vector<uint32_t> components =
      base::Version(UPDATER_VERSION_STRING).components();
  base::CheckedNumeric<uint32_t> new_version = components[0];
  new_version += offset;
  ASSERT_TRUE(new_version.AssignIfValid(&components[0]));
  SetupFakeUpdater(scope, base::Version(std::move(components)));
}

void SetupFakeUpdaterLowerVersion(UpdaterScope scope) {
  SetupFakeUpdaterVersion(scope, -1);
}

void SetupFakeUpdaterHigherVersion(UpdaterScope scope) {
  SetupFakeUpdaterVersion(scope, 1);
}

void SetFakeExistenceCheckerPath(const std::string& app_id) {
  std::unique_ptr<GlobalPrefs> global_prefs = CreateGlobalPrefs();
  auto persisted_data =
      base::MakeRefCounted<PersistedData>(global_prefs->GetPrefService());
  base::FilePath fake_ecp =
      persisted_data->GetExistenceCheckerPath(app_id).Append(
          FILE_PATH_LITERAL("NOT_THERE"));
  persisted_data->SetExistenceCheckerPath(app_id, fake_ecp);

  PrefsCommitPendingWrites(global_prefs->GetPrefService());

  EXPECT_EQ(fake_ecp.value(),
            persisted_data->GetExistenceCheckerPath(app_id).value());
}

void ExpectAppUnregisteredExistenceCheckerPath(const std::string& app_id) {
  std::unique_ptr<GlobalPrefs> global_prefs = CreateGlobalPrefs();
  auto persisted_data =
      base::MakeRefCounted<PersistedData>(global_prefs->GetPrefService());
  EXPECT_EQ(base::FilePath(FILE_PATH_LITERAL("")).value(),
            persisted_data->GetExistenceCheckerPath(app_id).value());
}

bool Run(UpdaterScope scope, base::CommandLine command_line, int* exit_code) {
  base::ScopedAllowBaseSyncPrimitivesForTesting allow_wait_process;
  command_line.AppendSwitch("enable-logging");
  command_line.AppendSwitchASCII("vmodule", "*/updater/*=2");
  if (scope == UpdaterScope::kSystem) {
    command_line.AppendSwitch(kSystemSwitch);
    command_line = MakeElevated(command_line);
  }
  VLOG(0) << " Run command: " << command_line.GetCommandLineString();
  base::Process process = base::LaunchProcess(command_line, {});
  if (!process.IsValid())
    return false;

  // TODO(crbug.com/1096654): Get the timeout from TestTimeouts.
  return process.WaitForExitWithTimeout(base::TimeDelta::FromSeconds(45),
                                        exit_code);
}

void SleepFor(int seconds) {
  VLOG(2) << "Sleeping " << seconds << " seconds...";
  base::WaitableEvent sleep(base::WaitableEvent::ResetPolicy::MANUAL,
                            base::WaitableEvent::InitialState::NOT_SIGNALED);
  base::ThreadPool::PostDelayedTask(
      FROM_HERE, {base::MayBlock()},
      base::BindOnce(&base::WaitableEvent::Signal, base::Unretained(&sleep)),
      base::TimeDelta::FromSeconds(seconds));
  sleep.Wait();
  VLOG(2) << "Sleep complete.";
}

class IntegrationTest
    : public ::testing::TestWithParam<scoped_refptr<IntegrationTestCommands>> {
 protected:
  void SetUp() override {
    test_commands_ = GetParam();
    Clean();
    ExpectClean();
    EnterTestMode(GURL("http://localhost:1234"));
  }

  void TearDown() override {
    if (::testing::Test::HasFailure())
      PrintLog();
    // TODO(crbug.com/1159189): Use a specific test output directory
    // because Uninstall() deletes the files under GetDataDirPath().
    CopyLog();
    ExpectClean();
    Clean();
  }
  scoped_refptr<IntegrationTestCommands> test_commands_;

  void CopyLog() { test_commands_->CopyLog(); }
  void PrintLog() { test_commands_->PrintLog(); }
  void Install() { test_commands_->Install(); }
  void ExpectInstalled() { test_commands_->ExpectInstalled(); }
  void Uninstall() { test_commands_->Uninstall(); }
  void ExpectCandidateUninstalled() {
    test_commands_->ExpectCandidateUninstalled();
  }
  void Clean() { test_commands_->Clean(); }
  void ExpectClean() { test_commands_->ExpectClean(); }
  void EnterTestMode(const GURL& url) { test_commands_->EnterTestMode(url); }
  void ExpectVersionActive(const std::string& version) {
    test_commands_->ExpectVersionActive(version);
  }
  void ExpectVersionNotActive(const std::string& version) {
    test_commands_->ExpectVersionNotActive(version);
  }
  void ExpectActiveUpdater() { test_commands_->ExpectActiveUpdater(); }
  void SetupFakeUpdaterHigherVersion() {
    test_commands_->SetupFakeUpdaterHigherVersion();
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
  void SetFakeExistenceCheckerPath(const std::string& app_id) {
    test_commands_->SetFakeExistenceCheckerPath(app_id);
  }
  void ExpectAppUnregisteredExistenceCheckerPath(const std::string& app_id) {
    test_commands_->ExpectAppUnregisteredExistenceCheckerPath(app_id);
  }
  void RegisterApp(const std::string& app_id) {
    test_commands_->RegisterApp(app_id);
  }
  void RegisterTestApp() { test_commands_->RegisterTestApp(); }
  void RunWake(int exit_code) { test_commands_->RunWake(exit_code); }
  UpdaterScope GetUpdaterScope() { return test_commands_->scope(); }

 private:
  base::test::TaskEnvironment environment_;
};

// The project's position is that component builds are not portable outside of
// the build directory. Therefore, installation of component builds is not
// expected to work and these tests do not run on component builders.
// See crbug.com/1112527.
#if defined(OS_WIN) || !defined(COMPONENT_BUILD)

TEST_P(IntegrationTest, InstallUninstall) {
  Install();
  ExpectInstalled();
  ExpectVersionActive(UPDATER_VERSION_STRING);
  ExpectActiveUpdater();
#if defined(OS_WIN)
  // Tests the COM registration after the install. For now, tests that the
  // COM interfaces are registered, which is indirectly testing the type
  // library separation for the public, private, and legacy interfaces.
  ExpectInterfacesRegistered();
#endif  // OS_WIN
  Uninstall();
}

TEST_P(IntegrationTest, SelfUninstallOutdatedUpdater) {
  // TODO(crbug.com/1096654): Enable for system.
  if (GetUpdaterScope() == UpdaterScope::kSystem) {
    Uninstall();
    return;
  }

  Install();
  ExpectInstalled();
  SetupFakeUpdaterHigherVersion();
  ExpectVersionNotActive(UPDATER_VERSION_STRING);

  RunWake(0);

  // The mac server will remain active for 10 seconds after it replies to the
  // wake client, then shut down and uninstall itself. Sleep to wait for this
  // to happen.
  SleepFor(13);

  ExpectCandidateUninstalled();
  // The candidate uninstall should not have altered global prefs.
  ExpectVersionNotActive(UPDATER_VERSION_STRING);
  ExpectVersionNotActive("0.0.0.0");

  Uninstall();
  Clean();
}

#if defined(OS_MAC)
// TODO(crbug.com/1163524): Enable on Windows.
TEST_P(IntegrationTest, RegisterTestApp) {
  RegisterTestApp();
  ExpectInstalled();
  ExpectVersionActive(UPDATER_VERSION_STRING);
  ExpectActiveUpdater();
  Uninstall();
}
#endif  // OS_MAC

// TODO(crbug.com/1163625): Failing on Mac 10.11.
TEST_P(IntegrationTest, ReportsActive) {
  // TODO(crbug.com/1096654): Enable for system.
  if (GetUpdaterScope() == UpdaterScope::kSystem) {
    Uninstall();
    return;
  }

  // A longer than usual timeout is needed for this test because the macOS
  // UpdateServiceInternal server takes at least 10 seconds to shut down after
  // Install, and RegisterApp cannot make progress until it shut downs and
  // releases the global prefs lock. We give it at most 18 seconds to be safe.
  base::test::ScopedRunLoopTimeout timeout(FROM_HERE,
                                           base::TimeDelta::FromSeconds(18));

  ScopedServer test_server;
  Install();
  ExpectInstalled();

  // Register apps test1 and test2. Expect registration pings for each.
  // TODO(crbug.com/1159525): Registration pings are currently not being sent.
  RegisterApp("test1");
  RegisterApp("test2");

  // Set test1 to be active and do a background updatecheck.
  SetActive("test1");
  ExpectActive("test1");
  ExpectNotActive("test2");
  test_server.ExpectOnce(
      R"(.*"appid":"test1","enabled":true,"ping":{"a":-2,.*)",
      R"()]}')"
      "\n"
      R"({"response":{"protocol":"3.1","daystart":{"elapsed_)"
      R"(days":5098}},"app":[{"appid":"test1","status":"ok",)"
      R"("updatecheck":{"status":"noupdate"}},{"appid":"test2",)"
      R"("status":"ok","updatecheck":{"status":"noupdate"}}]})");
  RunWake(0);

  // The updater has cleared the active bits.
  ExpectNotActive("test1");
  ExpectNotActive("test2");

  Uninstall();
}

#if defined(OS_MAC)
// TODO(https://crbug.com/1186583): Test failing frequently on Mac
#if defined(OS_MAC)
#define MAYBE_UnregisterUninstalledApp DISABLED_UnregisterUninstalledApp
#else
#define MAYBE_UnregisterUninstalledApp UnregisterUninstalledApp
#endif
TEST_P(IntegrationTest, MAYBE_UnregisterUninstalledApp) {
  RegisterTestApp();
  ExpectInstalled();
  ExpectVersionActive(UPDATER_VERSION_STRING);
  ExpectActiveUpdater();

  RegisterApp("test1");
  RegisterApp("test2");

  SetFakeExistenceCheckerPath(kTestAppId);

  RunWake(0);

  SleepFor(13);
  ExpectInstalled();

  ExpectAppUnregisteredExistenceCheckerPath(kTestAppId);

  Uninstall();
}

// TODO(https://crbug.com/1186583): Test failing frequently on Mac
#if defined(OS_MAC)
#define MAYBE_UninstallUpdaterWhenAllAppsUninstalled \
  DISABLED_UninstallUpdaterWhenAllAppsUninstalled
#else
#define MAYBE_UninstallUpdaterWhenAllAppsUninstalled \
  UninstallUpdaterWhenAllAppsUninstalled
#endif
TEST_P(IntegrationTest, MAYBE_UninstallUpdaterWhenAllAppsUninstalled) {
  RegisterTestApp();
  ExpectInstalled();
  ExpectVersionActive(UPDATER_VERSION_STRING);
  ExpectActiveUpdater();

  SetFakeExistenceCheckerPath(kTestAppId);

  RunWake(0);

  SleepFor(13);
}

// TODO(https://crbug.com/1166196): Fix flaky timeouts. The timeout is in
// RunWake(0).
#if defined(OS_MAC)
#define MAYBE_UnregisterUnownedApp DISABLED_UnregisterUnownedApp
#else
#define MAYBE_UnregisterUnownedApp UnregisterUnownedApp
#endif
TEST_P(IntegrationTest, MAYBE_UnregisterUnownedApp) {
  RegisterTestApp();
  ExpectInstalled();
  ExpectVersionActive(UPDATER_VERSION_STRING);
  ExpectActiveUpdater();

  {
    std::unique_ptr<GlobalPrefs> global_prefs = CreateGlobalPrefs();
    auto persisted_data =
        base::MakeRefCounted<PersistedData>(global_prefs->GetPrefService());
    base::FilePath fake_ecp{FILE_PATH_LITERAL("/Library")};
    persisted_data->SetExistenceCheckerPath(kTestAppId, fake_ecp);

    PrefsCommitPendingWrites(global_prefs->GetPrefService());

    EXPECT_EQ(fake_ecp.value(),
              persisted_data->GetExistenceCheckerPath(kTestAppId).value());
  }

  RunWake(0);

  {
    std::unique_ptr<GlobalPrefs> global_prefs = CreateGlobalPrefs();
    auto persisted_data =
        base::MakeRefCounted<PersistedData>(global_prefs->GetPrefService());
    EXPECT_EQ(base::FilePath(FILE_PATH_LITERAL("")).value(),
              persisted_data->GetExistenceCheckerPath(kTestAppId).value());
  }

  Uninstall();
}

#endif  // OS_MAC

#if defined(OS_MAC)
// TODO(crbug.com/1096654): Enable tests for IntegrationTestCommandsSystem on
// bots that support passwordless sudo.
INSTANTIATE_TEST_SUITE_P(
    IntegrationTestVariations,
    IntegrationTest,
    testing::Values(
        IntegrationTestCommandsUser::CreateIntegrationTestCommands()));
#endif

#if defined(OS_WIN)
// TODO(crbug.com/1096654): Enable for system.
INSTANTIATE_TEST_SUITE_P(
    IntegrationTestVariations,
    IntegrationTest,
    testing::Values(
        IntegrationTestCommandsUser::CreateIntegrationTestCommands()));
#endif

#endif  // defined(OS_WIN) || !defined(COMPONENT_BUILD)

}  // namespace test

}  // namespace updater
