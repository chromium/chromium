// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "chrome/browser/ui/tabs/organization/trigger_policies.h"

#include "base/test/metrics/histogram_tester.h"
#include "base/test/simple_test_tick_clock.h"
#include "base/test/task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

class UsageTickClockTest : public testing::Test {
 public:
  UsageTickClockTest() {
    metrics::DesktopSessionDurationTracker::Initialize();
    clock_ = std::make_unique<UsageTickClock>(&inner_clock_);
  }

  ~UsageTickClockTest() override {
    clock_.reset();
    metrics::DesktopSessionDurationTracker::CleanupForTesting();
  }

  UsageTickClock* clock() { return clock_.get(); }
  base::SimpleTestTickClock* inner_clock() { return &inner_clock_; }

 private:
  // Required to use DesktopSessionDurationTracker.
  base::test::TaskEnvironment task_environment;
  base::SimpleTestTickClock inner_clock_;
  std::unique_ptr<UsageTickClock> clock_;
};

TEST_F(UsageTickClockTest, TestClock) {
  const base::TimeTicks start_time = clock()->NowTicks();
  auto* tracker = metrics::DesktopSessionDurationTracker::Get();

  // Time stands still while Chrome is not in use.
  inner_clock()->Advance(base::Minutes(1));
  EXPECT_EQ(start_time, clock()->NowTicks());

  // Time moves normally while Chrome is in use.
  tracker->OnVisibilityChanged(true, base::TimeDelta());
  tracker->OnUserEvent();
  inner_clock()->Advance(base::Minutes(1));
  EXPECT_EQ(base::Minutes(1), clock()->NowTicks() - start_time);

  // Only active time is counted if there's some active and some inactive time.
  inner_clock()->Advance(base::Minutes(1));
  tracker->OnVisibilityChanged(false, base::TimeDelta());
  inner_clock()->Advance(base::Minutes(1));
  EXPECT_EQ(base::Minutes(2), clock()->NowTicks() - start_time);
}

class FakeBackoffLevelProvider final : public BackoffLevelProvider {
  unsigned int Get() const override { return level_; }
  void Increment() override { level_++; }
  void Decrement() override { level_ = std::max(1u, level_) - 1; }

 private:
  unsigned int level_ = 0;
};

class TargetFrequencyTriggerTest : public testing::Test {
 public:
  TargetFrequencyTriggerTest() {
    auto clock = std::make_unique<base::SimpleTestTickClock>();
    clock_ = clock.get();
    backoff_level_provider_ = std::make_unique<FakeBackoffLevelProvider>();
    policy_ = std::make_unique<TargetFrequencyTriggerPolicy>(
        std::move(clock), base::Days(1.0f), 2.0f,
        backoff_level_provider_.get());

    // Start 10% into the period so +1 period doesn't put us exactly on the
    // rollover instant.
    clock_->Advance(base::Days(0.1f));
  }

  void advance_to_next_phase() { clock()->Advance(base::Days(0.5f)); }

  base::SimpleTestTickClock* clock() { return clock_.get(); }
  TargetFrequencyTriggerPolicy* policy() { return policy_.get(); }

 private:
  std::unique_ptr<BackoffLevelProvider> backoff_level_provider_;
  std::unique_ptr<TargetFrequencyTriggerPolicy> policy_;
  raw_ptr<base::SimpleTestTickClock> clock_;
};

TEST_F(TargetFrequencyTriggerTest, TriggersWhenBestScoreBeaten) {
  EXPECT_FALSE(policy()->ShouldTrigger(1.0f));

  advance_to_next_phase();

  EXPECT_FALSE(policy()->ShouldTrigger(0.5f));
  EXPECT_TRUE(policy()->ShouldTrigger(2.0f));
}

TEST_F(TargetFrequencyTriggerTest, DoesntTriggerInObservationPhase) {
  EXPECT_FALSE(policy()->ShouldTrigger(1.0f));
}

TEST_F(TargetFrequencyTriggerTest, DoesntTriggerWithoutObservation) {
  advance_to_next_phase();

  EXPECT_FALSE(policy()->ShouldTrigger(9001.0f));
}

TEST_F(TargetFrequencyTriggerTest, TriggersOncePerPeriod) {
  EXPECT_FALSE(policy()->ShouldTrigger(1.0f));

  advance_to_next_phase();

  EXPECT_TRUE(policy()->ShouldTrigger(2.0f));
  EXPECT_FALSE(policy()->ShouldTrigger(9001.0f));
}

TEST_F(TargetFrequencyTriggerTest, TriggersAgainNextPeriod) {
  // Go through one period, with a trigger.
  EXPECT_FALSE(policy()->ShouldTrigger(1.0f));
  advance_to_next_phase();
  EXPECT_TRUE(policy()->ShouldTrigger(2.0f));

  // Next period, observe.
  advance_to_next_phase();
  EXPECT_FALSE(policy()->ShouldTrigger(0.5f));

  advance_to_next_phase();

  // Should trigger again, as long as we beat this period's best score.
  EXPECT_TRUE(policy()->ShouldTrigger(0.75f));
}

TEST_F(TargetFrequencyTriggerTest, BacksOffAfterFailure) {
  EXPECT_FALSE(policy()->ShouldTrigger(1.0f));

  policy()->OnTriggerFailed();

  advance_to_next_phase();
  EXPECT_FALSE(policy()->ShouldTrigger(2.0f));
  advance_to_next_phase();
  EXPECT_TRUE(policy()->ShouldTrigger(3.0f));
}

TEST_F(TargetFrequencyTriggerTest, UnBacksOffAfterSuccess) {
  EXPECT_FALSE(policy()->ShouldTrigger(1.0f));
  policy()->OnTriggerFailed();

  policy()->OnTriggerSucceeded();

  advance_to_next_phase();
  EXPECT_TRUE(policy()->ShouldTrigger(2.0f));
}

TEST_F(TargetFrequencyTriggerTest, PeriodClampedToBasePeriod) {
  EXPECT_FALSE(policy()->ShouldTrigger(1.0f));

  policy()->OnTriggerSucceeded();

  advance_to_next_phase();
  EXPECT_TRUE(policy()->ShouldTrigger(2.0f));
}

TEST_F(TargetFrequencyTriggerTest,
       HistogramLoggedOnShouldTriggerAfterPeriodExpiration) {
  base::HistogramTester histogram_tester;

  // Observation phase 1
  EXPECT_FALSE(policy()->ShouldTrigger(1.0f));
  advance_to_next_phase();

  // trigger phase 1
  EXPECT_TRUE(policy()->ShouldTrigger(2.0f));
  advance_to_next_phase();

  // observation phase 2. should log the results of triggering from phase 1.
  EXPECT_FALSE(policy()->ShouldTrigger(1.0f));
  histogram_tester.ExpectBucketCount(
      "Tab.Organization.Trigger.TriggeredInPeriod", true, 1);
  advance_to_next_phase();

  // trigger phase 2
  EXPECT_FALSE(policy()->ShouldTrigger(0.0f));
  advance_to_next_phase();

  // observation phase 3. should log the results of triggering from phase 2.
  EXPECT_FALSE(policy()->ShouldTrigger(1.0f));
  histogram_tester.ExpectBucketCount(
      "Tab.Organization.Trigger.TriggeredInPeriod", false, 1);
}
