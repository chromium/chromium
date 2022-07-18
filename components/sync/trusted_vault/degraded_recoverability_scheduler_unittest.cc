// Copyright (c) 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/trusted_vault/degraded_recoverability_scheduler.h"

#include <memory>
#include "base/test/mock_callback.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace syncer {
namespace {

class DegradedRecoverabilitySchedulerTest : public ::testing::Test {
 public:
  DegradedRecoverabilitySchedulerTest() = default;
  ~DegradedRecoverabilitySchedulerTest() override = default;

  void SetUp() override {
    scheduler_ = std::make_unique<DegradedRecoverabilityScheduler>(
        refresh_callback_.Get());
    // Moving the time forward by one millisecond to make sure that the first
    // refresh had called.
    task_environment().FastForwardBy(base::Milliseconds(1));
  }

  DegradedRecoverabilityScheduler& scheduler() { return *scheduler_.get(); }

  base::MockCallback<base::RepeatingClosure>& refresh_callback() {
    return refresh_callback_;
  }

  base::test::SingleThreadTaskEnvironment& task_environment() {
    return task_environment_;
  }

 protected:
  base::MockCallback<base::RepeatingClosure> refresh_callback_;
  std::unique_ptr<DegradedRecoverabilityScheduler> scheduler_;
  base::test::SingleThreadTaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
};

TEST_F(DegradedRecoverabilitySchedulerTest, ShouldRefreshOnceWhenInitialize) {
  base::MockCallback<base::RepeatingClosure> refresh_callback;
  EXPECT_CALL(refresh_callback, Run());
  std::unique_ptr<DegradedRecoverabilityScheduler> scheduler =
      std::make_unique<DegradedRecoverabilityScheduler>(refresh_callback.Get());
  task_environment().FastForwardBy(base::Milliseconds(1));
}

TEST_F(DegradedRecoverabilitySchedulerTest, ShouldRefreshImmediately) {
  EXPECT_CALL(refresh_callback(), Run());
  scheduler().RefreshImmediately();
}

TEST_F(DegradedRecoverabilitySchedulerTest, ShouldRefreshOncePerLongPeriod) {
  EXPECT_CALL(refresh_callback(), Run());
  task_environment().FastForwardBy(kLongDegradedRecoverabilityRefreshPeriod +
                                   base::Milliseconds(1));
}

TEST_F(DegradedRecoverabilitySchedulerTest, ShouldSwitchToShortPeriod) {
  scheduler().StartShortIntervalRefreshing();
  EXPECT_CALL(refresh_callback(), Run());
  task_environment().FastForwardBy(kShortDegradedRecoverabilityRefreshPeriod +
                                   base::Milliseconds(1));
}

TEST_F(DegradedRecoverabilitySchedulerTest, ShouldSwitchToLongPeriod) {
  scheduler().StartShortIntervalRefreshing();
  scheduler().StartLongIntervalRefreshing();
  EXPECT_CALL(refresh_callback(), Run()).Times(0);
  task_environment().FastForwardBy(kShortDegradedRecoverabilityRefreshPeriod +
                                   base::Milliseconds(1));
  EXPECT_CALL(refresh_callback(), Run());
  task_environment().FastForwardBy(kLongDegradedRecoverabilityRefreshPeriod +
                                   base::Milliseconds(1));
}

TEST_F(DegradedRecoverabilitySchedulerTest,
       ShouldSwitchToShortPeriodAndAccountForTimePassed) {
  task_environment().FastForwardBy(kShortDegradedRecoverabilityRefreshPeriod -
                                   base::Seconds(1));
  scheduler().StartShortIntervalRefreshing();
  EXPECT_CALL(refresh_callback(), Run());
  task_environment().FastForwardBy(base::Seconds(1) + base::Milliseconds(1));
}

TEST_F(DegradedRecoverabilitySchedulerTest,
       ShouldSwitchToShortPeriodAndRefreshImmediately) {
  task_environment().FastForwardBy(kShortDegradedRecoverabilityRefreshPeriod +
                                   base::Seconds(1));
  EXPECT_CALL(refresh_callback(), Run());
  scheduler().StartShortIntervalRefreshing();
  task_environment().FastForwardBy(base::Milliseconds(1));
}

}  // namespace
}  // namespace syncer
