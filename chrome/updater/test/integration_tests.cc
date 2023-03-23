// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <cstdlib>
#include <memory>
#include <string>

#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/scoped_refptr.h"
#include "base/run_loop.h"
#include "base/strings/stringprintf.h"
#include "base/test/bind.h"
#include "base/test/scoped_run_loop_timeout.h"
#include "base/test/task_environment.h"
#include "base/test/test_timeouts.h"
#include "base/time/time.h"
#include "base/values.h"
#include "base/version.h"
#include "build/branding_buildflags.h"
#include "build/build_config.h"
#include "build/buildflag.h"
#include "chrome/updater/constants.h"
#include "chrome/updater/ipc/ipc_support.h"
#include "chrome/updater/service_proxy_factory.h"
#include "chrome/updater/test/integration_test_commands.h"
#include "chrome/updater/test/integration_tests_impl.h"
#include "chrome/updater/test/server.h"
#include "chrome/updater/test_scope.h"
#include "chrome/updater/update_service.h"
#include "chrome/updater/updater_version.h"
#include "chrome/updater/util/unittest_util.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

#if BUILDFLAG(IS_LINUX)
#include <unistd.h>

#include "base/environment.h"
#include "base/strings/strcat.h"
#endif

#if BUILDFLAG(IS_WIN)
#include <shlobj.h>

#include "base/strings/utf_string_conversions.h"
#include "base/win/registry.h"
#include "chrome/updater/app/server/win/updater_legacy_idl.h"
#include "chrome/updater/test_scope.h"
#include "chrome/updater/util/win_util.h"
#include "chrome/updater/win/win_constants.h"
#endif  // BUILDFLAG(IS_WIN)

