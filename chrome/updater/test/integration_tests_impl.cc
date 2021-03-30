// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/updater/test/integration_tests_impl.h"

#include <cstdlib>
#include <memory>
#include <string>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/memory/scoped_refptr.h"
#include "base/numerics/checked_math.h"
#include "base/optional.h"
#include "base/process/launch.h"
#include "base/process/process.h"
#include "base/run_loop.h"
#include "base/strings/strcat.h"
#include "base/task/post_task.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/test/bind.h"
#include "base/time/time.h"
#include "base/version.h"
#include "chrome/updater/constants.h"
#include "chrome/updater/persisted_data.h"
#include "chrome/updater/prefs.h"
#include "chrome/updater/registration_data.h"
#include "chrome/updater/update_service.h"
#include "chrome/updater/updater_scope.h"
#include "chrome/updater/updater_version.h"
#include "chrome/updater/util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace updater {
namespace test {

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
    VLOG(0) << "End contents of updater.log.";
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
  ASSERT_TRUE(global_prefs) << "No global prefs.";
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

}  // namespace test
}  // namespace updater
