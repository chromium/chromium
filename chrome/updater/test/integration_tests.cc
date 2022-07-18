// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <cstdlib>
#include <memory>
#include <string>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/logging.h"
#include "base/memory/scoped_refptr.h"
#include "base/numerics/checked_math.h"
#include "base/process/launch.h"
#include "base/process/process.h"
#include "base/strings/strcat.h"
#include "base/strings/stringprintf.h"
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
#include "chrome/updater/persisted_data.h"
#include "chrome/updater/prefs.h"
#include "chrome/updater/registration_data.h"
#include "chrome/updater/test/integration_test_commands.h"
#include "chrome/updater/test/integration_tests_impl.h"
#include "chrome/updater/test/server.h"
#include "chrome/updater/update_service.h"
#include "chrome/updater/updater_scope.h"
#include "chrome/updater/updater_version.h"
#include "chrome/updater/util.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

#if BUILDFLAG(IS_WIN)
#include <shlobj.h>

#include "base/strings/utf_string_conversions.h"
#include "base/win/registry.h"
#include "chrome/updater/app/server/win/updater_legacy_idl.h"
#include "chrome/updater/win/win_constants.h"
#include "chrome/updater/win/win_util.h"
#endif  // BUILDFLAG(IS_WIN)

namespace updater {
namespace test {
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
    logging::SetLogItems(true,    // enable_process_id
                         true,    // enable_thread_id
                         true,    // enable_timestamp
                         false);  // enable_tickcount
    Clean();
    ExpectClean();
    // TODO(crbug.com/1233612) - reenable the code when system tests pass.
    // SetUpTestService();
    EnterTestMode(GURL("http://localhost:1234"));
  }

  void TearDown() override {
    ExpectClean();
    PrintLog();
    // TODO(crbug.com/1159189): Use a specific test output directory
    // because Uninstall() deletes the files under GetDataDirPath().
    CopyLog();
    // TODO(crbug.com/1233612) - reenable the code when system tests pass.
    // TearDownTestService();
    Clean();
  }

  void CopyLog() { test_commands_->CopyLog(); }

  void PrintLog() { test_commands_->PrintLog(); }

  void Install() { test_commands_->Install(); }

  void ExpectInstalled() { test_commands_->ExpectInstalled(); }

  void Uninstall() {
    PrintLog();
    CopyLog();
    test_commands_->Uninstall();
    WaitForUpdaterExit();
  }

  void ExpectCandidateUninstalled() {
    test_commands_->ExpectCandidateUninstalled();
  }

  void Clean() { test_commands_->Clean(); }

  void ExpectClean() { test_commands_->ExpectClean(); }

  void EnterTestMode(const GURL& url) { test_commands_->EnterTestMode(url); }

  void SetGroupPolicies(const base::Value::Dict& values) {
    test_commands_->SetGroupPolicies(values);
  }

  void ExpectVersionActive(const std::string& version) {
    test_commands_->ExpectVersionActive(version);
  }

  void ExpectVersionNotActive(const std::string& version) {
    test_commands_->ExpectVersionNotActive(version);
  }

  void ExpectActiveUpdater() { test_commands_->ExpectActiveUpdater(); }

#if BUILDFLAG(IS_WIN)
  void ExpectInterfacesRegistered() {
    test_commands_->ExpectInterfacesRegistered();
  }

