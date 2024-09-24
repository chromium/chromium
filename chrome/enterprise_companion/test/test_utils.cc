// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/enterprise_companion/test/test_utils.h"

#include <optional>

#include "base/command_line.h"
#include "base/containers/flat_set.h"
#include "base/files/file_enumerator.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/functional/function_ref.h"
#include "base/logging.h"
#include "base/memory/scoped_refptr.h"
#include "base/path_service.h"
#include "base/process/launch.h"
#include "base/process/process.h"
#include "base/run_loop.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/test/bind.h"
#include "base/test/multiprocess_test.h"
#include "base/test/test_timeouts.h"
#include "base/threading/platform_thread.h"
#include "base/threading/thread_restrictions.h"
#include "base/time/time.h"
#include "chrome/enterprise_companion/device_management_storage/dm_storage.h"
#include "chrome/enterprise_companion/enterprise_companion.h"
#include "chrome/enterprise_companion/enterprise_companion_client.h"
#include "chrome/enterprise_companion/installer_paths.h"

namespace enterprise_companion {

namespace {

#if BUILDFLAG(IS_POSIX)
constexpr char kTestExe[] = "enterprise_companion_test";
#elif BUILDFLAG(IS_WIN)
constexpr char kTestExe[] = "enterprise_companion_test.exe";
#endif

void RunAppUnderTest(const std::string& switch_string) {
  const base::FilePath test_exe_path =
      base::PathService::CheckedGet(base::DIR_EXE).AppendASCII(kTestExe);
  ASSERT_TRUE(base::PathExists(test_exe_path));

  base::CommandLine command_line(test_exe_path);
  command_line.AppendSwitch(switch_string);
  base::Process installer_process = base::LaunchProcess(command_line, {});
  ASSERT_EQ(WaitForProcess(installer_process), 0);
}

}  // namespace

int WaitForProcess(base::Process& process) {
  int exit_code = 0;
  bool process_exited = false;
  base::RunLoop wait_for_process_exit_loop;
  base::ThreadPool::CreateSequencedTaskRunner({base::MayBlock()})
      ->PostTaskAndReply(
          FROM_HERE, base::BindLambdaForTesting([&] {
            base::ScopedAllowBaseSyncPrimitivesForTesting allow_blocking;
            process_exited = process.WaitForExitWithTimeout(
                TestTimeouts::action_timeout(), &exit_code);
          }),
          wait_for_process_exit_loop.QuitClosure());
  wait_for_process_exit_loop.Run();
  process.Close();
  EXPECT_TRUE(process_exited);
  return exit_code;
}

bool WaitFor(base::FunctionRef<bool()> predicate,
             base::FunctionRef<void()> still_waiting) {
  constexpr base::TimeDelta kOutputInterval = base::Seconds(10);
  auto notify_next = base::TimeTicks::Now() + kOutputInterval;
  const auto deadline = base::TimeTicks::Now() + TestTimeouts::action_timeout();
  while (base::TimeTicks::Now() < deadline) {
    if (predicate()) {
      return true;
    }
    if (notify_next < base::TimeTicks::Now()) {
      still_waiting();
      notify_next += kOutputInterval;
    }
    base::PlatformThread::Sleep(TestTimeouts::tiny_timeout());
  }
  return false;
}

void TestMethods::Clean() {
  std::optional<base::FilePath> path = GetInstallDirectory();
  ASSERT_TRUE(path);
  // Deleting the install directory may transiently fail. This has been observed
  // on Windows in particular with access denied and "file is open in another
  // program" errors.
  ASSERT_TRUE(
      WaitFor([&] { return base::DeletePathRecursively(*path); },
              [&] { VLOG(1) << "Waiting to delete " << *path << "..."; }));

  scoped_refptr<device_management_storage::DMStorage> dm_storage =
      device_management_storage::GetDefaultDMStorage();
  ASSERT_TRUE(dm_storage);
  EXPECT_TRUE(dm_storage->DeleteDMToken());
  EXPECT_TRUE(dm_storage->DeleteEnrollmentToken());
  EXPECT_TRUE(dm_storage->RemoveAllPolicies());
}

void TestMethods::ExpectClean() {
  std::optional<base::FilePath> path = GetInstallDirectory();
  if (base::PathExists(*path)) {
    // On Windows the uninstaller delegates the deletion of the installation
    // directory to a script running in a process that outlives the uninstaller.
    // Thus, tests should wait for the install to be clean.
    EXPECT_TRUE(WaitFor(
        [&] {
          // The uninstaller cannot reliably completely remove the installer
          // directory itself, because it writes the log file while it is
          // operating. If the installation path exists, it must be a directory
          // with only this file present to be considered "clean".
          base::flat_set<base::FilePath> remaining_files;
          base::FileEnumerator(*path, false, base::FileEnumerator::NAMES_ONLY)
              .ForEach([&](const base::FilePath& path) {
                remaining_files.insert(path.BaseName());
              });
          return remaining_files.size() == 0 ||
                 (remaining_files.size() == 1 &&
                  remaining_files.contains(base::FilePath(
                      FILE_PATH_LITERAL("enterprise_companion.log"))));
        },
        [&] { VLOG(1) << "Waiting for " << *path << " to become clean."; }));
  }

  scoped_refptr<device_management_storage::DMStorage> dm_storage =
      device_management_storage::GetDefaultDMStorage();
  ASSERT_TRUE(dm_storage);
  EXPECT_EQ(dm_storage->GetDmToken(), "");
  EXPECT_EQ(dm_storage->GetEnrollmentToken(), "");
}

void TestMethods::ExpectInstalled() {
  std::optional<base::FilePath> install_dir = GetInstallDirectory();
  ASSERT_TRUE(install_dir);
  ASSERT_TRUE(base::PathExists(install_dir->AppendASCII(kExecutableName)));
}

void TestMethods::Install() {
  RunAppUnderTest(kInstallSwitch);
}

void TestMethods::InstallIfNeeded() {
  RunAppUnderTest(kInstallIfNeededSwitch);
}

}  // namespace enterprise_companion
