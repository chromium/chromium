// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/network_time/time_tracker/time_tracker.h"

#include "base/test/simple_test_clock.h"
#include "base/test/simple_test_tick_clock.h"
#include "base/time/time.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace network_time {

class TimeTrackerTest : public ::testing::Test {
 public:
  ~TimeTrackerTest() override = default;
  TimeTrackerTest() = default;

 protected:
  base::SimpleTestClock clock_;
  base::SimpleTestTickClock tick_clock_;
};

TEST_F(TimeTrackerTest, SystemClockIsWrong) {
  base::Time in_network_time = clock_.Now() - base::Days(90);
  TimeTracker tracker = TimeTracker(clock_.Now(), tick_clock_.NowTicks(),
                                    in_network_time, base::TimeDelta());

  base::Time out_network_time;
  EXPECT_TRUE(tracker.GetTime(clock_.Now(), tick_clock_.NowTicks(),
                              &out_network_time, nullptr));
  EXPECT_EQ(in_network_time, out_network_time);
}

TEST_F(TimeTrackerTest, SmallDivergence) {
  base::Time in_network_time = clock_.Now();
  TimeTracker tracker = TimeTracker(clock_.Now(), tick_clock_.NowTicks(),
                                    in_network_time, base::TimeDelta());
  // Make tick clock and clock diverge by 30 seconds, which is under the
  // allowed limit.
  tick_clock_.Advance(base::Seconds(30));
  base::Time out_network_time;
  EXPECT_TRUE(tracker.GetTime(clock_.Now(), tick_clock_.NowTicks(),
                              &out_network_time, nullptr));
  EXPECT_EQ(in_network_time + base::Seconds(30), out_network_time);
}

TEST_F(TimeTrackerTest, BigDivergence) {
  base::Time in_network_time = clock_.Now();
  TimeTracker tracker = TimeTracker(clock_.Now(), tick_clock_.NowTicks(),
                                    in_network_time, base::TimeDelta());
  // Make tick clock and clock diverge by 61 seconds, which is over the
  // allowed limit.
  tick_clock_.Advance(base::Seconds(61));
  base::Time out_network_time;
  EXPECT_FALSE(tracker.GetTime(clock_.Now(), tick_clock_.NowTicks(),
                               &out_network_time, nullptr));
}

TEST_F(TimeTrackerTest, TimeRanBackwards) {
  base::Time in_network_time = clock_.Now();
  TimeTracker tracker = TimeTracker(clock_.Now(), tick_clock_.NowTicks(),
                                    in_network_time, base::TimeDelta());
  clock_.Advance(base::Seconds(-1));
  base::Time out_network_time;
  EXPECT_FALSE(tracker.GetTime(clock_.Now(), tick_clock_.NowTicks(),
                               &out_network_time, nullptr));
}

TEST_F(TimeTrackerTest, UncertaintyCalculation) {
  base::Time in_network_time = clock_.Now();
  base::TimeDelta initial_uncertainty = base::Seconds(5);
  TimeTracker tracker = TimeTracker(clock_.Now(), tick_clock_.NowTicks(),
                                    in_network_time, initial_uncertainty);
  base::Time out_network_time;
  // The returned uncertainty should be equal to the initial uncertainty + the
  // divergence between clock and tick clock.
  base::TimeDelta out_uncertainty;
  tick_clock_.Advance(base::Seconds(3));
  EXPECT_TRUE(tracker.GetTime(clock_.Now(), tick_clock_.NowTicks(),
                              &out_network_time, &out_uncertainty));
  EXPECT_EQ(in_network_time + base::Seconds(3), out_network_time);
  EXPECT_EQ(out_uncertainty, base::Seconds(8));
}

}  // namespace network_time
