// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/chrome_cleaner/ipc/sandbox.h"

#include <windows.h>

#include <algorithm>
#include <string>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/command_line.h"
#include "base/files/file_util.h"
#include "base/macros.h"
#include "base/numerics/safe_conversions.h"
#include "base/optional.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/test/multiprocess_test.h"
#include "base/win/scoped_handle.h"
#include "chrome/chrome_cleaner/buildflags.h"
#include "chrome/chrome_cleaner/ipc/mojo_task_runner.h"
#include "chrome/chrome_cleaner/logging/scoped_logging.h"
#include "chrome/chrome_cleaner/os/disk_util.h"
#include "chrome/chrome_cleaner/os/initializer.h"
#include "sandbox/win/src/sandbox_factory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/multiprocess_func_list.h"

namespace chrome_cleaner {

namespace {

constexpr int kChildExitCode = 420042;

class MockSandboxTargetServices : public sandbox::TargetServices {
 public:
  MockSandboxTargetServices() = default;
  ~MockSandboxTargetServices() = default;

  MOCK_METHOD0(Init, sandbox::ResultCode());
  MOCK_METHOD0(LowerToken, void());
  MOCK_METHOD0(GetState, sandbox::ProcessState*());
};

class TestSandboxSetupHooks : public SandboxSetupHooks {
 public:
  explicit TestSandboxSetupHooks(base::Process* process_holder)
      : process_holder_(process_holder) {}
  ~TestSandboxSetupHooks() override = default;

  ResultCode TargetSpawned(
      const base::Process& target_process,
      const base::win::ScopedHandle& target_thread) override {
    DCHECK(process_holder_);
    *process_holder_ = target_process.Duplicate();
    return SandboxSetupHooks::TargetSpawned(target_process, target_thread);
  }

 private:
  base::Process* process_holder_;

  DISALLOW_COPY_AND_ASSIGN(TestSandboxSetupHooks);
};

class TestSandboxTargetHooks : public SandboxTargetHooks {
 public:
  TestSandboxTargetHooks() = default;
  ~TestSandboxTargetHooks() override = default;

  ResultCode TargetDroppedPrivileges(
      const base::CommandLine& command_line) override {
    return RESULT_CODE_SUCCESS;
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(TestSandboxTargetHooks);
};

class SandboxTest : public base::MultiProcessTest {
 protected:
  SandboxTest() = default;

  void SetUp() override {
    // Delete the sandbox process log file. If we decide to log its content,
    // it will only contain output relevant to this test case. DeleteFile
    // returns true on success or if attempting to delete a file that does not
    // exist.
    sandbox_process_log_file_path_ =
        ScopedLogging::GetLogFilePath(kSandboxLogFileSuffix);
    EXPECT_TRUE(base::DeleteFile(sandbox_process_log_file_path_, false));
  }

  void TearDown() override {
    if (HasFailure() && base::PathExists(sandbox_process_log_file_path_)) {
      // Collect the sandbox process log file, and dump the contents, to help
      // debugging failures.
      std::string log_file_contents;
      if (base::ReadFileToString(sandbox_process_log_file_path_,
                                 &log_file_contents)) {
        std::vector<base::StringPiece> lines = base::SplitStringPiece(
            log_file_contents, "\n", base::TRIM_WHITESPACE,
            base::SPLIT_WANT_NONEMPTY);
        LOG(ERROR) << "Dumping sandbox process log";
        for (const auto& line : lines) {
          LOG(ERROR) << "Sandbox process log line: " << line;
        }
      } else {
        LOG(ERROR) << "Failed to read sandbox process log file";
      }
    }
  }

  // Starts a child process using the StartSandboxTarget API.
  bool SpawnMockSandboxProcess(base::Process* process) {
    TestSandboxSetupHooks setup_hooks(process);
    return chrome_cleaner::StartSandboxTarget(
               MakeCmdLine("MockSandboxProcessMain"), &setup_hooks,
               SandboxType::kTest) == RESULT_CODE_SUCCESS;
  }

  ResultCode TestRunSandboxTarget(const base::CommandLine& command_line) {
    TestSandboxTargetHooks hooks;
    return RunSandboxTarget(command_line, &mock_sandbox_target_services_,
                            &hooks);
  }

 private:
  MockSandboxTargetServices mock_sandbox_target_services_;
  base::FilePath sandbox_process_log_file_path_;
};

MULTIPROCESS_TEST_MAIN(MockSandboxProcessMain) {
  base::FilePath product_path;
  bool success = chrome_cleaner::GetAppDataProductDirectory(&product_path);
  CHECK(success);
  auto* target_services = sandbox::SandboxFactory::GetTargetServices();
  CHECK(target_services);
  NotifyInitializationDone();
  target_services->LowerToken();

  bool have_write_access = false;
  base::FilePath temp_file;
  if (base::CreateTemporaryFileInDir(product_path, &temp_file)) {
    have_write_access = true;
    base::DeleteFile(temp_file, /*recursive=*/false);
  }

#if BUILDFLAG(IS_OFFICIAL_CHROME_CLEANER_BUILD)
  CHECK(!have_write_access);
#else
  CHECK(have_write_access);
#endif

  // Lower token, test access.
  return kChildExitCode;
}

}  // namespace

TEST_F(SandboxTest, SpawnSandboxTarget) {
  base::Process target_process;
  EXPECT_TRUE(SpawnMockSandboxProcess(&target_process));
  EXPECT_TRUE(target_process.IsValid());

  int exit_code = -1;
  EXPECT_TRUE(target_process.WaitForExitWithTimeout(
      base::TimeDelta::FromSeconds(10), &exit_code));
  EXPECT_EQ(kChildExitCode, exit_code);
}

TEST_F(SandboxTest, RunSandboxTarget) {
  EXPECT_EQ(RESULT_CODE_SUCCESS,
            TestRunSandboxTarget(*base::CommandLine::ForCurrentProcess()));
}

}  // namespace chrome_cleaner
