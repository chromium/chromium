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
#include "base/path_service.h"
#include "base/process/launch.h"
#include "base/process/process.h"
#include "base/strings/strcat.h"
#include "base/strings/stringprintf.h"
#include "base/test/bind.h"
#include "base/test/scoped_run_loop_timeout.h"
#include "base/test/task_environment.h"
#include "base/test/test_timeouts.h"
#include "base/time/time.h"
#include "base/version.h"
#include "build/build_config.h"
#include "chrome/common/chrome_paths.h"
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

#if defined(OS_WIN)
#include "base/strings/utf_string_conversions.h"
#endif  // OS_WIN

namespace updater {
namespace test {
namespace {

#if defined(OS_WIN) || !defined(COMPONENT_BUILD)

#if defined(OS_MAC)
constexpr char kDoNothingCRXName[] = "updater_qualification_app_dmg.crx";
constexpr char kDoNothingCRXRun[] = "updater_qualification_app_dmg.dmg";
constexpr char kDoNothingCRXHash[] =
    "c9eeadf63732f3259e2ad1cead6298f90a3ef4b601b1ba1cbb0f37b6112a632c";
#elif defined(OS_WIN)
constexpr char kDoNothingCRXName[] = "updater_qualification_app_exe.crx";
constexpr char kDoNothingCRXRun[] = "qualification_app.exe";
constexpr char kDoNothingCRXHash[] =
    "0705f7eedb0427810db76dfc072c8cbc302fbeb9b2c56fa0de3752ed8d6f9164";
#else
static_assert(false, "Unsupported platform for IntegrationTest.*");
#endif

std::string GetUpdateResponse(const std::string& app_id,
                              const std::string& codebase,
                              const base::Version& version) {
  return base::StringPrintf(
      ")]}'\n"
      R"({"response":{)"
      R"(  "protocol":"3.1",)"
      R"(  "app":[)"
      R"(    {)"
      R"(      "appid":"%s",)"
      R"(      "status":"ok",)"
      R"(      "updatecheck":{)"
      R"(        "status":"ok",)"
      R"(        "urls":{"url":[{"codebase":"%s"}]},)"
      R"(        "manifest":{)"
      R"(          "version":"%s",)"
      R"(          "run":"%s",)"
      R"(          "packages":{)"
      R"(            "package":[)"
      R"(              {"name":"%s","hash_sha256":"%s"})"
      R"(            ])"
      R"(          })"
      R"(        })"
      R"(      })"
      R"(    })"
      R"(  ])"
      R"(}})",
      app_id.c_str(), codebase.c_str(), version.GetString().c_str(),
      kDoNothingCRXRun, kDoNothingCRXName, kDoNothingCRXHash);
}

void ExpectUpdateSequence(ScopedServer* test_server,
                          const std::string& app_id,
                          const base::Version& from_version,
                          const base::Version& to_version) {
  // First request: update check.
  test_server->ExpectOnce(
      base::StringPrintf(R"(.*"appid":"%s".*)", app_id.c_str()),
      GetUpdateResponse(app_id, test_server->base_url().spec(), to_version));

  // Second request: update download.
  base::FilePath test_data_path;
  ASSERT_TRUE(base::PathService::Get(chrome::DIR_TEST_DATA, &test_data_path));
  base::FilePath crx_path = test_data_path.Append(FILE_PATH_LITERAL("updater"))
                                .AppendASCII(kDoNothingCRXName);
  ASSERT_TRUE(base::PathExists(crx_path));
  std::string crx_bytes;
  base::ReadFileToString(crx_path, &crx_bytes);
  test_server->ExpectOnce("", crx_bytes);

  // Third request: event ping.
  test_server->ExpectOnce(
      base::StringPrintf(R"(.*"eventresult":1,"eventtype":3,)"
                         R"("nextversion":"%s","previousversion":"%s".*)",
                         to_version.GetString().c_str(),
                         from_version.GetString().c_str()),
      ")]}'\n");
}

void ExpectNoUpdateSequence(ScopedServer* test_server,
                            const std::string& app_id) {
  test_server->ExpectOnce(
      base::StringPrintf(R"(.*"appid":"%s".*)", app_id.c_str()),
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

#endif  // defined(OS_WIN) || !defined(COMPONENT_BUILD)

}  // namespace

// TODO(crbug.com/1096654): Enable for system integration tests for Win.

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

#if defined(OS_WIN)
  void ExpectInterfacesRegistered() {
    test_commands_->ExpectInterfacesRegistered();
  }

  void ExpectLegacyUpdate3WebSucceeds(const std::string& app_id) {
    test_commands_->ExpectLegacyUpdate3WebSucceeds(app_id);
  }

#endif  // OS_WIN

  void SetupFakeUpdaterHigherVersion() {
    test_commands_->SetupFakeUpdaterHigherVersion();
  }

  void SetupFakeUpdaterLowerVersion() {
    test_commands_->SetupFakeUpdaterLowerVersion();
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

  void ExpectAppUnregisteredExistenceCheckerPath(const std::string& app_id) {
    test_commands_->ExpectAppUnregisteredExistenceCheckerPath(app_id);
  }

  void ExpectAppVersion(const std::string& app_id,
                        const base::Version& version) {
    test_commands_->ExpectAppVersion(app_id, version);
  }

  void RegisterApp(const std::string& app_id) {
    test_commands_->RegisterApp(app_id);
  }

  void RunWake(int exit_code) { test_commands_->RunWake(exit_code); }

  void Update(const std::string& app_id) { test_commands_->Update(app_id); }

  void UpdateAll() { test_commands_->UpdateAll(); }

  base::FilePath GetDifferentUserPath() {
    return test_commands_->GetDifferentUserPath();
  }

  void WaitForServerExit() { test_commands_->WaitForServerExit(); }

  void SetUpTestService() {
#if defined(OS_WIN)
    test_commands_->SetUpTestService();
#endif  // OS_WIN
  }

  void TearDownTestService() {
#if defined(OS_WIN)
    test_commands_->TearDownTestService();
#endif  // OS_WIN
  }

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
  SleepFor(2);
  SetupFakeUpdaterHigherVersion();
  ExpectVersionNotActive(kUpdaterVersion);

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

TEST_F(IntegrationTest, QualifyUpdater) {
  ScopedServer test_server(test_commands_);
  Install();
  ExpectInstalled();
  WaitForServerExit();
  SetupFakeUpdaterLowerVersion();
  ExpectVersionNotActive(kUpdaterVersion);

  ExpectUpdateSequence(&test_server, kQualificationAppId, base::Version("0.1"),
                       base::Version("0.2"));

  RunWake(0);
  WaitForServerExit();

  // This instance is now qualified and should activate itself and check itself
  // for updates on the next check.
  test_server.ExpectOnce(base::StringPrintf(".*%s.*", kUpdaterAppId), ")]}'\n");
  RunWake(0);
  WaitForServerExit();
  ExpectVersionActive(kUpdaterVersion);

  Uninstall();
  Clean();
}

TEST_F(IntegrationTest, SelfUpdate) {
  ScopedServer test_server(test_commands_);
  Install();

  base::Version next_version(base::StringPrintf("%s1", kUpdaterVersion));
  ExpectUpdateSequence(&test_server, kUpdaterAppId,
                       base::Version(kUpdaterVersion), next_version);

  RunWake(0);
  WaitForServerExit();
  ExpectAppVersion(kUpdaterAppId, next_version);

  Uninstall();
  Clean();
}

TEST_F(IntegrationTest, ReportsActive) {
  // A longer than usual timeout is needed for this test because the macOS
  // UpdateServiceInternal server takes at least 10 seconds to shut down after
  // Install, and RegisterApp cannot make progress until it shut downs and
  // releases the global prefs lock. We give it at most 18 seconds to be safe.
  base::test::ScopedRunLoopTimeout timeout(FROM_HERE, base::Seconds(18));

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

TEST_F(IntegrationTest, UpdateApp) {
  ScopedServer test_server(test_commands_);
  Install();

  const std::string kAppId("test");
  RegisterApp(kAppId);
  base::Version v1("1");
  ExpectUpdateSequence(&test_server, kAppId, base::Version("0.1"), v1);
  RunWake(0);

  base::Version v2("2");
  ExpectUpdateSequence(&test_server, kAppId, v1, v2);
  Update(kAppId);
  WaitForServerExit();
  ExpectAppVersion(kAppId, v2);

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

#if defined(OS_WIN)
TEST_F(IntegrationTest, LegacyUpdate3Web) {
  ScopedServer test_server(test_commands_);
  Install();

  const char kAppId[] = "test1";
  RegisterApp(kAppId);

  ExpectNoUpdateSequence(&test_server, kAppId);
  ExpectLegacyUpdate3WebSucceeds(kAppId);

  ExpectUpdateSequence(&test_server, kAppId, base::Version("0.1"),
                       base::Version("0.2"));
  ExpectLegacyUpdate3WebSucceeds(kAppId);

  Uninstall();
}
#endif  // OS_WIN

TEST_F(IntegrationTest, UnregisterUninstalledApp) {
  Install();
  ExpectInstalled();
  RegisterApp("test1");
  RegisterApp("test2");

  WaitForServerExit();
  ExpectVersionActive(kUpdaterVersion);
  ExpectActiveUpdater();
  SetExistenceCheckerPath("test1", base::FilePath(FILE_PATH_LITERAL("NONE")));

  RunWake(0);

  WaitForServerExit();
  ExpectInstalled();
  ExpectAppUnregisteredExistenceCheckerPath("test1");

  Uninstall();
}

TEST_F(IntegrationTest, UninstallIfMaxServerWakesBeforeRegistrationExceeded) {
  Install();
  WaitForServerExit();
  ExpectInstalled();
  SetServerStarts(24);
  RunWake(0);
  WaitForServerExit();
  SleepFor(2);
  ExpectClean();
}

TEST_F(IntegrationTest, UninstallUpdaterWhenAllAppsUninstalled) {
  Install();
  RegisterApp("test1");
  ExpectInstalled();
  WaitForServerExit();
  SetServerStarts(24);
  RunWake(0);
  WaitForServerExit();
  ExpectInstalled();
  ExpectVersionActive(kUpdaterVersion);
  ExpectActiveUpdater();
  SetExistenceCheckerPath("test1", base::FilePath(FILE_PATH_LITERAL("NONE")));
  RunWake(0);
  WaitForServerExit();
  SleepFor(2);
  ExpectClean();
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
