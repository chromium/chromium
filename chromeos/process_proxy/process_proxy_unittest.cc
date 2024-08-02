// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include <gtest/gtest.h>
#include <stddef.h>

#include <memory>
#include <string>

#include "base/at_exit.h"
#include "base/command_line.h"
#include "base/functional/bind.h"
#include "base/location.h"
#include "base/memory/raw_ptr.h"
#include "base/process/kill.h"
#include "base/process/process.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "base/threading/thread.h"
#include "chromeos/process_proxy/process_proxy_registry.h"

namespace chromeos {

namespace {

// The test line must have all distinct characters.
const char kTestLineToSend[] = "abcdefgh\n";
const char kTestLineExpected[] = "abcdefgh\r\n";

const char kCatCommand[] = "cat";
const char kFakeUserHash[] = "0123456789abcdef";
const char kStdoutType[] = "stdout";
const int kTestLineNum = 100;

class TestRunner {
 public:
  TestRunner() = default;
  virtual ~TestRunner() = default;
  virtual void SetupExpectations(const std::string& id,
                                 const base::Process* process) = 0;
  virtual void OnSomeRead(const std::string& id,
                          const std::string& type,
                          const std::string& output) = 0;
  virtual void StartRegistryTest(ProcessProxyRegistry* registry) = 0;

  void set_done_read_closure(base::OnceClosure done_closure) {
    done_read_closure_ = std::move(done_closure);
  }

 protected:
  std::string id_;
  raw_ptr<const base::Process, AcrossTasksDanglingUntriaged> process_;

  base::OnceClosure done_read_closure_;
};

class RegistryTestRunner : public TestRunner {
 public:
  ~RegistryTestRunner() override = default;

  void SetupExpectations(const std::string& id,
                         const base::Process* process) override {
    id_ = id;
    process_ = process;
    left_to_check_index_[0] = 0;
    left_to_check_index_[1] = 0;
    // We consider that a line processing has started if a value in
    // left_to_check__[index] is set to 0, thus -2.
    lines_left_ = 2 * kTestLineNum - 2;
    expected_line_ = kTestLineExpected;
  }

  // Method to test validity of received input. We will receive two streams of
  // the same data. (input will be echoed twice by the testing process). Each
  // stream will contain the same string repeated |kTestLineNum| times. So we
  // have to match 2 * |kTestLineNum| lines. The problem is the received lines
  // from different streams may be interleaved (e.g. we may receive
  // abc|abcdef|defgh|gh). To deal with that, we allow to test received text
  // against two lines. The lines MUST NOT have two same characters for this
  // algorithm to work.
  void OnSomeRead(const std::string& id,
                  const std::string& type,
                  const std::string& output) override {
    EXPECT_EQ(type, kStdoutType);
    EXPECT_EQ(id_, id);

    bool valid = true;
    for (size_t i = 0; i < output.length(); i++) {
      // The character output[i] should be next in at least one of the lines we
      // are testing.
      valid = (ProcessReceivedCharacter(output[i], 0) ||
               ProcessReceivedCharacter(output[i], 1));
      EXPECT_TRUE(valid) << "Received: " << output;
    }

    if (!valid || TestSucceeded()) {
      ASSERT_FALSE(done_read_closure_.is_null());
      std::move(done_read_closure_).Run();
    }
  }

  void StartRegistryTest(ProcessProxyRegistry* registry) override {
    for (int i = 0; i < kTestLineNum; i++) {
      registry->SendInput(id_, kTestLineToSend, base::BindOnce([](bool result) {
                            EXPECT_TRUE(result);
                          }));
    }
  }

 private:
  bool ProcessReceivedCharacter(char received, size_t stream) {
    if (stream >= std::size(left_to_check_index_))
      return false;
    bool success = left_to_check_index_[stream] < expected_line_.length() &&
        expected_line_[left_to_check_index_[stream]] == received;
    if (success)
      left_to_check_index_[stream]++;
    if (left_to_check_index_[stream] == expected_line_.length() &&
        lines_left_ > 0) {
      // Take another line to test for this stream, if there are any lines left.
      // If not, this stream is done.
      left_to_check_index_[stream] = 0;
      lines_left_--;
    }
    return success;
  }

  bool TestSucceeded() {
    return left_to_check_index_[0] == expected_line_.length() &&
        left_to_check_index_[1] == expected_line_.length() &&
        lines_left_ == 0;
  }

  size_t left_to_check_index_[2];
  size_t lines_left_;
  std::string expected_line_;
};

class RegistryNotifiedOnProcessExitTestRunner : public TestRunner {
 public:
  ~RegistryNotifiedOnProcessExitTestRunner() override = default;

