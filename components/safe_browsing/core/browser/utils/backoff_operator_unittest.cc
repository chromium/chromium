// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/safe_browsing/core/browser/utils/backoff_operator.h"

#include "base/test/task_environment.h"
#include "testing/platform_test.h"

namespace safe_browsing {

namespace {
const size_t kNumFailuresToEnforceBackoff = 3;
const size_t kMinBackOffResetDurationInSeconds = 5 * 60;   //  5 minutes.
const size_t kMaxBackOffResetDurationInSeconds = 30 * 60;  // 30 minutes.
}  // namespace

class BackoffOperatorTest : public PlatformTest {
 public:
  void SetUp() override {
    PlatformTest::SetUp();

    backoff_operator_ = std::make_unique<BackoffOperator>(
        kNumFailuresToEnforceBackoff, kMinBackOffResetDurationInSeconds,
        kMaxBackOffResetDurationInSeconds);
  }

  bool ReportError() { return backoff_operator_->ReportError(); }
  void ReportSuccess() { backoff_operator_->ReportSuccess(); }
  bool IsInBackoffMode() { return backoff_operator_->IsInBackoffMode(); }
  base::TimeDelta GetBackoffRemainingDuration() {
    return backoff_operator_->GetBackoffRemainingDuration();
  }

  std::unique_ptr<BackoffOperator> backoff_operator_;
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
};

TEST_F(BackoffOperatorTest, TestBackoffAndTimerReset) {
  // Not in backoff at the beginning.
  ASSERT_FALSE(IsInBackoffMode());
  EXPECT_EQ(GetBackoffRemainingDuration(), base::Seconds(0));

  // Failure 1: No backoff.
  EXPECT_FALSE(ReportError());
  EXPECT_FALSE(IsInBackoffMode());

  // Failure 2: No backoff.
  EXPECT_FALSE(ReportError());
  EXPECT_FALSE(IsInBackoffMode());

  // Failure 3: Entered backoff.
  EXPECT_TRUE(ReportError());
  EXPECT_TRUE(IsInBackoffMode());
  EXPECT_EQ(GetBackoffRemainingDuration(), base::Seconds(300));

  // Backoff not reset after 1 second.
  task_environment_.FastForwardBy(base::Seconds(1));
  EXPECT_TRUE(IsInBackoffMode());
  EXPECT_EQ(GetBackoffRemainingDuration(), base::Seconds(299));

  // Backoff not reset after 299 seconds.
  task_environment_.FastForwardBy(base::Seconds(298));
  EXPECT_TRUE(IsInBackoffMode());
  EXPECT_EQ(GetBackoffRemainingDuration(), base::Seconds(1));

  // Backoff should have been reset after 300 seconds.
  task_environment_.FastForwardBy(base::Seconds(1));
  EXPECT_FALSE(IsInBackoffMode());
  EXPECT_EQ(GetBackoffRemainingDuration(), base::Seconds(0));

  // Backoff should still be reset after 310 seconds.
  task_environment_.FastForwardBy(base::Seconds(10));
  EXPECT_FALSE(IsInBackoffMode());
  EXPECT_EQ(GetBackoffRemainingDuration(), base::Seconds(0));
}

TEST_F(BackoffOperatorTest, TestBackoffAndRequestSuccessReset) {
  // Not in backoff at the beginning.
  ASSERT_FALSE(IsInBackoffMode());

  // Failure 1: No backoff.
  EXPECT_FALSE(ReportError());
  EXPECT_FALSE(IsInBackoffMode());

  // Request success resets the backoff counter.
  ReportSuccess();
  EXPECT_FALSE(IsInBackoffMode());

  // Failure 1: No backoff.
  EXPECT_FALSE(ReportError());
  EXPECT_FALSE(IsInBackoffMode());

  // Failure 2: No backoff.
  EXPECT_FALSE(ReportError());
  EXPECT_FALSE(IsInBackoffMode());

  // Request success resets the backoff counter.
  ReportSuccess();
  EXPECT_FALSE(IsInBackoffMode());

  // Failure 1: No backoff.
  EXPECT_FALSE(ReportError());
  EXPECT_FALSE(IsInBackoffMode());

  // Failure 2: No backoff.
  EXPECT_FALSE(ReportError());
  EXPECT_FALSE(IsInBackoffMode());

  // Failure 3: Entered backoff.
  EXPECT_TRUE(ReportError());
  EXPECT_TRUE(IsInBackoffMode());

  // Request success resets the backoff counter.
  ReportSuccess();
  EXPECT_FALSE(IsInBackoffMode());
}

TEST_F(BackoffOperatorTest, TestExponentialBackoff) {
  ///////////////////////////////
  // Initial backoff: 300 seconds
  ///////////////////////////////

  // Not in backoff at the beginning.
  ASSERT_FALSE(IsInBackoffMode());

  // Failure 1: No backoff.
  EXPECT_FALSE(ReportError());
  EXPECT_FALSE(IsInBackoffMode());

  // Failure 2: No backoff.
  EXPECT_FALSE(ReportError());
  EXPECT_FALSE(IsInBackoffMode());

  // Failure 3: Entered backoff.
  EXPECT_TRUE(ReportError());
  EXPECT_TRUE(IsInBackoffMode());
  EXPECT_EQ(GetBackoffRemainingDuration(), base::Seconds(300));

  // Backoff not reset after 1 second.
  task_environment_.FastForwardBy(base::Seconds(1));
  EXPECT_TRUE(IsInBackoffMode());
  EXPECT_EQ(GetBackoffRemainingDuration(), base::Seconds(299));

  // Backoff not reset after 299 seconds.
  task_environment_.FastForwardBy(base::Seconds(298));
  EXPECT_TRUE(IsInBackoffMode());
  EXPECT_EQ(GetBackoffRemainingDuration(), base::Seconds(1));

  // Backoff should have been reset after 300 seconds.
  task_environment_.FastForwardBy(base::Seconds(1));
  EXPECT_FALSE(IsInBackoffMode());

  /////////////////////////////////////
  // Exponential backoff 1: 600 seconds
  /////////////////////////////////////

  EXPECT_FALSE(ReportError());
  EXPECT_FALSE(IsInBackoffMode());
  EXPECT_FALSE(ReportError());
  EXPECT_FALSE(IsInBackoffMode());
  EXPECT_TRUE(ReportError());
  EXPECT_TRUE(IsInBackoffMode());
  EXPECT_EQ(GetBackoffRemainingDuration(), base::Seconds(600));

  // Backoff not reset after 1 second.
  task_environment_.FastForwardBy(base::Seconds(1));
  EXPECT_TRUE(IsInBackoffMode());
  EXPECT_EQ(GetBackoffRemainingDuration(), base::Seconds(599));

  // Backoff not reset after 599 seconds.
  task_environment_.FastForwardBy(base::Seconds(598));
  EXPECT_TRUE(IsInBackoffMode());
  EXPECT_EQ(GetBackoffRemainingDuration(), base::Seconds(1));

  // Backoff should have been reset after 600 seconds.
  task_environment_.FastForwardBy(base::Seconds(1));
  EXPECT_FALSE(IsInBackoffMode());
  EXPECT_EQ(GetBackoffRemainingDuration(), base::Seconds(0));

  //////////////////////////////////////
  // Exponential backoff 2: 1200 seconds
  //////////////////////////////////////

  EXPECT_FALSE(ReportError());
  EXPECT_FALSE(IsInBackoffMode());
  EXPECT_FALSE(ReportError());
  EXPECT_FALSE(IsInBackoffMode());
  EXPECT_TRUE(ReportError());
  EXPECT_TRUE(IsInBackoffMode());
  EXPECT_EQ(GetBackoffRemainingDuration(), base::Seconds(1200));

  // Backoff not reset after 1 second.
  task_environment_.FastForwardBy(base::Seconds(1));
  EXPECT_TRUE(IsInBackoffMode());
  EXPECT_EQ(GetBackoffRemainingDuration(), base::Seconds(1199));

  // Backoff not reset after 1199 seconds.
  task_environment_.FastForwardBy(base::Seconds(1198));
  EXPECT_TRUE(IsInBackoffMode());
  EXPECT_EQ(GetBackoffRemainingDuration(), base::Seconds(1));

  // Backoff should have been reset after 1200 seconds.
  task_environment_.FastForwardBy(base::Seconds(1));
  EXPECT_FALSE(IsInBackoffMode());
  EXPECT_EQ(GetBackoffRemainingDuration(), base::Seconds(0));

  ///////////////////////////////////////////////////
  // Exponential backoff 3: 1800 seconds (30 minutes)
  ///////////////////////////////////////////////////

  EXPECT_FALSE(ReportError());
  EXPECT_FALSE(IsInBackoffMode());
  EXPECT_FALSE(ReportError());
  EXPECT_FALSE(IsInBackoffMode());
  EXPECT_TRUE(ReportError());
  EXPECT_TRUE(IsInBackoffMode());
  EXPECT_EQ(GetBackoffRemainingDuration(), base::Seconds(1800));

  // Backoff not reset after 1 second.
  task_environment_.FastForwardBy(base::Seconds(1));
  EXPECT_TRUE(IsInBackoffMode());
  EXPECT_EQ(GetBackoffRemainingDuration(), base::Seconds(1799));

  // Backoff not reset after 1799 seconds.
  task_environment_.FastForwardBy(base::Seconds(1798));
  EXPECT_TRUE(IsInBackoffMode());
  EXPECT_EQ(GetBackoffRemainingDuration(), base::Seconds(1));

  // Backoff should have been reset after 1800 seconds.
  task_environment_.FastForwardBy(base::Seconds(1));
  EXPECT_FALSE(IsInBackoffMode());
  EXPECT_EQ(GetBackoffRemainingDuration(), base::Seconds(0));

  ///////////////////////////////////////////////////
  // Exponential backoff 4: 1800 seconds (30 minutes)
  ///////////////////////////////////////////////////

  EXPECT_FALSE(ReportError());
  EXPECT_FALSE(IsInBackoffMode());
  EXPECT_FALSE(ReportError());
  EXPECT_FALSE(IsInBackoffMode());
  EXPECT_TRUE(ReportError());
  EXPECT_TRUE(IsInBackoffMode());
  EXPECT_EQ(GetBackoffRemainingDuration(), base::Seconds(1800));

  // Backoff not reset after 1 second.
  task_environment_.FastForwardBy(base::Seconds(1));
  EXPECT_TRUE(IsInBackoffMode());
  EXPECT_EQ(GetBackoffRemainingDuration(), base::Seconds(1799));

  // Backoff not reset after 1799 seconds.
  task_environment_.FastForwardBy(base::Seconds(1798));
  EXPECT_TRUE(IsInBackoffMode());
  EXPECT_EQ(GetBackoffRemainingDuration(), base::Seconds(1));

  // Backoff should have been reset after 1800 seconds.
  task_environment_.FastForwardBy(base::Seconds(1));
  EXPECT_FALSE(IsInBackoffMode());
  EXPECT_EQ(GetBackoffRemainingDuration(), base::Seconds(0));
}

TEST_F(BackoffOperatorTest, TestExponentialBackoffWithResetOnSuccess) {
  ///////////////////////////////
  // Initial backoff: 300 seconds
  ///////////////////////////////

  // Not in backoff at the beginning.
  ASSERT_FALSE(IsInBackoffMode());

  // Failure 1: No backoff.
  EXPECT_FALSE(ReportError());
  EXPECT_FALSE(IsInBackoffMode());

  // Failure 2: No backoff.
  EXPECT_FALSE(ReportError());
  EXPECT_FALSE(IsInBackoffMode());

  // Failure 3: Entered backoff.
  EXPECT_TRUE(ReportError());
  EXPECT_TRUE(IsInBackoffMode());
  EXPECT_EQ(GetBackoffRemainingDuration(), base::Seconds(300));

  // Backoff not reset after 1 second.
  task_environment_.FastForwardBy(base::Seconds(1));
  EXPECT_TRUE(IsInBackoffMode());
  EXPECT_EQ(GetBackoffRemainingDuration(), base::Seconds(299));

  // Backoff not reset after 299 seconds.
  task_environment_.FastForwardBy(base::Seconds(298));
  EXPECT_TRUE(IsInBackoffMode());
  EXPECT_EQ(GetBackoffRemainingDuration(), base::Seconds(1));

  // Backoff should have been reset after 300 seconds.
  task_environment_.FastForwardBy(base::Seconds(1));
  EXPECT_FALSE(IsInBackoffMode());
  EXPECT_EQ(GetBackoffRemainingDuration(), base::Seconds(0));

  /////////////////////////////////////
  // Exponential backoff 1: 600 seconds
  /////////////////////////////////////

  EXPECT_FALSE(ReportError());
  EXPECT_FALSE(IsInBackoffMode());
  EXPECT_FALSE(ReportError());
  EXPECT_FALSE(IsInBackoffMode());
  EXPECT_TRUE(ReportError());
  EXPECT_TRUE(IsInBackoffMode());
  EXPECT_EQ(GetBackoffRemainingDuration(), base::Seconds(600));

  // Backoff not reset after 1 second.
  task_environment_.FastForwardBy(base::Seconds(1));
  EXPECT_TRUE(IsInBackoffMode());
  EXPECT_EQ(GetBackoffRemainingDuration(), base::Seconds(599));

  // Backoff not reset after 599 seconds.
  task_environment_.FastForwardBy(base::Seconds(598));
  EXPECT_TRUE(IsInBackoffMode());
  EXPECT_EQ(GetBackoffRemainingDuration(), base::Seconds(1));

  // Backoff should have been reset after 600 seconds.
  task_environment_.FastForwardBy(base::Seconds(1));
  EXPECT_FALSE(IsInBackoffMode());
  EXPECT_EQ(GetBackoffRemainingDuration(), base::Seconds(0));

  // The next request is a success. This should reset the backoff duration to
  // |min_backoff_reset_duration_in_seconds_|
  ReportSuccess();

  // Failure 1: No backoff.
  EXPECT_FALSE(ReportError());
  EXPECT_FALSE(IsInBackoffMode());

  // Failure 2: No backoff.
  EXPECT_FALSE(ReportError());
  EXPECT_FALSE(IsInBackoffMode());

  // Failure 3: Entered backoff.
  EXPECT_TRUE(ReportError());
  EXPECT_TRUE(IsInBackoffMode());
  EXPECT_EQ(GetBackoffRemainingDuration(), base::Seconds(300));

  // Backoff not reset after 1 second.
  task_environment_.FastForwardBy(base::Seconds(1));
  EXPECT_TRUE(IsInBackoffMode());
  EXPECT_EQ(GetBackoffRemainingDuration(), base::Seconds(299));

  // Backoff not reset after 299 seconds.
  task_environment_.FastForwardBy(base::Seconds(298));
  EXPECT_TRUE(IsInBackoffMode());
  EXPECT_EQ(GetBackoffRemainingDuration(), base::Seconds(1));

  // Backoff should have been reset after 300 seconds.
  task_environment_.FastForwardBy(base::Seconds(1));
  EXPECT_FALSE(IsInBackoffMode());
  EXPECT_EQ(GetBackoffRemainingDuration(), base::Seconds(0));
}

}  // namespace safe_browsing
