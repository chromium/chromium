// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

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
#include "chrome/updater/test/integration_test_commands.h"
#include "chrome/updater/test/integration_tests_impl.h"
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

// TODO(crbug.com/1096654): Enable for system integration tests for Win.

class IntegrationTest : public ::testing::Test {
 public:
  IntegrationTest() : test_commands_(CreateIntegrationTestCommands()) {}
  ~IntegrationTest() override = default;

 protected:
  void SetUp() override {
    Clean();
    ExpectClean();
    EnterTestMode(GURL("http://localhost:1234"));
  }

  void TearDown() override {
    ExpectClean();
    if (::testing::Test::HasFailure())
      PrintLog();
    // TODO(crbug.com/1159189): Use a specific test output directory
    // because Uninstall() deletes the files under GetDataDirPath().
    CopyLog();
    Clean();
  }

  void CopyLog() { test_commands_->CopyLog(); }

  void PrintLog() { test_commands_->PrintLog(); }

  void Install() { test_commands_->Install(); }

  void ExpectInstalled() { test_commands_->ExpectInstalled(); }

  void Uninstall() {
    if (::testing::Test::HasFailure())
      PrintLog();
    CopyLog();
    test_commands_->Uninstall();
  }

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

  void SetExistenceCheckerPath(const std::string& app_id,
                               const base::FilePath& path) {
    test_commands_->SetExistenceCheckerPath(app_id, path);
  }

  void ExpectAppUnregisteredExistenceCheckerPath(const std::string& app_id) {
    test_commands_->ExpectAppUnregisteredExistenceCheckerPath(app_id);
  }

  void RegisterApp(const std::string& app_id) {
    test_commands_->RegisterApp(app_id);
  }

  void RegisterTestApp() { test_commands_->RegisterTestApp(); }

  void RunWake(int exit_code) { test_commands_->RunWake(exit_code); }

  base::FilePath GetDifferentUserPath() {
    return test_commands_->GetDifferentUserPath();
  }

  void WaitForServerExit() { test_commands_->WaitForServerExit(); }

  scoped_refptr<IntegrationTestCommands> test_commands_;

 private:
  base::test::TaskEnvironment environment_;
};

// The project's position is that component builds are not portable outside of
// the build directory. Therefore, installation of component builds is not
// expected to work and these tests do not run on component builders.
// See crbug.com/1112527.
#if defined(OS_WIN) || !defined(COMPONENT_BUILD)

TEST_F(IntegrationTest, InstallUninstall) {
  Install();
  ExpectInstalled();
  ExpectVersionActive(kUpdaterVersion);
  ExpectActiveUpdater();
#if defined(OS_WIN)
  // Tests the COM registration after the install. For now, tests that the
  // COM interfaces are registered, which is indirectly testing the type
  // library separation for the public, private, and legacy interfaces.
  ExpectInterfacesRegistered();
#endif  // OS_WIN
  Uninstall();
}

TEST_F(IntegrationTest, SelfUninstallOutdatedUpdater) {
  Install();
  ExpectInstalled();
  SetupFakeUpdaterHigherVersion();
  ExpectVersionNotActive(kUpdaterVersion);
  SleepFor(2);

  RunWake(0);

  // The mac server will remain active for 10 seconds after it replies to the
  // wake client, then shut down and uninstall itself. Sleep to wait for this
  // to happen.
  SleepFor(11);

  ExpectCandidateUninstalled();
  // The candidate uninstall should not have altered global prefs.
  ExpectVersionNotActive(kUpdaterVersion);
  ExpectVersionNotActive("0.0.0.0");

  Uninstall();
  Clean();
}

TEST_F(IntegrationTest, RegisterTestApp) {
  RegisterTestApp();
  ExpectInstalled();
  ExpectVersionActive(kUpdaterVersion);
  ExpectActiveUpdater();
  Uninstall();
}

TEST_F(IntegrationTest, ReportsActive) {
  // A longer than usual timeout is needed for this test because the macOS
  // UpdateServiceInternal server takes at least 10 seconds to shut down after
  // Install, and RegisterApp cannot make progress until it shut downs and
  // releases the global prefs lock. We give it at most 18 seconds to be safe.
  base::test::ScopedRunLoopTimeout timeout(FROM_HERE,
                                           base::TimeDelta::FromSeconds(18));

  ScopedServer test_server(test_commands_);
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

TEST_F(IntegrationTest, UnregisterUninstalledApp) {
  RegisterTestApp();
  ExpectInstalled();
  ExpectVersionActive(kUpdaterVersion);
  ExpectActiveUpdater();

  RegisterApp("test1");
  RegisterApp("test2");

  SetExistenceCheckerPath(kTestAppId,
                          base::FilePath(FILE_PATH_LITERAL("NONE")));

  RunWake(0);

  SleepFor(13);
  ExpectInstalled();

  ExpectAppUnregisteredExistenceCheckerPath(kTestAppId);

  Uninstall();
}

TEST_F(IntegrationTest, UninstallUpdaterWhenAllAppsUninstalled) {
  RegisterTestApp();
  ExpectInstalled();
  ExpectVersionActive(kUpdaterVersion);
  ExpectActiveUpdater();

  SetExistenceCheckerPath(kTestAppId,
                          base::FilePath(FILE_PATH_LITERAL("NONE")));

  RunWake(0);

  SleepFor(13);
}

// Windows does not currently have a concept of app ownership, so this
// test need not run on Windows.
#if defined(OS_MAC)
TEST_F(IntegrationTest, UnregisterUnownedApp) {
  Install();
  ExpectInstalled();
  ExpectVersionActive(kUpdaterVersion);
  ExpectActiveUpdater();

  RegisterApp("test1");
  RegisterApp("test2");

  SetExistenceCheckerPath("test1", GetDifferentUserPath());

  RunWake(0);
  WaitForServerExit();

  ExpectAppUnregisteredExistenceCheckerPath("test1");

  Uninstall();
}
#endif  // defined(OS_MAC)

#endif  // defined(OS_WIN) || !defined(COMPONENT_BUILD)

}  // namespace test
}  // namespace updater