  void ExpectLegacyUpdate3WebSucceeds(const std::string& app_id,
                                      int expected_final_state,
                                      int expected_error_code) {
    test_commands_->ExpectLegacyUpdate3WebSucceeds(app_id, expected_final_state,
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

  void RunUninstallCmdLine() { test_commands_->RunUninstallCmdLine(); }
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

  void RunWake(int exit_code) { test_commands_->RunWake(exit_code); }

  void RunWakeActive(int exit_code) {
    test_commands_->RunWakeActive(exit_code);
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

  void WaitForUpdaterExit() { test_commands_->WaitForUpdaterExit(); }

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

  void ExpectUpdateSequence(ScopedServer* test_server,
                            const std::string& app_id,
                            const std::string& install_data_index,
                            const base::Version& from_version,
                            const base::Version& to_version) {
    test_commands_->ExpectUpdateSequence(
        test_server, app_id, install_data_index, from_version, to_version);
  }

  void ExpectSelfUpdateSequence(ScopedServer* test_server) {
    test_commands_->ExpectSelfUpdateSequence(test_server);
  }

  void ExpectInstallEvent(ScopedServer* test_server,
                          const std::string& app_id) {
    test_server->ExpectOnce(
        {base::BindRepeating(
            RequestMatcherRegex,
            base::StrCat({R"(.*"appid":")", app_id, R"(","enabled":true,")",
                          R"(event":\[{"eventresult":1,"eventtype":2,.*)"}))},
        "");
  }

  void StressUpdateService() { test_commands_->StressUpdateService(); }

  void CallServiceUpdate(
      const std::string& app_id,
      const std::string& install_data_index,
      UpdateService::PolicySameVersionUpdate policy_same_version_update) {
    test_commands_->CallServiceUpdate(app_id, install_data_index,
                                      policy_same_version_update);
  }

  void SetupFakeLegacyUpdaterData() {
    test_commands_->SetupFakeLegacyUpdaterData();
  }

  void ExpectLegacyUpdaterDataMigrated() {
    test_commands_->ExpectLegacyUpdaterDataMigrated();
  }

  void RunRecoveryComponent(const std::string& app_id,
                            const base::Version& version) {
    test_commands_->RunRecoveryComponent(app_id, version);
  }

  void ExpectLastChecked() { test_commands_->ExpectLastChecked(); }

  void ExpectLastStarted() { test_commands_->ExpectLastStarted(); }

  void RunOfflineInstall() { test_commands_->RunOfflineInstall(); }

  scoped_refptr<IntegrationTestCommands> test_commands_;

 private:
  base::test::TaskEnvironment environment_;
};

// The project's position is that component builds are not portable outside of
// the build directory. Therefore, installation of component builds is not
// expected to work and these tests do not run on component builders.
// See crbug.com/1112527.
#if BUILDFLAG(IS_WIN) || !defined(COMPONENT_BUILD)

TEST_F(IntegrationTest, InstallUninstall) {
  Install();
  WaitForUpdaterExit();
  ExpectInstalled();
  ExpectVersionActive(kUpdaterVersion);
  ExpectActiveUpdater();
#if BUILDFLAG(IS_WIN)
  // Tests the COM registration after the install. For now, tests that the
  // COM interfaces are registered, which is indirectly testing the type
  // library separation for the public, private, and legacy interfaces.
  ExpectInterfacesRegistered();
#endif  // BUILDFLAG(IS_WIN)
  Uninstall();
}

#if BUILDFLAG(IS_MAC)
TEST_F(IntegrationTest, OverinstallWorking) {
#else
TEST_F(IntegrationTest, DISABLED_OverinstallWorking) {
#endif
  SetupRealUpdaterLowerVersion();
  WaitForUpdaterExit();
  ExpectVersionNotActive(kUpdaterVersion);

  // A new version hands off installation to the old version, and doesn't
  // change the active version of the updater.
  Install();
  WaitForUpdaterExit();
  ExpectVersionNotActive(kUpdaterVersion);

  Uninstall();
}

// TODO(https://crbug.com/1344846): Flaky on Mac.
#if BUILDFLAG(IS_MAC)
TEST_F(IntegrationTest, DISABLED_OverinstallBroken) {
#else
TEST_F(IntegrationTest, DISABLED_OverinstallBroken) {
#endif
  SetupRealUpdaterLowerVersion();
  WaitForUpdaterExit();
  DeleteUpdaterDirectory();

  // Since the old version is not working, the new version should install and
  // become active.
  Install();
  WaitForUpdaterExit();
  ExpectVersionActive(kUpdaterVersion);

  Uninstall();
}

TEST_F(IntegrationTest, SelfUninstallOutdatedUpdater) {
  Install();
  ExpectInstalled();
  WaitForUpdaterExit();
  SetupFakeUpdaterHigherVersion();
  ExpectVersionNotActive(kUpdaterVersion);

  RunWake(0);
  WaitForUpdaterExit();

  ExpectCandidateUninstalled();
  // The candidate uninstall should not have altered global prefs.
  ExpectVersionNotActive(kUpdaterVersion);
  ExpectVersionNotActive("0.0.0.0");

  Uninstall();
  Clean();
}

TEST_F(IntegrationTest, QualifyUpdater) {
  ScopedServer test_server(test_commands_);
  Install();
  ExpectInstalled();
  WaitForUpdaterExit();
  SetupFakeUpdaterLowerVersion();
  ExpectVersionNotActive(kUpdaterVersion);

  ExpectUpdateSequence(&test_server, kQualificationAppId, "",
                       base::Version("0.1"), base::Version("0.2"));

  RunWake(0);
  WaitForUpdaterExit();

  // This instance is now qualified and should activate itself and check itself
  // for updates on the next check.
  test_server.ExpectOnce(
      {base::BindRepeating(RequestMatcherRegex,
                           base::StringPrintf(".*%s.*", kUpdaterAppId))},
      ")]}'\n");
  RunWake(0);
  WaitForUpdaterExit();
  ExpectVersionActive(kUpdaterVersion);

  Uninstall();
  Clean();
}

TEST_F(IntegrationTest, SelfUpdate) {
  ScopedServer test_server(test_commands_);
  Install();

  base::Version next_version(base::StringPrintf("%s1", kUpdaterVersion));
  ExpectUpdateSequence(&test_server, kUpdaterAppId, "",
                       base::Version(kUpdaterVersion), next_version);

  RunWake(0);
  WaitForUpdaterExit();
  ExpectAppVersion(kUpdaterAppId, next_version);

  Uninstall();
  Clean();
}

TEST_F(IntegrationTest, ReportsActive) {
  // A longer than usual timeout is needed for this test because the macOS
  // UpdateServiceInternal server takes at least 10 seconds to shut down after
  // Install, and InstallApp cannot make progress until it shut downs and
  // releases the global prefs lock. We give it at most 18 seconds to be safe.
  base::test::ScopedRunLoopTimeout timeout(FROM_HERE, base::Seconds(18));

  ScopedServer test_server(test_commands_);
  Install();
  ExpectInstalled();

  // Register apps test1 and test2. Expect pings for each.
  InstallApp("test1");
  InstallApp("test2");

  // Set test1 to be active and do a background updatecheck.
  SetActive("test1");
  ExpectActive("test1");
  ExpectNotActive("test2");
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
  RunWake(0);

  // The updater has cleared the active bits.
  ExpectNotActive("test1");
  ExpectNotActive("test2");

  Uninstall();
}

TEST_F(IntegrationTest, UpdateApp) {
  ScopedServer test_server(test_commands_);
  Install();

  const std::string kAppId("test");
  InstallApp(kAppId);
  base::Version v1("1");
  ExpectUpdateSequence(&test_server, kAppId, "", base::Version("0.1"), v1);
  RunWake(0);

  base::Version v2("2");
  const std::string kInstallDataIndex("test_install_data_index");
  ExpectUpdateSequence(&test_server, kAppId, kInstallDataIndex, v1, v2);
  Update(kAppId, kInstallDataIndex);
  WaitForUpdaterExit();
  ExpectAppVersion(kAppId, v2);
  ExpectLastChecked();
  ExpectLastStarted();

  Uninstall();
  Clean();
}

TEST_F(IntegrationTest, MultipleWakesOneNetRequest) {
  ScopedServer test_server(test_commands_);
  Install();

  // Only one sequence visible to the server despite multiple wakes.
  ExpectNoUpdateSequence(&test_server, kUpdaterAppId);
  RunWake(0);
  RunWake(0);

  Uninstall();
  Clean();
}

TEST_F(IntegrationTest, MultipleUpdateAllsMultipleNetRequests) {
  ScopedServer test_server(test_commands_);
  Install();

  ExpectNoUpdateSequence(&test_server, kUpdaterAppId);
  UpdateAll();
  ExpectNoUpdateSequence(&test_server, kUpdaterAppId);
  UpdateAll();

  Uninstall();
  Clean();
}

#if BUILDFLAG(IS_WIN) && BUILDFLAG(GOOGLE_CHROME_BRANDING)
TEST_F(IntegrationTest, LegacyUpdate3Web) {
  ScopedServer test_server(test_commands_);
  Install();

  const char kAppId[] = "test1";
  InstallApp(kAppId);

  ExpectNoUpdateSequence(&test_server, kAppId);
  ExpectLegacyUpdate3WebSucceeds(kAppId, STATE_NO_UPDATE, S_OK);

  base::Value::Dict group_policies;
  group_policies.Set("Updatetest1", kPolicyAutomaticUpdatesOnly);
  SetGroupPolicies(group_policies);
  ExpectLegacyUpdate3WebSucceeds(
      kAppId, STATE_ERROR, GOOPDATE_E_APP_UPDATE_DISABLED_BY_POLICY_MANUAL);

  group_policies.Set("Updatetest1", kPolicyDisabled);
  SetGroupPolicies(group_policies);
  ExpectLegacyUpdate3WebSucceeds(kAppId, STATE_ERROR,
                                 GOOPDATE_E_APP_UPDATE_DISABLED_BY_POLICY);

  group_policies.clear();
  SetGroupPolicies(group_policies);
  ExpectUpdateSequence(&test_server, kAppId, "", base::Version("0.1"),
                       base::Version("0.2"));
  ExpectLegacyUpdate3WebSucceeds(kAppId, STATE_INSTALL_COMPLETE, S_OK);

  Uninstall();
}

TEST_F(IntegrationTest, LegacyProcessLauncher) {
  Install();
  ExpectLegacyProcessLauncherSucceeds();
  Uninstall();
}

TEST_F(IntegrationTest, LegacyAppCommandWeb) {
  Install();

  const char kAppId[] = "test1";
  InstallApp(kAppId);

  base::Value::List parameters;
  parameters.Append("5432");
  ExpectLegacyAppCommandWebSucceeds(kAppId, "command1", parameters, 5432);

  Uninstall();
}

TEST_F(IntegrationTest, UninstallCmdLine) {
  Install();
  ExpectInstalled();
  ExpectVersionActive(kUpdaterVersion);
  ExpectActiveUpdater();

  // Running the uninstall command does not uninstall this instance of the
  // updater right after installing it (not enough server starts).
  RunUninstallCmdLine();
  WaitForUpdaterExit();
  ExpectInstalled();

  SetServerStarts(24);

  // Uninstall the idle updater.
  RunUninstallCmdLine();
  WaitForUpdaterExit();
  ExpectClean();
}
#endif  // BUILDFLAG(IS_WIN) && BUILDFLAG(GOOGLE_CHROME_BRANDING)

TEST_F(IntegrationTest, UnregisterUninstalledApp) {
  Install();
  ExpectInstalled();
  InstallApp("test1");
  InstallApp("test2");

  WaitForUpdaterExit();
  ExpectVersionActive(kUpdaterVersion);
  ExpectActiveUpdater();
  UninstallApp("test1");

  RunWake(0);

  WaitForUpdaterExit();
  ExpectInstalled();
  ExpectNotRegistered("test1");
  ExpectRegistered("test2");

  Uninstall();
}

TEST_F(IntegrationTest, UninstallIfMaxServerWakesBeforeRegistrationExceeded) {
  Install();
  WaitForUpdaterExit();
  ExpectInstalled();
  SetServerStarts(24);
  RunWake(0);
  WaitForUpdaterExit();
  ExpectClean();
}

TEST_F(IntegrationTest, UninstallUpdaterWhenAllAppsUninstalled) {
  Install();
  InstallApp("test1");
  ExpectInstalled();
  WaitForUpdaterExit();
  // TODO(crbug.com/1287235): The test is flaky without the following line.
  SetServerStarts(24);
  RunWake(0);
  WaitForUpdaterExit();
  ExpectInstalled();
  ExpectVersionActive(kUpdaterVersion);
  ExpectActiveUpdater();
  UninstallApp("test1");
  RunWake(0);
  WaitForUpdaterExit();
  ExpectClean();
}

// Windows does not currently have a concept of app ownership, so this
// test need not run on Windows.
#if BUILDFLAG(IS_MAC)
TEST_F(IntegrationTest, UnregisterUnownedApp) {
  Install();
  ExpectInstalled();
  ExpectVersionActive(kUpdaterVersion);
  ExpectActiveUpdater();

  InstallApp("test1");
  InstallApp("test2");
  WaitForUpdaterExit();

  SetExistenceCheckerPath("test1", GetDifferentUserPath());

  RunWake(0);
  WaitForUpdaterExit();

  ExpectNotRegistered("test1");
  ExpectRegistered("test2");

  Uninstall();
}
#endif  // BUILDFLAG(IS_MAC)

#if BUILDFLAG(CHROMIUM_BRANDING) || BUILDFLAG(GOOGLE_CHROME_BRANDING)
#if !defined(COMPONENT_BUILD)
#if BUILDFLAG(IS_MAC)
TEST_F(IntegrationTest, SelfUpdateFromOldReal) {
#else
TEST_F(IntegrationTest, DISABLED_SelfUpdateFromOldReal) {
#endif
  ScopedServer test_server(test_commands_);

  SetupRealUpdaterLowerVersion();
  ExpectVersionNotActive(kUpdaterVersion);

  // Trigger an old instance update check.
  ExpectSelfUpdateSequence(&test_server);
  RunWakeActive(0);

  // Qualify the new instance.
  ExpectUpdateSequence(&test_server, kQualificationAppId, "",
                       base::Version("0.1"), base::Version("0.2"));
  RunWake(0);
  WaitForUpdaterExit();

  // Activate the new instance. (It should not check itself for updates.)
  RunWake(0);
  WaitForUpdaterExit();

  ExpectVersionActive(kUpdaterVersion);
  Uninstall();
}
#endif
#endif

// TODO(crbug.com/1336591) - enable test after investigating crbug.com/1336591
// or open a new crbug for debugging this test if it is the culprit.
#if BUILDFLAG(IS_WIN)
#define MAYBE_UpdateServiceStress DISABLED_UpdateServiceStress
#else
#define MAYBE_UpdateServiceStress UpdateServiceStress
#endif
TEST_F(IntegrationTest, MAYBE_UpdateServiceStress) {
  Install();
  ExpectInstalled();
  StressUpdateService();
  Uninstall();
}

TEST_F(IntegrationTest, SameVersionUpdate) {
  ScopedServer test_server(test_commands_);
  Install();
  ExpectInstalled();

  const std::string app_id = "test-appid";
  InstallApp(app_id);

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
  CallServiceUpdate(app_id, "",
                    UpdateService::PolicySameVersionUpdate::kAllowed);

  test_server.ExpectOnce(
      {base::BindRepeating(RequestMatcherRegex,
                           R"(.*"updatecheck":{},"version":"0.1"}.*)")},
      response);
  CallServiceUpdate(app_id, "",
                    UpdateService::PolicySameVersionUpdate::kNotAllowed);
  Uninstall();
}

TEST_F(IntegrationTest, InstallDataIndex) {
  ScopedServer test_server(test_commands_);
  Install();
  ExpectInstalled();

  const std::string app_id = "test-appid";
  const std::string install_data_index = "test-install-data-index";

  InstallApp(app_id);

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

  CallServiceUpdate(app_id, install_data_index,
                    UpdateService::PolicySameVersionUpdate::kAllowed);

  Uninstall();
}

TEST_F(IntegrationTest, MigrateLegacyUpdater) {
  SetupFakeLegacyUpdaterData();
  Install();
  ExpectInstalled();
  ExpectLegacyUpdaterDataMigrated();
  Uninstall();
}

TEST_F(IntegrationTest, RecoveryNoUpdater) {
  const std::string appid = "test1";
  const base::Version version("0.1");
  RunRecoveryComponent(appid, version);
  WaitForUpdaterExit();
  ExpectInstalled();
  ExpectActiveUpdater();
  ExpectAppVersion(appid, version);
  Uninstall();
}

TEST_F(IntegrationTest, OfflineInstall) {
  Install();
  ExpectInstalled();
  RunOfflineInstall();
  Uninstall();
}

#endif  // BUILDFLAG(IS_WIN) || !defined(COMPONENT_BUILD)

}  // namespace test
}  // namespace updater