  void SetupExpectations(const std::string& id,
                         const base::Process* process) override {
    output_received_ = false;
    id_ = id;
    process_ = process;
  }

  void OnSomeRead(const std::string& id,
                  const std::string& type,
                  const std::string& output) override {
    EXPECT_EQ(id_, id);
    if (!output_received_) {
      base::ScopedAllowBaseSyncPrimitivesForTesting allow_sync_primitives;
      output_received_ = true;
      EXPECT_EQ(type, "stdout");
      EXPECT_EQ(output, "p");
      process_->Terminate(0, true);
      return;
    }
    EXPECT_EQ("exit", type);
    ASSERT_FALSE(done_read_closure_.is_null());
    std::move(done_read_closure_).Run();
  }

  void StartRegistryTest(ProcessProxyRegistry* registry) override {
    registry->SendInput(
        id_, "p", base::BindOnce([](bool result) { EXPECT_TRUE(result); }));
  }

 private:
  bool output_received_;
};

}  // namespace

class ProcessProxyTest : public testing::Test {
 public:
  ProcessProxyTest() = default;
  ~ProcessProxyTest() override = default;

 protected:
  void InitRegistryTest(base::OnceClosure done_closure) {
    registry_ = ProcessProxyRegistry::Get();

    base::CommandLine cmdline{{kCatCommand}};
    bool success = registry_->OpenProcess(
        cmdline, kFakeUserHash,
        base::BindRepeating(&ProcessProxyTest::HandleRead,
                            base::Unretained(this)),
        &id_);
    process_ = registry_->GetProcessForTesting(id_);

    EXPECT_TRUE(success);
    test_runner_->set_done_read_closure(std::move(done_closure));
    test_runner_->SetupExpectations(id_, process_);
    test_runner_->StartRegistryTest(registry_);
  }

  void HandleRead(const std::string& id,
                  const std::string& output_type,
                  const std::string& output) {
    test_runner_->OnSomeRead(id, output_type, output);
    registry_->AckOutput(id);
  }

  void EndRegistryTest(base::OnceClosure done_closure) {
    base::ScopedAllowBaseSyncPrimitivesForTesting allow_sync_primitives;

    registry_->CloseProcess(id_);

    int unused_exit_code = 0;
    base::TerminationStatus status =
        base::GetTerminationStatus(process_->Handle(), &unused_exit_code);
    EXPECT_NE(base::TERMINATION_STATUS_STILL_RUNNING, status);
    if (status == base::TERMINATION_STATUS_STILL_RUNNING) {
      process_->Terminate(0, true);
    }

    registry_->ShutDown();

    std::move(done_closure).Run();
  }

  void RunTest() {
    base::test::TestFuture<void> init_registry_waiter;
    ProcessProxyRegistry::GetTaskRunner()->PostTask(
        FROM_HERE,
        base::BindOnce(&ProcessProxyTest::InitRegistryTest,
                       base::Unretained(this),
                       init_registry_waiter.GetSequenceBoundCallback()));
    // Wait until all data from output watcher is received (QuitTask will be
    // fired on watcher thread).
    ASSERT_TRUE(init_registry_waiter.Wait());

    base::test::TestFuture<void> end_registry_waiter;
    ProcessProxyRegistry::GetTaskRunner()->PostTask(
        FROM_HERE,
        base::BindOnce(&ProcessProxyTest::EndRegistryTest,
                       base::Unretained(this),
                       end_registry_waiter.GetSequenceBoundCallback()));
    // Wait until we clean up the process proxy.
    ASSERT_TRUE(end_registry_waiter.Wait());
  }

  std::unique_ptr<TestRunner> test_runner_;

 private:
  // Destroys ProcessProxyRegistry LazyInstance after each test.
  base::ShadowingAtExitManager shadowing_at_exit_manager_;

  raw_ptr<ProcessProxyRegistry> registry_;
  std::string id_;
  raw_ptr<const base::Process, AcrossTasksDanglingUntriaged> process_ = nullptr;

  base::test::TaskEnvironment task_environment_;
};

// Test will open new process that will run cat command, and verify data we
// write to process gets echoed back.
TEST_F(ProcessProxyTest, RegistryTest) {
  test_runner_ = std::make_unique<RegistryTestRunner>();
  RunTest();
}

// Open new process, then kill it. Verifiy that we detect when the process dies.
//
// Disabled due to flakiness: https://crbug.com/1151205
TEST_F(ProcessProxyTest, DISABLED_RegistryNotifiedOnProcessExit) {
  test_runner_ = std::make_unique<RegistryNotifiedOnProcessExitTestRunner>();
  RunTest();
}

}  // namespace chromeos
