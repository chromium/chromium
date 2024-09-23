// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/segmentation_platform/internal/signals/user_action_signal_handler.h"

#include "base/metrics/metrics_hashes.h"
#include "base/test/task_environment.h"
#include "components/segmentation_platform/internal/database/mock_signal_database.h"
#include "components/segmentation_platform/internal/database/mock_ukm_database.h"
#include "components/segmentation_platform/public/proto/types.pb.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::_;
using testing::Eq;

namespace segmentation_platform {

namespace {

const char kProfileId[] = "12";
const char kExpectedUserAction[] = "some_event";
const uint64_t kExpectedHash = base::HashMetricName(kExpectedUserAction);

MATCHER_P(SampleEq, hash, "") {
  EXPECT_EQ(arg.type, proto::SignalType::USER_ACTION);
  return arg.name_hash == hash;
}

}  // namespace

class UserActionSignalHandlerTest : public testing::Test {
 public:
  UserActionSignalHandlerTest() = default;
  ~UserActionSignalHandlerTest() override = default;

  void SetUp() override {
    base::SetRecordActionTaskRunner(
        task_environment_.GetMainThreadTaskRunner());
    signal_database_ = std::make_unique<MockSignalDatabase>();
    ukm_db_ = std::make_unique<MockUkmDatabase>();
    user_action_signal_handler_ = std::make_unique<UserActionSignalHandler>(
        kProfileId, signal_database_.get(), ukm_db_.get());
  }

  void SetupUserActions() {
    std::set<uint64_t> actions;
    actions.insert(kExpectedHash);
    user_action_signal_handler_->SetRelevantUserActions(actions);
  }

  base::test::TaskEnvironment task_environment_;
  std::unique_ptr<MockSignalDatabase> signal_database_;
  std::unique_ptr<MockUkmDatabase> ukm_db_;
  std::unique_ptr<UserActionSignalHandler> user_action_signal_handler_;
};

TEST_F(UserActionSignalHandlerTest, UserActionsAreRecorded) {
  // Initialize and register the list of user actions we are listening to.
  user_action_signal_handler_->EnableMetrics(true);
  SetupUserActions();

  // Fire a registered user action. It should be recorded.
  EXPECT_CALL(*signal_database_,
              WriteSample(proto::SignalType::USER_ACTION, kExpectedHash,
                          Eq(std::nullopt), _));
  EXPECT_CALL(*ukm_db_, AddUmaMetric(kProfileId, SampleEq(kExpectedHash)));
  base::RecordComputedActionAt(kExpectedUserAction, base::TimeTicks::Now());

  // Fire an unrelated user action. It should be ignored.
  std::string kUnrelatedUserAction = "unrelated_event";
  EXPECT_CALL(*signal_database_,
              WriteSample(proto::SignalType::USER_ACTION,
                          base::HashMetricName(kUnrelatedUserAction),
                          Eq(std::nullopt), _))
      .Times(0);
  EXPECT_CALL(*ukm_db_, AddUmaMetric(kProfileId, SampleEq(base::HashMetricName(
                                                     kUnrelatedUserAction))))
      .Times(0);
  base::RecordComputedActionAt(kUnrelatedUserAction, base::TimeTicks::Now());
}

TEST_F(UserActionSignalHandlerTest, DisableMetrics) {
  base::TimeTicks time = base::TimeTicks::Now();
  SetupUserActions();

  // Metrics is disabled on startup.
  EXPECT_CALL(*signal_database_,
              WriteSample(proto::SignalType::USER_ACTION,
                          base::HashMetricName(kExpectedUserAction),
                          Eq(std::nullopt), _))
      .Times(0);
  EXPECT_CALL(*ukm_db_,
              AddUmaMetric(kProfileId,
                           SampleEq(base::HashMetricName(kExpectedUserAction))))
      .Times(0);
  base::RecordComputedActionAt(kExpectedUserAction, time);

  // Enable metrics.
  user_action_signal_handler_->EnableMetrics(true);
  EXPECT_CALL(*signal_database_,
              WriteSample(proto::SignalType::USER_ACTION,
                          base::HashMetricName(kExpectedUserAction),
                          Eq(std::nullopt), _))
      .Times(1);
  EXPECT_CALL(*ukm_db_, AddUmaMetric(kProfileId, SampleEq(base::HashMetricName(
                                                     kExpectedUserAction))));
  base::RecordComputedActionAt(kExpectedUserAction, time);

  // Disable metrics again.
  user_action_signal_handler_->EnableMetrics(false);
  EXPECT_CALL(*signal_database_,
              WriteSample(proto::SignalType::USER_ACTION,
                          base::HashMetricName(kExpectedUserAction),
                          Eq(std::nullopt), _))
      .Times(0);
  EXPECT_CALL(*ukm_db_,
              AddUmaMetric(kProfileId,
                           SampleEq(base::HashMetricName(kExpectedUserAction))))
      .Times(0);
  base::RecordComputedActionAt(kExpectedUserAction, time);

  // Enable metrics again.
  user_action_signal_handler_->EnableMetrics(true);
  EXPECT_CALL(*signal_database_,
              WriteSample(proto::SignalType::USER_ACTION,
                          base::HashMetricName(kExpectedUserAction),
                          Eq(std::nullopt), _))
      .Times(1);
  EXPECT_CALL(*ukm_db_, AddUmaMetric(kProfileId, SampleEq(base::HashMetricName(
                                                     kExpectedUserAction))));
  base::RecordComputedActionAt(kExpectedUserAction, time);
}

}  // namespace segmentation_platform
