// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/enterprise_companion/crash_client.h"

#include "base/base_paths.h"
#include "base/command_line.h"
#include "base/files/file_enumerator.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/memory/scoped_refptr.h"
#include "base/path_service.h"
#include "base/process/launch.h"
#include "base/process/process.h"
#include "base/run_loop.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "base/test/test_timeouts.h"
#include "base/threading/thread_restrictions.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace enterprise_companion {

namespace {
#if BUILDFLAG(IS_WIN)
constexpr char kCrashExe[] = "test_crashpad_embedder.exe";
#else
constexpr char kCrashExe[] = "test_crashpad_embedder";
#endif

constexpr char kCrashDatabaseSwitch[] = "crash-database-path";
}  // namespace

class CrashClientTest : public ::testing::Test {
 public:
  void SetUp() override { ASSERT_TRUE(database_.CreateUniqueTempDir()); }

 protected:
  base::test::TaskEnvironment environment_{
      base::test::TaskEnvironment::MainThreadType::IO};
  // Helper thread to wait for process exit without blocking the main thread.
  scoped_refptr<base::SequencedTaskRunner> wait_for_process_exit_runner_ =
      base::ThreadPool::CreateSequencedTaskRunner({base::MayBlock()});
  base::ScopedTempDir database_;

  // Runs the test crashpad embedded binary which should crash and produce a
  // dump in the provided database.
  void RunChildProcess() {
    base::FilePath test_exe =
        base::PathService::CheckedGet(base::DIR_EXE).AppendASCII(kCrashExe);
    EXPECT_TRUE(base::PathExists(test_exe));
    base::CommandLine command_line = base::CommandLine(test_exe);
    command_line.AppendSwitchPath(kCrashDatabaseSwitch, database_.GetPath());
    base::Process process =
        base::LaunchProcess(command_line, base::LaunchOptionsForTest());

    base::RunLoop wait_for_process_exit_loop;
    wait_for_process_exit_runner_->PostTaskAndReply(
        FROM_HERE, base::BindLambdaForTesting([&] {
          base::ScopedAllowBaseSyncPrimitivesForTesting allow_blocking;
          EXPECT_TRUE(process.WaitForExitWithTimeout(
              TestTimeouts::action_timeout(), nullptr));
        }),
        wait_for_process_exit_loop.QuitClosure());
    wait_for_process_exit_loop.Run();
    process.Close();
  }

  int CountCrashDumps() {
    int count = 0;
    base::FileEnumerator(database_.GetPath(), true, base::FileEnumerator::FILES,
                         FILE_PATH_LITERAL("*.dmp"),
                         base::FileEnumerator::FolderSearchPolicy::ALL)
        .ForEach([&count](const base::FilePath& name) { ++count; });
    return count;
  }
};

TEST_F(CrashClientTest, CrashCreatesReport) {
  RunChildProcess();

  EXPECT_EQ(CountCrashDumps(), 1);
}

}  // namespace enterprise_companion
