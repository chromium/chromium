// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/segmentation_platform/internal/signals/signal_filter_processor.h"

#include "base/metrics/metrics_hashes.h"
#include "base/run_loop.h"
#include "base/test/task_environment.h"
#include "components/segmentation_platform/internal/database/segment_info_database.h"
#include "components/segmentation_platform/internal/database/test_segment_info_database.h"
#include "components/segmentation_platform/internal/signals/user_action_signal_handler.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::_;
using testing::Contains;
using testing::SaveArg;

namespace segmentation_platform {

class MockUserActionSignalHandler : public UserActionSignalHandler {
 public:
  MockUserActionSignalHandler() : UserActionSignalHandler(nullptr) {}
  MOCK_METHOD(void, SetRelevantUserActions, (std::set<uint64_t>));
  MOCK_METHOD(void, EnableMetrics, (bool));
};

class SignalFilterProcessorTest : public testing::Test {
 public:
  SignalFilterProcessorTest() = default;
  ~SignalFilterProcessorTest() override = default;

  void SetUp() override {
    base::SetRecordActionTaskRunner(
        task_environment_.GetMainThreadTaskRunner());
    segment_database_ = std::make_unique<test::TestSegmentInfoDatabase>();
    user_action_signal_handler_ =
        std::make_unique<MockUserActionSignalHandler>();
    signal_filter_processor_ = std::make_unique<SignalFilterProcessor>(
        segment_database_.get(), user_action_signal_handler_.get());
  }

  base::test::TaskEnvironment task_environment_;
  std::unique_ptr<test::TestSegmentInfoDatabase> segment_database_;
  std::unique_ptr<MockUserActionSignalHandler> user_action_signal_handler_;
  std::unique_ptr<SignalFilterProcessor> signal_filter_processor_;
};

TEST_F(SignalFilterProcessorTest, UserActionRegistrationFlow) {
  std::string kUserActionName1 = "some_action_1";
  segment_database_->AddUserAction(
      OptimizationTarget::OPTIMIZATION_TARGET_PAGE_TOPICS, kUserActionName1);
  std::string kUserActionName2 = "some_action_2";
  segment_database_->AddUserAction(
      OptimizationTarget::OPTIMIZATION_TARGET_LANGUAGE_DETECTION,
      kUserActionName2);

  std::set<uint64_t> actions;
  EXPECT_CALL(*user_action_signal_handler_, SetRelevantUserActions(_))
      .Times(1)
      .WillOnce(SaveArg<0>(&actions));

  signal_filter_processor_->OnSignalListUpdated();
  ASSERT_THAT(actions, Contains(base::HashMetricName(kUserActionName1)));
  ASSERT_THAT(actions, Contains(base::HashMetricName(kUserActionName2)));
}

TEST_F(SignalFilterProcessorTest, EnableMetrics) {
  EXPECT_CALL(*user_action_signal_handler_, EnableMetrics(true));
  signal_filter_processor_->EnableMetrics(true);
  EXPECT_CALL(*user_action_signal_handler_, EnableMetrics(false));
  signal_filter_processor_->EnableMetrics(false);
}

}  // namespace segmentation_platform
