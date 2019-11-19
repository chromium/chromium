// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/chrome_cleaner/components/recovery_component.h"

#include <memory>
#include <string>
#include <utility>

#include "base/files/file.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/test/task_environment.h"
#include "base/test/test_simple_task_runner.h"
#include "chrome/chrome_cleaner/http/mock_http_agent_factory.h"
#include "chrome/chrome_cleaner/test/test_pup_data.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chrome_cleaner {

namespace {

const UwSId kRemovableUwSId = 42;
const UwSId kReportOnlyUwSId = 21;

}  // namespace

class TestRecoveryComponent : public RecoveryComponent {
 public:
  TestRecoveryComponent() = default;

  using RecoveryComponent::WaitForDoneExpandingCrxForTest;

  bool run_called() const { return run_called_; }
  bool unpack_component_called() const { return unpack_component_called_; }
  const std::string& crx_file_contents() const { return crx_file_contents_; }

 protected:
  // RecoveryComponent.
  void Run() override { run_called_ = true; }
  void UnpackComponent(const base::FilePath& crx_file) override {
    unpack_component_called_ = true;

    if (base::PathExists(crx_file)) {
      ASSERT_TRUE(base::ReadFileToString(crx_file, &crx_file_contents_));
    }
  }

 private:
  bool run_called_{false};
  bool unpack_component_called_{false};
  std::string crx_file_contents_;
};

class RecoveryComponentTest : public testing::Test {
 public:
  void SetUp() override {
    RecoveryComponent::SetHttpAgentFactoryForTesting(factory_.get());
  }

  void TearDown() override {
    RecoveryComponent::SetHttpAgentFactoryForTesting(nullptr);
  }

 protected:
  RecoveryComponentTest() : task_runner_(new base::TestSimpleTaskRunner) {}

  // Needed for the current task runner to be available.
  base::test::SingleThreadTaskEnvironment task_environment_{
      base::test::SingleThreadTaskEnvironment::MainThreadType::UI};

  // The recover component under test. This declaration must be after the
  // |ui_message_loop_| because the |RecoveryComponent| constructor needs
  // a reference to the current run loop.
  TestRecoveryComponent recovery_component_;

  // A task runner and a URL fetcher factory are necessary to test URL requests.
  scoped_refptr<base::TestSimpleTaskRunner> task_runner_;

  MockHttpAgentConfig config_;
  std::unique_ptr<HttpAgentFactory> factory_{
      std::make_unique<MockHttpAgentFactory>(&config_)};
};

TEST_F(RecoveryComponentTest, Success) {
  MockHttpAgentConfig::Calls calls(HttpStatus::kOk);
  config_.AddCalls(calls);

  recovery_component_.PreScan();
  recovery_component_.WaitForDoneExpandingCrxForTest();
  EXPECT_TRUE(recovery_component_.unpack_component_called());
}

TEST_F(RecoveryComponentTest, Failure) {
  MockHttpAgentConfig::Calls calls(HttpStatus::kOk);
  calls.request_succeeds = false;
  config_.AddCalls(calls);

  recovery_component_.PreScan();
  recovery_component_.WaitForDoneExpandingCrxForTest();
  EXPECT_FALSE(recovery_component_.unpack_component_called());
}

TEST_F(RecoveryComponentTest, CrxDataSavedToDisk) {
  const std::string test_data = "test data";

  MockHttpAgentConfig::Calls calls(HttpStatus::kOk);
  calls.read_data_result = test_data;
  config_.AddCalls(calls);

  recovery_component_.PreScan();
  recovery_component_.WaitForDoneExpandingCrxForTest();
  EXPECT_TRUE(recovery_component_.unpack_component_called());
  EXPECT_EQ(test_data, recovery_component_.crx_file_contents());
}

TEST_F(RecoveryComponentTest, CrxDataPartiallySavedToDisk) {
  // Ensure the buffer is large enough, so a second buffer needs to be read.
  const std::string test_data =
      std::string(RecoveryComponent::kReadDataFromResponseBufferSize + 1, 'x');

  MockHttpAgentConfig::Calls calls(HttpStatus::kOk);
  calls.read_data_result = test_data;
  calls.read_data_success_sequence.push_back(true);
  calls.read_data_success_sequence.push_back(false);
  config_.AddCalls(calls);

  recovery_component_.PreScan();
  recovery_component_.WaitForDoneExpandingCrxForTest();
  EXPECT_FALSE(recovery_component_.unpack_component_called());
}

TEST_F(RecoveryComponentTest, RunCalledForRemovablePUP) {
  const UwSId kRemovableUwSId = 42;
  std::vector<UwSId> found_pups;
  TestPUPData test_pup_data;

  // When a removable PUP was found, the post scan shouldn't run, it should
  // wait for the post-cleanup.
  test_pup_data.AddPUP(kRemovableUwSId, PUPData::FLAGS_ACTION_REMOVE, nullptr,
                       PUPData::kMaxFilesToRemoveSmallUwS);
  found_pups.push_back(kRemovableUwSId);
  recovery_component_.PostScan(found_pups);
  EXPECT_FALSE(recovery_component_.run_called());
  recovery_component_.PostCleanup(RESULT_CODE_SUCCESS, nullptr);
  EXPECT_TRUE(recovery_component_.run_called());
}

TEST_F(RecoveryComponentTest, RunCalledForNoPUPs) {
  std::vector<UwSId> found_pups;
  TestPUPData test_pup_data;

  // When no PUPs are found, the post scan should run, but not the post-cleanup.
  found_pups.clear();
  recovery_component_.PostScan(found_pups);
  EXPECT_TRUE(recovery_component_.run_called());
}

TEST_F(RecoveryComponentTest, RunCalledForReportOnlyUwS) {
  std::vector<UwSId> found_pups;
  TestPUPData test_pup_data;

  // When only report only PUPs are found, the post scan should run, but not the
  // post-cleanup.
  test_pup_data.AddPUP(kReportOnlyUwSId, PUPData::FLAGS_NONE, nullptr,
                       PUPData::kMaxFilesToRemoveSmallUwS);
  found_pups.push_back(kReportOnlyUwSId);
  recovery_component_.PostScan(found_pups);
  EXPECT_TRUE(recovery_component_.run_called());
}

TEST_F(RecoveryComponentTest, RunNotCalledPreReboot) {
  std::vector<UwSId> found_pups;
  TestPUPData test_pup_data;

  // And finally, when a reboot will be needed, none should call run.
  test_pup_data.AddPUP(kRemovableUwSId, PUPData::FLAGS_ACTION_REMOVE, nullptr,
                       PUPData::kMaxFilesToRemoveSmallUwS);
  found_pups.push_back(kRemovableUwSId);
  recovery_component_.PostScan(found_pups);
  EXPECT_FALSE(recovery_component_.run_called());
  recovery_component_.PostCleanup(RESULT_CODE_PENDING_REBOOT, nullptr);
  EXPECT_FALSE(recovery_component_.run_called());
}

}  // namespace chrome_cleaner
