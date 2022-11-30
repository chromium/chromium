// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/installer/setup/setup_singleton.h"

#include <windows.h>

#include <functional>
#include <string>
#include <vector>

#include "base/command_line.h"
#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/files/scoped_temp_dir.h"
#include "base/strings/string_number_conversions.h"
#include "base/test/multiprocess_test.h"
#include "base/test/test_timeouts.h"
#include "base/threading/platform_thread.h"
#include "base/time/time.h"
#include "chrome/installer/setup/installer_state.h"
#include "chrome/installer/util/initial_preferences.h"
#include "chrome/installer/util/installation_state.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/multiprocess_func_list.h"

namespace installer {

namespace {

constexpr char kInstallDirSwitch[] = "install-dir";
constexpr base::FilePath::CharType kSentinelFileName[] =
    FILE_PATH_LITERAL("sentinel.txt");
constexpr wchar_t kTestProcessReadyEventName[] =
    L"Local\\ChromeSetupSingletonTestProcessReady";

enum ErrorCode {
  SUCCESS,
  SETUP_SINGLETON_ACQUISITION_FAILED,
  SENTINEL_FILE_CREATE_ERROR,
  WAIT_RETURNED_FALSE,
};

base::CommandLine GetDummyCommandLine() {
  return base::CommandLine(base::FilePath(FILE_PATH_LITERAL("dummy.exe")));
}

std::wstring HashFilePath(const base::FilePath& path) {
  return base::NumberToWString(
      std::hash<base::FilePath::StringType>()(path.value()));
}

ErrorCode CreateAndDeleteSentinelFile(const base::FilePath& install_dir) {
  const base::FilePath sentinel_file_path =
      install_dir.Append(kSentinelFileName);

  base::File file(sentinel_file_path, base::File::FLAG_CREATE |
                                          base::File::FLAG_WRITE |
                                          base::File::FLAG_DELETE_ON_CLOSE);
  if (!file.IsValid())
    return SENTINEL_FILE_CREATE_ERROR;

  base::PlatformThread::Sleep(TestTimeouts::tiny_timeout());
  return SUCCESS;
}

MULTIPROCESS_TEST_MAIN(SetupSingletonTestExclusiveAccessProcessMain) {
  base::CommandLine* const command_line =
      base::CommandLine::ForCurrentProcess();
  const base::FilePath install_dir =
      command_line->GetSwitchValuePath(kInstallDirSwitch);

  InstallationState original_state;
  InstallerState installer_state;
  installer_state.set_target_path_for_testing(install_dir);

  // Acquire the exclusive right to modify the Chrome installation.
  std::unique_ptr<SetupSingleton> setup_singleton(SetupSingleton::Acquire(
      GetDummyCommandLine(), InitialPreferences::ForCurrentProcess(),
      &original_state, &installer_state));
  if (!setup_singleton)
    return SETUP_SINGLETON_ACQUISITION_FAILED;

  // Create a sentinel file and delete it after a few milliseconds. This will
  // fail if the sentinel file already exists (which shouldn't be the case since
  // we are in the scope of a SetupSingleton).
  return CreateAndDeleteSentinelFile(install_dir);
}

MULTIPROCESS_TEST_MAIN(SetupSingletonTestWaitForInterruptProcessMain) {
  base::CommandLine* const command_line =
      base::CommandLine::ForCurrentProcess();
  const base::FilePath install_dir =
      command_line->GetSwitchValuePath(kInstallDirSwitch);

  InstallationState original_state;
  InstallerState installer_state;
  installer_state.set_target_path_for_testing(install_dir);

  // Acquire the exclusive right to modify the Chrome installation.
  std::unique_ptr<SetupSingleton> setup_singleton(SetupSingleton::Acquire(
      GetDummyCommandLine(), InitialPreferences::ForCurrentProcess(),
      &original_state, &installer_state));
  if (!setup_singleton)
    return SETUP_SINGLETON_ACQUISITION_FAILED;

  // Signal an event to indicate that this process has acquired the
  // SetupSingleton.
  base::WaitableEvent ready_event(base::win::ScopedHandle(::CreateEvent(
      nullptr, FALSE, FALSE,
      (kTestProcessReadyEventName + HashFilePath(install_dir)).c_str())));
  ready_event.Signal();

  // Wait indefinitely. This should only return when another SetupSingleton is
  // instantiated for |install_dir|.
  if (!setup_singleton->WaitForInterrupt(base::TimeDelta::Max()))
    return WAIT_RETURNED_FALSE;

  // Create a sentinel file and delete it after a few milliseconds. This will
  // fail if the sentinel file already exists (which shouldn't be the case since
  // we are in the scope of a SetupSingleton).
  return CreateAndDeleteSentinelFile(install_dir);
}

class SetupSingletonTest : public base::MultiProcessTest {
 public:
  SetupSingletonTest() = default;

