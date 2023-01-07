// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill_assistant/browser/actions/stopwatch.h"

#include "base/logging.h"
#include "base/time/time.h"
#include "base/time/time_override.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace autofill_assistant {
namespace {

class TimeTicksOverride {
 public:
  static base::TimeTicks Now() { return now_ticks_; }

  static base::TimeTicks now_ticks_;
};

// static
base::TimeTicks TimeTicksOverride::now_ticks_ = base::TimeTicks::Now();

class StopwatchTest : public testing::Test {
 protected:
  Stopwatch stopwatch_;
};

TEST_F(StopwatchTest, SimpleRun) {
  base::subtle::ScopedTimeClockOverrides overrides(
      nullptr, &TimeTicksOverride::Now, nullptr);
  EXPECT_TRUE(stopwatch_.Start());
  EXPECT_FALSE(stopwatch_.Start());
  EXPECT_TRUE(stopwatch_.IsRunning());

  TimeTicksOverride::now_ticks_ += base::Seconds(1);
  EXPECT_EQ(base::Seconds(1), stopwatch_.TotalElapsed());

  TimeTicksOverride::now_ticks_ += base::Seconds(2);
  EXPECT_TRUE(stopwatch_.Stop());
  EXPECT_FALSE(stopwatch_.Stop());
  EXPECT_EQ(base::Seconds(3), stopwatch_.TotalElapsed());

  TimeTicksOverride::now_ticks_ += base::Seconds(2);
  EXPECT_TRUE(stopwatch_.Start());
  TimeTicksOverride::now_ticks_ += base::Seconds(1);
  EXPECT_EQ(base::Seconds(4), stopwatch_.TotalElapsed());
  TimeTicksOverride::now_ticks_ += base::Seconds(1);
  EXPECT_TRUE(stopwatch_.Stop());
  EXPECT_EQ(base::Seconds(5), stopwatch_.TotalElapsed());
}

TEST_F(StopwatchTest, AddTime) {
  base::subtle::ScopedTimeClockOverrides overrides(
      nullptr, &TimeTicksOverride::Now, nullptr);
  stopwatch_.Start();
  TimeTicksOverride::now_ticks_ += base::Seconds(1);
  stopwatch_.Stop();
  stopwatch_.AddTime(base::Seconds(2));
  EXPECT_EQ(base::Seconds(3), stopwatch_.TotalElapsed());
}

TEST_F(StopwatchTest, RemoveTime) {
  base::subtle::ScopedTimeClockOverrides overrides(
      nullptr, &TimeTicksOverride::Now, nullptr);
  stopwatch_.Start();
  TimeTicksOverride::now_ticks_ += base::Seconds(2);
  stopwatch_.Stop();
  stopwatch_.RemoveTime(base::Seconds(1));
  EXPECT_EQ(base::Seconds(1), stopwatch_.TotalElapsed());
}

TEST_F(StopwatchTest, RemoveGreaterThanElapsed) {
  base::subtle::ScopedTimeClockOverrides overrides(
      nullptr, &TimeTicksOverride::Now, nullptr);
  stopwatch_.Start();
  TimeTicksOverride::now_ticks_ += base::Seconds(1);
  stopwatch_.Stop();
  stopwatch_.RemoveTime(base::Seconds(2));
  EXPECT_EQ(base::Seconds(0), stopwatch_.TotalElapsed());
}

// This parameterized test uses 4 parameters: time to start at, time to stop at,
// expected time before the stop is invoked and expected time at the end.
// If start at or stop at times are 0, the regular Start() and Stop() methods
// are used instead of the StartAt(time) or StopAt(time).
class StopwatchStartStopTest
    : public StopwatchTest,
      public testing::WithParamInterface<std::tuple<long, long, long, long>> {};

INSTANTIATE_TEST_SUITE_P(ParametrizedTests,
                         StopwatchStartStopTest,
                         testing::Values(std::make_tuple(-1, 1, 1, 2),
                                         std::make_tuple(1, 2, 0, 1),
                                         std::make_tuple(2, 1, 0, 0),
                                         std::make_tuple(-1, 0, 1, 1),
                                         std::make_tuple(1, 0, 0, 0),
                                         std::make_tuple(0, 1, 0, 1)));

TEST_P(StopwatchStartStopTest, StartAndStopAt) {
  base::subtle::ScopedTimeClockOverrides overrides(
      nullptr, &TimeTicksOverride::Now, nullptr);
  long start_at = std::get<0>(GetParam());
  long stop_at = std::get<1>(GetParam());
  long expected_while_running = std::get<2>(GetParam());
  long expected = std::get<3>(GetParam());
  if (start_at) {
    stopwatch_.StartAt(TimeTicksOverride::now_ticks_ + base::Seconds(start_at));
  } else {
    stopwatch_.Start();
  }
  EXPECT_EQ(base::Seconds(expected_while_running), stopwatch_.TotalElapsed());
  if (stop_at) {
    stopwatch_.StopAt(TimeTicksOverride::now_ticks_ + base::Seconds(stop_at));
  } else {
    stopwatch_.Stop();
  }
  EXPECT_EQ(base::Seconds(expected), stopwatch_.TotalElapsed());
}

class ActionStopwatchTest : public testing::Test {
 protected:
  ActionStopwatch action_stopwatch_;
};

TEST_F(ActionStopwatchTest, SimpleRun) {
  base::subtle::ScopedTimeClockOverrides overrides(
      nullptr, &TimeTicksOverride::Now, nullptr);
  action_stopwatch_.StartActiveTime();
  TimeTicksOverride::now_ticks_ += base::Seconds(1);
  action_stopwatch_.StartWaitTime();
  TimeTicksOverride::now_ticks_ += base::Seconds(2);
  action_stopwatch_.Stop();
  EXPECT_EQ(base::Seconds(1), action_stopwatch_.TotalActiveTime());
  EXPECT_EQ(base::Seconds(2), action_stopwatch_.TotalWaitTime());
}

TEST_F(ActionStopwatchTest, TransferToActive) {
  base::subtle::ScopedTimeClockOverrides overrides(
      nullptr, &TimeTicksOverride::Now, nullptr);
  action_stopwatch_.StartActiveTime();
  TimeTicksOverride::now_ticks_ += base::Seconds(4);
  action_stopwatch_.StartWaitTime();
  TimeTicksOverride::now_ticks_ += base::Seconds(2);
  action_stopwatch_.Stop();
  action_stopwatch_.TransferToActiveTime(base::Seconds(3));
  EXPECT_EQ(base::Seconds(7), action_stopwatch_.TotalActiveTime());
  EXPECT_EQ(base::Seconds(0), action_stopwatch_.TotalWaitTime());
}

TEST_F(ActionStopwatchTest, TransferToWait) {
  base::subtle::ScopedTimeClockOverrides overrides(
      nullptr, &TimeTicksOverride::Now, nullptr);
  action_stopwatch_.StartActiveTime();
  TimeTicksOverride::now_ticks_ += base::Seconds(2);
  action_stopwatch_.StartWaitTime();
  TimeTicksOverride::now_ticks_ += base::Seconds(4);
  action_stopwatch_.Stop();
  action_stopwatch_.TransferToWaitTime(base::Seconds(3));
  EXPECT_EQ(base::Seconds(0), action_stopwatch_.TotalActiveTime());
  EXPECT_EQ(base::Seconds(7), action_stopwatch_.TotalWaitTime());
}

TEST_F(ActionStopwatchTest, StartTimesAt) {
  base::subtle::ScopedTimeClockOverrides overrides(
      nullptr, &TimeTicksOverride::Now, nullptr);
  action_stopwatch_.StartActiveTimeAt(TimeTicksOverride::now_ticks_ +
                                      base::Seconds(1));
  TimeTicksOverride::now_ticks_ += base::Seconds(5);
  action_stopwatch_.StartWaitTimeAt(TimeTicksOverride::now_ticks_ -
                                    base::Seconds(1));
  TimeTicksOverride::now_ticks_ += base::Seconds(2);
  action_stopwatch_.Stop();
  EXPECT_EQ(base::Seconds(3), action_stopwatch_.TotalActiveTime());
  EXPECT_EQ(base::Seconds(3), action_stopwatch_.TotalWaitTime());
}

}  // namespace
}  // namespace autofill_assistant