namespace updater::test {
namespace {

#if BUILDFLAG(IS_WIN) || !defined(COMPONENT_BUILD)

void ExpectNoUpdateSequence(ScopedServer* test_server,
                            const std::string& app_id) {
  test_server->ExpectOnce(
      {base::BindRepeating(
          RequestMatcherRegex,
          base::StringPrintf(R"(.*"appid":"%s".*)", app_id.c_str()))},
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

#endif  // BUILDFLAG(IS_WIN) || !defined(COMPONENT_BUILD)

}  // namespace

class IntegrationTest : public ::testing::Test {
 public:
  IntegrationTest() : test_commands_(CreateIntegrationTestCommands()) {}
  ~IntegrationTest() override = default;

 protected:
  void SetUp() override {
    ASSERT_NO_FATAL_FAILURE(CleanProcesses());
    ASSERT_TRUE(WaitForUpdaterExit());
    ASSERT_NO_FATAL_FAILURE(Clean());
    ASSERT_NO_FATAL_FAILURE(ExpectClean());
    // TODO(crbug.com/1233612) - reenable the code when system tests pass.
    // SetUpTestService();
    ASSERT_NO_FATAL_FAILURE(EnterTestMode(GURL("http://localhost:1234")));

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
  }

  void TearDown() override {
    ExitTestMode();
    if (!HasFailure()) {
      ExpectClean();
    }
    PrintLog();

    // TODO(crbug.com/1159189): Use a specific test output directory
    // because Uninstall() deletes the files under GetInstallDirectory().
    CopyLog();

    // TODO(crbug.com/1233612) - reenable the code when system tests pass.
    // TearDownTestService();

    // Updater process must not be running for `Clean()` to succeed.
    ASSERT_TRUE(WaitForUpdaterExit());
    Clean();
  }

  void CopyLog() { test_commands_->CopyLog(); }

  void PrintLog() { test_commands_->PrintLog(); }

  void Install() { test_commands_->Install(); }

  void InstallUpdaterAndApp(const std::string& app_id) {
    test_commands_->InstallUpdaterAndApp(app_id);
  }

  void ExpectInstalled() { test_commands_->ExpectInstalled(); }

  void Uninstall() {
    ASSERT_TRUE(WaitForUpdaterExit());
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

  void EnterTestMode(const GURL& url) { test_commands_->EnterTestMode(url); }

  void ExitTestMode() { test_commands_->ExitTestMode(); }

  void SetGroupPolicies(const base::Value::Dict& values) {
    test_commands_->SetGroupPolicies(values);
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
      int expected_error_code) {
    test_commands_->ExpectLegacyUpdate3WebSucceeds(
        app_id, app_bundle_web_create_mode, expected_final_state,
        expected_error_code);
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

  void ExpectAppVersion(const std::string& app_id,
                        const base::Version& version) {
    test_commands_->ExpectAppVersion(app_id, version);
  }

  void InstallApp(const std::string& app_id) {
    test_commands_->InstallApp(app_id);
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

  void RunWakeActive(int exit_code) {
    ASSERT_TRUE(WaitForUpdaterExit());
    test_commands_->RunWakeActive(exit_code);
  }

  void CheckForUpdate(const std::string& app_id) {
    test_commands_->CheckForUpdate(app_id);
  }

  void Update(const std::string& app_id,
              const std::string& install_data_index) {
    test_commands_->Update(app_id, install_data_index);
  }

  void UpdateAll() { test_commands_->UpdateAll(); }

  void DeleteUpdaterDirectory() { test_commands_->DeleteUpdaterDirectory(); }

  base::FilePath GetDifferentUserPath() {
    return test_commands_->GetDifferentUserPath();
  }

  [[nodiscard]] bool WaitForUpdaterExit() {
    return test_commands_->WaitForUpdaterExit();
  }

  void SetUpTestService() {
#if BUILDFLAG(IS_WIN)
    test_commands_->SetUpTestService();
#endif  // BUILDFLAG(IS_WIN)
  }

  void TearDownTestService() {
#if BUILDFLAG(IS_WIN)
    test_commands_->TearDownTestService();
#endif  // BUILDFLAG(IS_WIN)
  }

  void ExpectUpdateCheckSequence(ScopedServer* test_server,
                                 const std::string& app_id,
                                 UpdateService::Priority priority,
                                 const base::Version& from_version,
                                 const base::Version& to_version) {
    test_commands_->ExpectUpdateCheckSequence(test_server, app_id, priority,
                                              from_version, to_version);
  }

  void ExpectUpdateSequence(ScopedServer* test_server,
                            const std::string& app_id,
                            const std::string& install_data_index,
                            UpdateService::Priority priority,
                            const base::Version& from_version,
                            const base::Version& to_version) {
    test_commands_->ExpectUpdateSequence(test_server, app_id,
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
                             const base::Version& to_version) {
    test_commands_->ExpectInstallSequence(test_server, app_id,
                                          install_data_index, priority,
                                          from_version, to_version);
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

  void ExpectLegacyUpdaterMigrated() {
    test_commands_->ExpectLegacyUpdaterMigrated();
  }

  void RunRecoveryComponent(const std::string& app_id,
                            const base::Version& version) {
    test_commands_->RunRecoveryComponent(app_id, version);
  }

  void ExpectLastChecked() { test_commands_->ExpectLastChecked(); }

  void ExpectLastStarted() { test_commands_->ExpectLastStarted(); }

  void RunOfflineInstall(bool is_legacy_install, bool is_silent_install) {
    test_commands_->RunOfflineInstall(is_legacy_install, is_silent_install);
  }

  scoped_refptr<IntegrationTestCommands> test_commands_;

 private:
  base::test::TaskEnvironment environment_;
  ScopedIPCSupportWrapper ipc_support_;
};

// The project's position is that component builds are not portable outside of
// the build directory. Therefore, installation of component builds is not
// expected to work and these tests do not run on component builders.
// See crbug.com/1112527.
#if BUILDFLAG(IS_WIN) || !defined(COMPONENT_BUILD)

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

// TODO(crbug.com/1398845) Enable test once version-skewed updater is available
// for unbranded Linux.
#if !(BUILDFLAG(IS_LINUX) && BUILDFLAG(CHROMIUM_BRANDING))
TEST_F(IntegrationTest, OverinstallWorking) {
  ASSERT_NO_FATAL_FAILURE(SetupRealUpdaterLowerVersion());
  ASSERT_TRUE(WaitForUpdaterExit());
  ASSERT_NO_FATAL_FAILURE(ExpectVersionNotActive(kUpdaterVersion));

  // A new version hands off installation to the old version, and doesn't
  // change the active version of the updater.
  ASSERT_NO_FATAL_FAILURE(Install());
  ASSERT_TRUE(WaitForUpdaterExit());
  ASSERT_NO_FATAL_FAILURE(ExpectVersionNotActive(kUpdaterVersion));

  ASSERT_NO_FATAL_FAILURE(Uninstall());
}

TEST_F(IntegrationTest, OverinstallBroken) {
  ASSERT_NO_FATAL_FAILURE(SetupRealUpdaterLowerVersion());
  ASSERT_TRUE(WaitForUpdaterExit());
  ASSERT_NO_FATAL_FAILURE(DeleteUpdaterDirectory());

  // Since the old version is not working, the new version should install and
  // become active.
  ASSERT_NO_FATAL_FAILURE(Install());
  ASSERT_TRUE(WaitForUpdaterExit());
  ASSERT_NO_FATAL_FAILURE(ExpectVersionActive(kUpdaterVersion));

  ASSERT_NO_FATAL_FAILURE(Uninstall());

  // Cleanup the older version by reinstalling and uninstalling.
  ASSERT_NO_FATAL_FAILURE(SetupRealUpdaterLowerVersion());
  ASSERT_TRUE(WaitForUpdaterExit());
  ASSERT_NO_FATAL_FAILURE(Install());
  ASSERT_TRUE(WaitForUpdaterExit());
  ASSERT_NO_FATAL_FAILURE(Uninstall());
}
#endif  // !(BUILDFLAG(IS_LINUX) && BUILDFLAG(CHROMIUM_BRANDING))

TEST_F(IntegrationTest, SelfUninstallOutdatedUpdater) {
  ASSERT_NO_FATAL_FAILURE(Install());
  ASSERT_NO_FATAL_FAILURE(ExpectInstalled());
  ASSERT_TRUE(WaitForUpdaterExit());
  ASSERT_NO_FATAL_FAILURE(SetupFakeUpdaterHigherVersion());
  ASSERT_NO_FATAL_FAILURE(ExpectVersionNotActive(kUpdaterVersion));

  ASSERT_NO_FATAL_FAILURE(RunWake(0));
  ASSERT_TRUE(WaitForUpdaterExit());

  ASSERT_NO_FATAL_FAILURE(ExpectCandidateUninstalled());
  // The candidate uninstall should not have altered global prefs.
  ASSERT_NO_FATAL_FAILURE(ExpectVersionNotActive(kUpdaterVersion));
  ASSERT_NO_FATAL_FAILURE(ExpectVersionNotActive("0.0.0.0"));

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
  test_server.ExpectOnce(
      {base::BindRepeating(RequestMatcherRegex,
                           base::StringPrintf(".*%s.*", kUpdaterAppId))},
      ")]}'\n");
  ASSERT_NO_FATAL_FAILURE(RunWake(0));
  ASSERT_TRUE(WaitForUpdaterExit());
  ASSERT_NO_FATAL_FAILURE(ExpectVersionActive(kUpdaterVersion));

  ASSERT_NO_FATAL_FAILURE(Uninstall());
}

TEST_F(IntegrationTest, SelfUpdate) {
  ScopedServer test_server(test_commands_);
  ASSERT_NO_FATAL_FAILURE(Install());

  base::Version next_version(base::StringPrintf("%s1", kUpdaterVersion));
  ASSERT_NO_FATAL_FAILURE(ExpectUpdateSequence(
      &test_server, kUpdaterAppId, "", UpdateService::Priority::kBackground,
      base::Version(kUpdaterVersion), next_version));

  ASSERT_NO_FATAL_FAILURE(RunWake(0));
  ASSERT_TRUE(WaitForUpdaterExit());
  ASSERT_NO_FATAL_FAILURE(ExpectAppVersion(kUpdaterAppId, next_version));

  ASSERT_NO_FATAL_FAILURE(Uninstall());
}

TEST_F(IntegrationTest, SelfUpdateWithWakeAll) {
  ScopedServer test_server(test_commands_);
  ASSERT_NO_FATAL_FAILURE(Install());

  base::Version next_version(base::StringPrintf("%s1", kUpdaterVersion));
  ASSERT_NO_FATAL_FAILURE(ExpectUpdateSequence(
      &test_server, kUpdaterAppId, "", UpdateService::Priority::kBackground,
      base::Version(kUpdaterVersion), next_version));

  ASSERT_NO_FATAL_FAILURE(RunWakeAll());
  ASSERT_TRUE(WaitForUpdaterExit());
  ASSERT_NO_FATAL_FAILURE(ExpectAppVersion(kUpdaterAppId, next_version));

  ASSERT_NO_FATAL_FAILURE(Uninstall());
}

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
      {base::BindRepeating(
          RequestMatcherRegex,
          R"(.*"appid":"test1","enabled":true,"ping":{"a":-2,.*)")},
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

  ASSERT_NO_FATAL_FAILURE(Uninstall());
}

// Tests calling `CheckForUpdate` when the updater is not installed.
TEST_F(IntegrationTest, CheckForUpdate_UpdaterNotInstalled) {
  scoped_refptr<UpdateService> update_service =
      CreateUpdateServiceProxy(GetTestScope());
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
#if BUILDFLAG(IS_WIN)
  // TODO(crbug.com/1425609): Remove procmon logging once bug is fixed.
  const base::ScopedClosureRunner stop_procmon_logging(
      base::BindOnce(&updater::test::StopProcmonLogging,
                     updater::test::StartProcmonLogging()));
#endif  // #if BUILDFLAG(IS_WIN)

  ScopedServer test_server(test_commands_);
  ASSERT_NO_FATAL_FAILURE(Install());

  const std::string kAppId("test");
  ASSERT_NO_FATAL_FAILURE(InstallApp(kAppId));
  ASSERT_NO_FATAL_FAILURE(ExpectUpdateCheckSequence(
      &test_server, kAppId, UpdateService::Priority::kForeground,
      base::Version("0.1"), base::Version("1")));
  ASSERT_NO_FATAL_FAILURE(CheckForUpdate(kAppId));

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

  ASSERT_NO_FATAL_FAILURE(Uninstall());
}

#if BUILDFLAG(IS_WIN)  // TODO(crbug.com/1422385): fix for mac and linux.
TEST_F(IntegrationTest, InstallUpdaterAndApp) {
  ScopedServer test_server(test_commands_);
  const std::string kAppId("test");
  const base::Version v1("1");
  ASSERT_NO_FATAL_FAILURE(ExpectInstallSequence(
      &test_server, kAppId, "", UpdateService::Priority::kForeground,
      base::Version({0, 0, 0, 0}), v1));

  ASSERT_NO_FATAL_FAILURE(InstallUpdaterAndApp(kAppId));
  ASSERT_TRUE(WaitForUpdaterExit());

  ASSERT_NO_FATAL_FAILURE(ExpectAppVersion(kAppId, v1));

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

  ASSERT_NO_FATAL_FAILURE(Uninstall());
}

TEST_F(IntegrationTest, ForceInstallApp) {
  ScopedServer test_server(test_commands_);
  ASSERT_NO_FATAL_FAILURE(Install());

  base::Value::Dict group_policies;
  group_policies.Set("Installtest1", IsSystemInstall(GetTestScope())
                                         ? kPolicyForceInstallMachine
                                         : kPolicyForceInstallUser);
  ASSERT_NO_FATAL_FAILURE(SetGroupPolicies(group_policies));

  const std::string kAppId("test1");
  base::Version v0point1("0.1");
  base::Version v1("1");
  ASSERT_NO_FATAL_FAILURE(ExpectUpdateSequence(
      &test_server, kAppId, "", UpdateService::Priority::kBackground,
      base::Version("0.0.0.0"), v0point1));
  ASSERT_NO_FATAL_FAILURE(
      ExpectUpdateSequence(&test_server, kAppId, "",
                           UpdateService::Priority::kBackground, v0point1, v1));
  ASSERT_NO_FATAL_FAILURE(RunWake(0));

  ASSERT_TRUE(WaitForUpdaterExit());
  ASSERT_NO_FATAL_FAILURE(ExpectAppVersion(kAppId, v1));

  ASSERT_NO_FATAL_FAILURE(Uninstall());
}
#endif  // BUILDFLAG(IS_WIN)

TEST_F(IntegrationTest, MultipleWakesOneNetRequest) {
  ScopedServer test_server(test_commands_);
  ASSERT_NO_FATAL_FAILURE(Install());

  // Only one sequence visible to the server despite multiple wakes.
  ASSERT_NO_FATAL_FAILURE(ExpectNoUpdateSequence(&test_server, kUpdaterAppId));
  ASSERT_NO_FATAL_FAILURE(RunWake(0));
  ASSERT_NO_FATAL_FAILURE(RunWake(0));

  ASSERT_NO_FATAL_FAILURE(Uninstall());
}

TEST_F(IntegrationTest, MultipleUpdateAllsMultipleNetRequests) {
  ScopedServer test_server(test_commands_);
  ASSERT_NO_FATAL_FAILURE(Install());

  ASSERT_NO_FATAL_FAILURE(ExpectNoUpdateSequence(&test_server, kUpdaterAppId));
  ASSERT_NO_FATAL_FAILURE(UpdateAll());
  ASSERT_NO_FATAL_FAILURE(ExpectNoUpdateSequence(&test_server, kUpdaterAppId));
  ASSERT_NO_FATAL_FAILURE(UpdateAll());

  ASSERT_NO_FATAL_FAILURE(Uninstall());
}

#if BUILDFLAG(IS_WIN)
TEST_F(IntegrationTest, MarshalInterface) {
  ASSERT_NO_FATAL_FAILURE(Install());
  ASSERT_NO_FATAL_FAILURE(ExpectMarshalInterfaceSucceeds());
  ASSERT_NO_FATAL_FAILURE(Uninstall());
}

TEST_F(IntegrationTest, LegacyProcessLauncher) {
  ASSERT_NO_FATAL_FAILURE(Install());
  ASSERT_NO_FATAL_FAILURE(ExpectLegacyProcessLauncherSucceeds());
  ASSERT_NO_FATAL_FAILURE(Uninstall());
}

TEST_F(IntegrationTest, LegacyAppCommandWeb) {
  ASSERT_NO_FATAL_FAILURE(Install());

  const char kAppId[] = "test1";
  ASSERT_NO_FATAL_FAILURE(InstallApp(kAppId));

  base::Value::List parameters;
  parameters.Append("5432");
  ASSERT_NO_FATAL_FAILURE(
      ExpectLegacyAppCommandWebSucceeds(kAppId, "command1", parameters, 5432));

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
#endif  // BUILDFLAG(IS_WIN)

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
  // TODO(crbug.com/1287235): The test is flaky without the following line.
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

// Windows does not currently have a concept of app ownership, so this
// test need not run on Windows.
#if BUILDFLAG(IS_MAC)
TEST_F(IntegrationTest, UnregisterUnownedApp) {
  ASSERT_NO_FATAL_FAILURE(Install());
  ASSERT_NO_FATAL_FAILURE(ExpectInstalled());
  ASSERT_NO_FATAL_FAILURE(ExpectVersionActive(kUpdaterVersion));

  ASSERT_NO_FATAL_FAILURE(InstallApp("test1"));
  ASSERT_NO_FATAL_FAILURE(InstallApp("test2"));
  ASSERT_TRUE(WaitForUpdaterExit());

  ASSERT_NO_FATAL_FAILURE(
      SetExistenceCheckerPath("test1", GetDifferentUserPath()));

  ASSERT_NO_FATAL_FAILURE(RunWake(0));
  ASSERT_TRUE(WaitForUpdaterExit());

  ASSERT_NO_FATAL_FAILURE(ExpectNotRegistered("test1"));
  ASSERT_NO_FATAL_FAILURE(ExpectRegistered("test2"));

  ASSERT_NO_FATAL_FAILURE(Uninstall());
}
#endif  // BUILDFLAG(IS_MAC)

#if BUILDFLAG(CHROMIUM_BRANDING) || BUILDFLAG(GOOGLE_CHROME_BRANDING)
#if !defined(COMPONENT_BUILD)
// TODO(crbug.com/1398845) Enable test once version-skewed updater is available
// for unbranded Linux.
#if !BUILDFLAG(IS_LINUX) || BUILDFLAG(GOOGLE_CHROME_BRANDING)
// TODO(crbug.com/1097297) Enable these tests once the `Brand the updater and
// qualification app ids` change is available on CIPD.
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
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

  ASSERT_NO_FATAL_FAILURE(SetServerStarts(24));
  ASSERT_NO_FATAL_FAILURE(RunWake(0));
  ASSERT_TRUE(WaitForUpdaterExit());

  // Expect that the updater uninstalled itself as well as the lower version.
}
#endif  // #if BUILDFLAG(GOOGLE_CHROME_BRANDING) TODO(crbug.com/1097297) Enable
        // these tests once the `Brand the updater and qualification app ids`
        // change is available on CIPD.

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

#endif  // !BUILDFLAG(IS_LINUX) || BUILDFLAG(GOOGLE_CHROME_BRANDING)
#endif
#endif

TEST_F(IntegrationTest, UpdateServiceStress) {
  ASSERT_NO_FATAL_FAILURE(Install());
  ASSERT_NO_FATAL_FAILURE(ExpectInstalled());
  ASSERT_NO_FATAL_FAILURE(StressUpdateService());
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
      {base::BindRepeating(
          RequestMatcherRegex,
          R"(.*"updatecheck":{"sameversionupdate":true},"version":"0.1"}.*)")},
      response);
  ASSERT_NO_FATAL_FAILURE(CallServiceUpdate(
      app_id, "", UpdateService::PolicySameVersionUpdate::kAllowed));

  test_server.ExpectOnce(
      {base::BindRepeating(RequestMatcherRegex,
                           R"(.*"updatecheck":{},"version":"0.1"}.*)")},
      response);
  ASSERT_NO_FATAL_FAILURE(CallServiceUpdate(
      app_id, "", UpdateService::PolicySameVersionUpdate::kNotAllowed));
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
      {base::BindRepeating(
          RequestMatcherRegex,
          base::StringPrintf(
              R"(.*"data":\[{"index":"%s","name":"install"}],.*)",
              install_data_index.c_str()))},
      response);

  ASSERT_NO_FATAL_FAILURE(
      CallServiceUpdate(app_id, install_data_index,
                        UpdateService::PolicySameVersionUpdate::kAllowed));

  ASSERT_NO_FATAL_FAILURE(Uninstall());
}

TEST_F(IntegrationTest, MigrateLegacyUpdater) {
  ASSERT_NO_FATAL_FAILURE(SetupFakeLegacyUpdater());
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

TEST_F(IntegrationTest, OfflineInstall) {
  ASSERT_NO_FATAL_FAILURE(Install());
  ASSERT_NO_FATAL_FAILURE(ExpectInstalled());
  ASSERT_NO_FATAL_FAILURE(RunOfflineInstall(/*is_legacy_install=*/false,
                                            /*is_silent_install=*/false));
  ASSERT_NO_FATAL_FAILURE(Uninstall());
}

TEST_F(IntegrationTest, SilentOfflineInstall) {
  ASSERT_NO_FATAL_FAILURE(Install());
  ASSERT_NO_FATAL_FAILURE(ExpectInstalled());
  ASSERT_NO_FATAL_FAILURE(RunOfflineInstall(/*is_legacy_install=*/false,
                                            /*is_silent_install=*/true));
  ASSERT_NO_FATAL_FAILURE(Uninstall());
}

TEST_F(IntegrationTest, LegacySilentOfflineInstall) {
  ASSERT_NO_FATAL_FAILURE(Install());
  ASSERT_NO_FATAL_FAILURE(ExpectInstalled());
  ASSERT_NO_FATAL_FAILURE(RunOfflineInstall(/*is_legacy_install=*/true,
                                            /*is_silent_install=*/true));
  ASSERT_NO_FATAL_FAILURE(Uninstall());
}

#if BUILDFLAG(IS_WIN)
class IntegrationTestLegacyUpdate3Web : public IntegrationTest {
 public:
  IntegrationTestLegacyUpdate3Web() = default;
  ~IntegrationTestLegacyUpdate3Web() override = default;

 protected:
  void SetUp() override {
    IntegrationTest::SetUp();

    test_server_ = std::make_unique<ScopedServer>(test_commands_);
    ASSERT_NO_FATAL_FAILURE(Install());
    ASSERT_NO_FATAL_FAILURE(InstallApp(kAppId));
  }

  void TearDown() override {
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
  base::Value::Dict group_policies;
  group_policies.Set("Updatetest1", kPolicyAutomaticUpdatesOnly);
  ASSERT_NO_FATAL_FAILURE(SetGroupPolicies(group_policies));
  ASSERT_NO_FATAL_FAILURE(ExpectLegacyUpdate3WebSucceeds(
      kAppId, AppBundleWebCreateMode::kCreateInstalledApp, STATE_ERROR,
      GOOPDATE_E_APP_UPDATE_DISABLED_BY_POLICY_MANUAL));
}

TEST_F(IntegrationTestLegacyUpdate3Web, DisabledPolicy) {
  base::Value::Dict group_policies;
  group_policies.Set("Updatetest1", kPolicyDisabled);
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
  ASSERT_NO_FATAL_FAILURE(ExpectUpdateSequence(
      test_server_.get(), kAppId, "", UpdateService::Priority::kForeground,
      base::Version("0.1"), base::Version("0.1")));
  ASSERT_NO_FATAL_FAILURE(
      ExpectLegacyUpdate3WebSucceeds(kAppId, AppBundleWebCreateMode::kCreateApp,
                                     STATE_INSTALL_COMPLETE, S_OK));
}
#endif  // BUILDFLAG(IS_WIN)

#endif  // BUILDFLAG(IS_WIN) || !defined(COMPONENT_BUILD)

}  // namespace updater::test
