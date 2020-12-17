// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/updater/test/integration_tests.h"

#include <cstdlib>

#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/process/launch.h"
#include "base/process/process.h"
#include "base/strings/strcat.h"
#include "base/test/task_environment.h"
#include "base/version.h"
#include "build/build_config.h"
#include "chrome/updater/constants.h"
#include "chrome/updater/persisted_data.h"
#include "chrome/updater/prefs.h"
#include "chrome/updater/registration_data.h"
#include "chrome/updater/test/test_app/constants.h"
#include "chrome/updater/test/test_app/test_app_version.h"
#include "chrome/updater/updater_version.h"
#include "chrome/updater/util.h"
#include "testing/gtest/include/gtest/gtest.h"

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
}  // namespace

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
  ASSERT_TRUE(offset != 0);
  base::Version self_version = base::Version(UPDATER_VERSION_STRING);
  std::vector<uint32_t> components = self_version.components();
  ASSERT_FALSE(offset < 0 && components[0] <= uint32_t{abs(offset)});
  components[0] += offset;
  SetupFakeUpdater(base::Version(components));
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
  return process.WaitForExitWithTimeout(base::TimeDelta::FromSeconds(60),
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
    EnterTestMode();
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
TEST_F(IntegrationTest, RegisterTestApp) {
  RegisterTestApp();
  ExpectInstalled();
  ExpectActiveVersion(UPDATER_VERSION_STRING);
  ExpectActive();
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
