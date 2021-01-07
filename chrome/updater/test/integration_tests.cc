// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/updater/test/integration_tests.h"

#include <cstdlib>

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
#include "chrome/updater/updater_version.h"
#include "chrome/updater/util.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace updater {

namespace test {

// The project's position is that component builds are not portable outside of
// the build directory. Therefore, installation of component builds is not
// expected to work and these tests do not run on component builders.
// See crbug.com/1112527.
#if defined(OS_WIN) || !defined(COMPONENT_BUILD)

namespace {

void ExpectActiveVersion(std::string expected) {
  EXPECT_EQ(CreateGlobalPrefs()->GetActiveVersion(), expected);
}

#if defined(OS_MAC)
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
#endif  // defined(OS_MAC)

}  // namespace

void PrintLog() {
  std::string contents;
  VLOG(0) << GetDataDirPath().AppendASCII("updater.log");
  if (base::ReadFileToString(GetDataDirPath().AppendASCII("updater.log"),
                             &contents)) {
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
    base::FilePath dest_file_path = dest_dir.AppendASCII(
        base::StrCat({GetTestInfo()->test_suite_name(), ".",
                      GetTestInfo()->name(), "_updater.log"}));
    EXPECT_TRUE(
        base::CopyFile(src_dir.AppendASCII("updater.log"), dest_file_path));
  }
}

void RunWake(int expected_exit_code) {
  const base::FilePath installed_executable_path = GetInstalledExecutablePath();
  EXPECT_TRUE(base::PathExists(installed_executable_path));
  base::CommandLine command_line(installed_executable_path);
  command_line.AppendSwitch(kWakeSwitch);
  command_line.AppendSwitch(kEnableLoggingSwitch);
  command_line.AppendSwitchASCII(kLoggingModuleSwitch, "*/updater/*=2");
  int exit_code = -1;
  ASSERT_TRUE(Run(command_line, &exit_code));
  EXPECT_EQ(exit_code, expected_exit_code);
}

void SetupFakeUpdaterPrefs(const base::Version& version) {
  std::unique_ptr<GlobalPrefs> global_prefs = CreateGlobalPrefs();
  global_prefs->SetActiveVersion(version.GetString());
  global_prefs->SetSwapping(false);
  PrefsCommitPendingWrites(global_prefs->GetPrefService());

  ASSERT_EQ(version.GetString(), global_prefs->GetActiveVersion());
}

void SetupFakeUpdaterInstallFolder(const base::Version& version) {
  const base::FilePath folder_path = GetFakeUpdaterInstallFolderPath(version);
  ASSERT_TRUE(base::CreateDirectory(folder_path));
}

void SetupFakeUpdater(const base::Version& version) {
  SetupFakeUpdaterPrefs(version);
  SetupFakeUpdaterInstallFolder(version);
}

void SetupFakeUpdaterVersion(int offset) {
  ASSERT_NE(offset, 0);
  std::vector<uint32_t> components =
      base::Version(UPDATER_VERSION_STRING).components();
  base::CheckedNumeric<uint32_t> new_version = components[0];
  new_version += offset;
  ASSERT_TRUE(new_version.AssignIfValid(&components[0]));
  SetupFakeUpdater(base::Version(std::move(components)));
}

void SetupFakeUpdaterLowerVersion() {
  SetupFakeUpdaterVersion(-1);
}

void SetupFakeUpdaterHigherVersion() {
  SetupFakeUpdaterVersion(1);
}

bool Run(base::CommandLine command_line, int* exit_code) {
  command_line.AppendSwitch("enable-logging");
  command_line.AppendSwitchASCII("vmodule", "*/updater/*=2");
  base::Process process = base::LaunchProcess(command_line, {});
  if (!process.IsValid())
    return false;
  return process.WaitForExitWithTimeout(TestTimeouts::action_max_timeout(),
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

class IntegrationTest : public ::testing::Test {
 protected:
  void SetUp() override {
    Clean();
    ExpectClean();
    EnterTestMode(GURL("http://localhost:1234"));
  }

  void TearDown() override {
    if (::testing::Test::HasFailure())
      PrintLog();
    // TODO(crbug.com/1159189): Use a specific test output directory
    // because Uninstall() deletes the files under GetDataDirPath().
    CopyLog(GetDataDirPath());
    ExpectClean();
    Clean();
  }

 private:
  base::test::TaskEnvironment environment_;
};

TEST_F(IntegrationTest, InstallUninstall) {
  Install();
  ExpectInstalled();
  ExpectActiveVersion(UPDATER_VERSION_STRING);
  ExpectActive();
  Uninstall();
}

TEST_F(IntegrationTest, SelfUninstallOutdatedUpdater) {
  Install();
  ExpectInstalled();
  SetupFakeUpdaterHigherVersion();
  EXPECT_NE(CreateGlobalPrefs()->GetActiveVersion(), UPDATER_VERSION_STRING);

  RunWake(0);

  // The mac server will remain active for 10 seconds after it replies to the
  // wake client, then shut down and uninstall itself. Sleep to wait for this
  // to happen.
  SleepFor(11);

  ExpectCandidateUninstalled();
  // The candidate uninstall should not have altered global prefs.
  EXPECT_NE(CreateGlobalPrefs()->GetActiveVersion(), UPDATER_VERSION_STRING);
  EXPECT_NE(CreateGlobalPrefs()->GetActiveVersion(), "0.0.0.0");

  Uninstall();
  Clean();
}

#if defined(OS_MAC)
// TODO(crbug.com/1163524): Enable on Windows.
TEST_F(IntegrationTest, RegisterTestApp) {
  RegisterTestApp();
  ExpectInstalled();
  ExpectActiveVersion(UPDATER_VERSION_STRING);
  ExpectActive();
  Uninstall();
}

// TODO(crbug.com/1163524): Enable on Windows.
// TODO(crbug.com/1163625): Failing on Mac 10.11.
TEST_F(IntegrationTest, ReportsActive) {
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

TEST_F(IntegrationTest, UnregisterUninstalledApp) {
  RegisterTestApp();
  ExpectInstalled();
  ExpectActiveVersion(UPDATER_VERSION_STRING);
  ExpectActive();

  {
    std::unique_ptr<GlobalPrefs> global_prefs = CreateGlobalPrefs();
    auto persisted_data =
        base::MakeRefCounted<PersistedData>(global_prefs->GetPrefService());
    base::FilePath fake_ecp =
        persisted_data->GetExistenceCheckerPath(kTestAppId)
            .Append(FILE_PATH_LITERAL("NOT_THERE"));
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
  Clean();
}

TEST_F(IntegrationTest, UnregisterUnownedApp) {
  RegisterTestApp();
  ExpectInstalled();
  ExpectActiveVersion(UPDATER_VERSION_STRING);
  ExpectActive();

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
  Clean();
}

#endif  // OS_MAC

#if defined(OS_WIN)
// Tests the COM registration after the install. For now, tests that the
// COM interfaces are registered, which is indirectly testing the type
// library separation for the public, private, and legacy interfaces.
TEST_F(IntegrationTest, COMRegistration) {
  Install();
  ExpectInterfacesRegistered();
  Uninstall();
}
#endif  // OS_WIN

#endif  // defined(OS_WIN) || !defined(COMPONENT_BUILD)

}  // namespace test

}  // namespace updater
