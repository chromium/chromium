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
#include "base/test/task_environment.h"
#include "base/version.h"
#include "build/build_config.h"
#include "chrome/updater/constants.h"
#include "chrome/updater/persisted_data.h"
#include "chrome/updater/prefs.h"
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
    ExpectClean();
    if (::testing::Test::HasFailure())
      PrintLog();
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
#endif  // OS_MAC

#endif  // defined(OS_WIN) || !defined(COMPONENT_BUILD)

}  // namespace test

}  // namespace updater
