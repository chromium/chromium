// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/profiler/thread_profiler.h"

#include "base/macros.h"
#include "base/test/bind_test_util.h"
#include "base/test/simple_test_tick_clock.h"
#include "base/time/time.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

class TestScheduler : public PeriodicSamplingScheduler {
 public:
  TestScheduler(base::TimeDelta sampling_duration,
                double fraction_of_execution_time_to_sample)
      : PeriodicSamplingScheduler(sampling_duration,
                                  fraction_of_execution_time_to_sample,
                                  kStartTime),
        rand_double_value_(0.0) {
    tick_clock_.SetNowTicks(kStartTime);
  }

  double RandDouble() const override { return rand_double_value_; }
  base::TimeTicks Now() const override { return tick_clock_.NowTicks(); }

  void SetRandDouble(double value) { rand_double_value_ = value; }
  base::SimpleTestTickClock& tick_clock() { return tick_clock_; }

 private:
  static constexpr base::TimeTicks kStartTime = base::TimeTicks();
  base::SimpleTestTickClock tick_clock_;
  double rand_double_value_;

  DISALLOW_COPY_AND_ASSIGN(TestScheduler);
};

constexpr base::TimeTicks TestScheduler::kStartTime;

}  // namespace

TEST(ThreadProfilerTest, PeriodicSamplingScheduler) {
  const base::TimeDelta sampling_duration = base::TimeDelta::FromSeconds(30);
  const double fraction_of_execution_time_to_sample = 0.01;

  const base::TimeDelta expected_period =
      sampling_duration / fraction_of_execution_time_to_sample;

  TestScheduler scheduler(sampling_duration,
                          fraction_of_execution_time_to_sample);

  // The first collection should be exactly at the start time, since the random
  // value is 0.0.
  scheduler.SetRandDouble(0.0);
  EXPECT_EQ(base::TimeDelta::FromSeconds(0),
            scheduler.GetTimeToNextCollection());

  // With a random value of 1.0 the second collection should be at the end of
  // the second period.
  scheduler.SetRandDouble(1.0);
  EXPECT_EQ(2 * expected_period - sampling_duration,
            scheduler.GetTimeToNextCollection());

  // With a random value of 0.25 the second collection should be a quarter into
  // the third period exclusive of the sampling duration.
  scheduler.SetRandDouble(0.25);
  EXPECT_EQ(2 * expected_period + 0.25 * (expected_period - sampling_duration),
            scheduler.GetTimeToNextCollection());
}

TEST(ThreadProfilerTest, PeriodicSamplingSchedulerWithJumpInTimeTicks) {
  const base::TimeDelta sampling_duration = base::TimeDelta::FromSeconds(30);
  const double fraction_of_execution_time_to_sample = 0.01;

  const base::TimeDelta expected_period =
      sampling_duration / fraction_of_execution_time_to_sample;

  TestScheduler scheduler(sampling_duration,
                          fraction_of_execution_time_to_sample);

  // The first collection should be exactly at the start time, since the random
  // value is 0.0.
  scheduler.SetRandDouble(0.0);
  EXPECT_EQ(base::TimeDelta::FromSeconds(0),
            scheduler.GetTimeToNextCollection());

  // Simulate a non-continuous jump in the current TimeTicks such that the next
  // period would start before the current time. In this case the
  // period start should be reset to the current time, and the next collection
  // chosen within that period.
  scheduler.tick_clock().Advance(expected_period +
                                 base::TimeDelta::FromSeconds(1));
  scheduler.SetRandDouble(0.5);
  EXPECT_EQ(0.5 * (expected_period - sampling_duration),
            scheduler.GetTimeToNextCollection());
}