  SetupSingletonTest(const SetupSingletonTest&) = delete;
  SetupSingletonTest& operator=(const SetupSingletonTest&) = delete;

  void SetUp() override { ASSERT_TRUE(install_dir_.CreateUniqueTempDir()); }

  base::CommandLine MakeCmdLine(const std::string& procname) override {
    base::CommandLine command_line =
        base::MultiProcessTest::MakeCmdLine(procname);
    command_line.AppendSwitchPath(kInstallDirSwitch, install_dir_path());
    return command_line;
  }

  base::Process SpawnChildProcess(const std::string& process_name) {
    base::LaunchOptions options;
    options.start_hidden = true;
    return SpawnChildWithOptions(process_name, options);
  }

  const base::FilePath& install_dir_path() const {
    return install_dir_.GetPath();
  }

 private:
  base::ScopedTempDir install_dir_;
};

}  // namespace

// Verify that a single SetupSingleton can be active at a time for a given
// Chrome installation.
TEST_F(SetupSingletonTest, ExclusiveAccess) {
  constexpr int kNumProcesses = 10;

  std::vector<base::Process> processes;
  for (int i = 0; i < kNumProcesses; ++i) {
    processes.push_back(
        SpawnChildProcess("SetupSingletonTestExclusiveAccessProcessMain"));
  }

  for (base::Process& process : processes) {
    int exit_code = 0;
    EXPECT_TRUE(process.WaitForExit(&exit_code));
    EXPECT_EQ(SUCCESS, exit_code);
  }
}

// Verify that WaitForInterrupt() returns false when its delay expires before
TEST_F(SetupSingletonTest, WaitForInterruptNoInterrupt) {
  InstallationState original_state;
  InstallerState installer_state;
  installer_state.set_target_path_for_testing(install_dir_path());
  std::unique_ptr<SetupSingleton> setup_singleton(SetupSingleton::Acquire(
      GetDummyCommandLine(), InitialPreferences::ForCurrentProcess(),
      &original_state, &installer_state));
  ASSERT_TRUE(setup_singleton);

  EXPECT_FALSE(setup_singleton->WaitForInterrupt(TestTimeouts::tiny_timeout()));
}

// Verify that WaitForInterrupt() returns true immediately when another process
// tries to acquire a SetupSingleton.
TEST_F(SetupSingletonTest, WaitForInterruptWithInterrupt) {
  base::Process wait_process =
      SpawnChildProcess("SetupSingletonTestWaitForInterruptProcessMain");

  // Wait until the other process acquires the SetupSingleton.
  base::WaitableEvent ready_event(base::win::ScopedHandle(::CreateEvent(
      nullptr, FALSE, FALSE,
      (kTestProcessReadyEventName + HashFilePath(install_dir_path()))
          .c_str())));
  ready_event.Wait();

  // Acquire the SetupSingleton.
  InstallationState original_state;
  InstallerState installer_state;
  installer_state.set_target_path_for_testing(install_dir_path());
  std::unique_ptr<SetupSingleton> setup_singleton(SetupSingleton::Acquire(
      GetDummyCommandLine(), InitialPreferences::ForCurrentProcess(),
      &original_state, &installer_state));
  ASSERT_TRUE(setup_singleton);

  // Create a sentinel file and delete it after a few milliseconds. This will
  // fail if the sentinel file already exists (which shouldn't be the case since
  // we are in the scope of a SetupSingleton).
  EXPECT_EQ(SUCCESS, CreateAndDeleteSentinelFile(install_dir_path()));

  // Join |wait_process|.
  int exit_code = 0;
  EXPECT_TRUE(wait_process.WaitForExit(&exit_code));
  EXPECT_EQ(SUCCESS, exit_code);
}

}  // namespace installer
