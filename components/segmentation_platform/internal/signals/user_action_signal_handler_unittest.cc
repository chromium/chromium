// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/segmentation_platform/internal/signals/user_action_signal_handler.h"

#include "base/metrics/metrics_hashes.h"
#include "base/test/task_environment.h"
#include "components/segmentation_platform/internal/database/user_action_database.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::_;

namespace segmentation_platform {

namespace {
const char kExpectedUserAction[] = "some_event";
const uint64_t kExpectedHash = base::HashMetricName(kExpectedUserAction);

}  // namespace

class MockUserActionDatabase : public UserActionDatabase {
 public:
  MockUserActionDatabase() = default;
  MOCK_METHOD(void, WriteUserAction, (uint64_t, base::TimeTicks));
};

class UserActionSignalHandlerTest : public testing::Test {
 public:
  UserActionSignalHandlerTest() = default;
  ~UserActionSignalHandlerTest() override = default;

  void SetUp() override {
    base::SetRecordActionTaskRunner(
        task_environment_.GetMainThreadTaskRunner());
    user_action_database_ = std::make_unique<MockUserActionDatabase>();
    user_action_signal_handler_ =
        std::make_unique<UserActionSignalHandler>(user_action_database_.get());
  }

  void SetupUserActions() {
    std::set<uint64_t> actions;
    actions.insert(kExpectedHash);
    user_action_signal_handler_->SetRelevantUserActions(actions);
  }

  base::test::TaskEnvironment task_environment_;
  std::unique_ptr<MockUserActionDatabase> user_action_database_;
  std::unique_ptr<UserActionSignalHandler> user_action_signal_handler_;
};

TEST_F(UserActionSignalHandlerTest, UserActionsAreRecorded) {
  base::TimeTicks time = base::TimeTicks::Now();

  // Initialize and register the list of user actions we are listening to.
  user_action_signal_handler_->EnableMetrics(true);
  SetupUserActions();

  // Fire a registered user action. It should be recorded.
  EXPECT_CALL(*user_action_database_, WriteUserAction(kExpectedHash, time));
  // user_action_signal_handler_->EnableMetrics(true);
  base::RecordComputedActionAt(kExpectedUserAction, time);

  // Fire an unrelated user action. It should be ignored.
  std::string kUnrelatedUserAction = "unrelated_event";
  EXPECT_CALL(*user_action_database_,
              WriteUserAction(base::HashMetricName(kUnrelatedUserAction), time))
      .Times(0);
  base::RecordComputedActionAt(kUnrelatedUserAction, time);
}

TEST_F(UserActionSignalHandlerTest, DisableMetrics) {
  base::TimeTicks time = base::TimeTicks::Now();
  SetupUserActions();

  // Metrics is disabled on startup.
  EXPECT_CALL(*user_action_database_,
              WriteUserAction(base::HashMetricName(kExpectedUserAction), time))
      .Times(0);
  base::RecordComputedActionAt(kExpectedUserAction, time);

  // Enable metrics.
  user_action_signal_handler_->EnableMetrics(true);
  EXPECT_CALL(*user_action_database_,
              WriteUserAction(base::HashMetricName(kExpectedUserAction), time))
      .Times(1);
  base::RecordComputedActionAt(kExpectedUserAction, time);

  // Disable metrics again.
  user_action_signal_handler_->EnableMetrics(false);
  EXPECT_CALL(*user_action_database_,
              WriteUserAction(base::HashMetricName(kExpectedUserAction), time))
      .Times(0);
  base::RecordComputedActionAt(kExpectedUserAction, time);

  // Enable metrics again.
  user_action_signal_handler_->EnableMetrics(true);
  EXPECT_CALL(*user_action_database_,
              WriteUserAction(base::HashMetricName(kExpectedUserAction), time))
      .Times(1);
  base::RecordComputedActionAt(kExpectedUserAction, time);
}

}  // namespace segmentation_platform
