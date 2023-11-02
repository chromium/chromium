// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/common/frame_sinks/delay_based_time_source.h"

#include <stdint.h>

#include "base/test/simple_test_tick_clock.h"
#include "base/test/test_mock_time_task_runner.h"
#include "components/viz/test/fake_delay_based_time_source.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace viz {
namespace {

base::TimeDelta Interval() {
  return base::Microseconds(base::Time::kMicrosecondsPerSecond / 60);
}

class DelayBasedTimeSourceTest : public ::testing::Test {
 protected:
  void SetUp() override {
    task_runner_ = base::MakeRefCounted<base::TestMockTimeTaskRunner>();
    delay_based_time_source_ = std::make_unique<FakeDelayBasedTimeSource>(
        task_runner_->GetMockTickClock(), task_runner_.get());
    delay_based_time_source_->SetClient(&client_);
  }

  void TearDown() override {
    delay_based_time_source_.reset();
    task_runner_ = nullptr;
  }

  base::TestMockTimeTaskRunner* task_runner() { return task_runner_.get(); }

  FakeDelayBasedTimeSource* timer() { return delay_based_time_source_.get(); }

  FakeDelayBasedTimeSourceClient* client() { return &client_; }

  FakeDelayBasedTimeSourceClient client_;
  scoped_refptr<base::TestMockTimeTaskRunner> task_runner_;
  std::unique_ptr<FakeDelayBasedTimeSource> delay_based_time_source_;
};

TEST_F(DelayBasedTimeSourceTest, TaskPostedAndTickCalled) {
  timer()->SetTimebaseAndInterval(base::TimeTicks(), Interval());
  timer()->SetActive(true);
  EXPECT_TRUE(timer()->Active());
  EXPECT_TRUE(task_runner()->HasPendingTask());

  task_runner()->AdvanceMockTickClock(Interval());
  task_runner()->RunUntilIdle();
  EXPECT_TRUE(timer()->Active());
  EXPECT_TRUE(client()->TickCalled());
}

TEST_F(DelayBasedTimeSourceTest, TickNotCalledWithTaskPosted) {
  timer()->SetTimebaseAndInterval(base::TimeTicks(), Interval());
  timer()->SetActive(true);
  EXPECT_TRUE(task_runner()->HasPendingTask());
  timer()->SetActive(false);
  task_runner()->RunUntilIdle();
  EXPECT_FALSE(client()->TickCalled());
}

TEST_F(DelayBasedTimeSourceTest, StartTwiceEnqueuesOneTask) {
  timer()->SetTimebaseAndInterval(base::TimeTicks(), Interval());
  timer()->SetActive(true);
  EXPECT_TRUE(task_runner()->HasPendingTask());
  task_runner()->ClearPendingTasks();
  timer()->SetActive(true);
  EXPECT_FALSE(task_runner()->HasPendingTask());
}

TEST_F(DelayBasedTimeSourceTest, StartWhenRunningDoesntTick) {
  timer()->SetTimebaseAndInterval(base::TimeTicks(), Interval());
  timer()->SetActive(true);
  EXPECT_TRUE(task_runner()->HasPendingTask());
  task_runner()->RunUntilIdle();
  task_runner()->ClearPendingTasks();
  timer()->SetActive(true);
  EXPECT_FALSE(task_runner()->HasPendingTask());
}

// At 60Hz, when the tick returns at exactly the requested next time, make sure
// a 16ms next delay is posted.
TEST_F(DelayBasedTimeSourceTest, NextDelaySaneWhenExactlyOnRequestedTime) {
  timer()->SetTimebaseAndInterval(base::TimeTicks(), Interval());
  timer()->SetActive(true);
  // Run the first tick.
  task_runner()->RunUntilIdle();

  EXPECT_EQ(16, task_runner()->NextPendingTaskDelay().InMilliseconds());

  task_runner()->AdvanceMockTickClock(Interval());
  task_runner()->RunUntilIdle();

  EXPECT_EQ(16, task_runner()->NextPendingTaskDelay().InMilliseconds());
}

// At 60Hz, when the tick returns at slightly after the requested next time,
// make sure a 16ms next delay is posted.
TEST_F(DelayBasedTimeSourceTest, NextDelaySaneWhenSlightlyAfterRequestedTime) {
  timer()->SetTimebaseAndInterval(base::TimeTicks(), Interval());
  timer()->SetActive(true);
  // Run the first tick.
  task_runner()->RunUntilIdle();

  EXPECT_EQ(16, task_runner()->NextPendingTaskDelay().InMilliseconds());

  task_runner()->AdvanceMockTickClock(Interval() + base::Microseconds(1));
  task_runner()->RunUntilIdle();

  EXPECT_EQ(16, task_runner()->NextPendingTaskDelay().InMilliseconds());
}

// At 60Hz, when the tick returns at exactly 2*interval after the requested next
// time, make sure we don't tick unnecessarily.
TEST_F(DelayBasedTimeSourceTest,
       NextDelaySaneWhenExactlyTwiceAfterRequestedTime) {
  timer()->SetTimebaseAndInterval(base::TimeTicks(), Interval());
  timer()->SetActive(true);
  // Run the first tick.
  task_runner()->RunUntilIdle();

  EXPECT_EQ(16, task_runner()->NextPendingTaskDelay().InMilliseconds());

  task_runner()->AdvanceMockTickClock(2 * Interval());
  task_runner()->RunUntilIdle();

  EXPECT_EQ(16, task_runner()->NextPendingTaskDelay().InMilliseconds());
}

// At 60Hz, when the tick returns at 2*interval and a bit after the requested
// next time, make sure a 16ms next delay is posted.
TEST_F(DelayBasedTimeSourceTest,
       NextDelaySaneWhenSlightlyAfterTwiceRequestedTime) {
  timer()->SetTimebaseAndInterval(base::TimeTicks(), Interval());
  timer()->SetActive(true);
  // Run the first tick.
  task_runner()->RunUntilIdle();

  EXPECT_EQ(16, task_runner()->NextPendingTaskDelay().InMilliseconds());

  task_runner()->AdvanceMockTickClock(2 * Interval() + base::Microseconds(1));
  task_runner()->RunUntilIdle();

  EXPECT_EQ(16, task_runner()->NextPendingTaskDelay().InMilliseconds());
}

// At 60Hz, when the tick returns halfway to the next frame time, make sure
// a correct next delay value is posted.
TEST_F(DelayBasedTimeSourceTest, NextDelaySaneWhenHalfAfterRequestedTime) {
  timer()->SetTimebaseAndInterval(base::TimeTicks(), Interval());
  timer()->SetActive(true);
  // Run the first tick.
  task_runner()->RunUntilIdle();

  EXPECT_EQ(16, task_runner()->NextPendingTaskDelay().InMilliseconds());

  task_runner()->AdvanceMockTickClock(Interval() + base::Milliseconds(8));
  task_runner()->RunUntilIdle();

  EXPECT_EQ(8, task_runner()->NextPendingTaskDelay().InMilliseconds());
}

TEST_F(DelayBasedTimeSourceTest, JitteryRuntimeWithFutureTimebases) {
  timer()->SetTimebaseAndInterval(base::TimeTicks(), Interval());
  timer()->SetActive(true);

  // Run the first tick.
  task_runner()->RunUntilIdle();
  EXPECT_EQ(16, task_runner()->NextPendingTaskDelay().InMilliseconds());

  base::TimeTicks future_timebase = timer()->Now() + Interval() * 10;

  // 1ms jitter
  base::TimeDelta jitter1 = base::Milliseconds(1);

  // Tick with +1ms of jitter
  future_timebase += Interval();
  timer()->SetTimebaseAndInterval(future_timebase, Interval());
  task_runner()->AdvanceMockTickClock(Interval() + jitter1);
  task_runner()->RunUntilIdle();
  EXPECT_EQ(15, task_runner()->NextPendingTaskDelay().InMilliseconds());

  // Tick with 0ms of jitter
  future_timebase += Interval();
  timer()->SetTimebaseAndInterval(future_timebase, Interval());
  task_runner()->AdvanceMockTickClock(Interval() - jitter1);
  task_runner()->RunUntilIdle();
  EXPECT_EQ(16, task_runner()->NextPendingTaskDelay().InMilliseconds());

  // Tick with -1ms of jitter
  future_timebase += Interval();
  timer()->SetTimebaseAndInterval(future_timebase, Interval());
  task_runner()->AdvanceMockTickClock(Interval() - jitter1);
  task_runner()->RunUntilIdle();
  EXPECT_EQ(1, task_runner()->NextPendingTaskDelay().InMilliseconds());

  // Tick with 0ms of jitter
  future_timebase += Interval();
  timer()->SetTimebaseAndInterval(future_timebase, Interval());
  task_runner()->AdvanceMockTickClock(Interval() + jitter1);
  task_runner()->RunUntilIdle();
  EXPECT_EQ(16, task_runner()->NextPendingTaskDelay().InMilliseconds());

  // 8 ms jitter
  base::TimeDelta jitter8 = base::Milliseconds(8);

  // Tick with +8ms of jitter
  future_timebase += Interval();
  timer()->SetTimebaseAndInterval(future_timebase, Interval());
  task_runner()->AdvanceMockTickClock(Interval() + jitter8);
  task_runner()->RunUntilIdle();
  EXPECT_EQ(8, task_runner()->NextPendingTaskDelay().InMilliseconds());

  // Tick with 0ms of jitter
  future_timebase += Interval();
  timer()->SetTimebaseAndInterval(future_timebase, Interval());
  task_runner()->AdvanceMockTickClock(Interval() - jitter8);
  task_runner()->RunUntilIdle();
  EXPECT_EQ(16, task_runner()->NextPendingTaskDelay().InMilliseconds());

  // Tick with -8ms of jitter
  future_timebase += Interval();
  timer()->SetTimebaseAndInterval(future_timebase, Interval());
  task_runner()->AdvanceMockTickClock(Interval() - jitter8);
  task_runner()->RunUntilIdle();
  EXPECT_EQ(8, task_runner()->NextPendingTaskDelay().InMilliseconds());

  // Tick with 0ms of jitter
  future_timebase += Interval();
  timer()->SetTimebaseAndInterval(future_timebase, Interval());
  task_runner()->AdvanceMockTickClock(Interval() + jitter8);
  task_runner()->RunUntilIdle();
  EXPECT_EQ(16, task_runner()->NextPendingTaskDelay().InMilliseconds());

  // 15 ms jitter
  base::TimeDelta jitter15 = base::Milliseconds(15);

  // Tick with +15ms jitter
  future_timebase += Interval();
  timer()->SetTimebaseAndInterval(future_timebase, Interval());
  task_runner()->AdvanceMockTickClock(Interval() + jitter15);
  task_runner()->RunUntilIdle();
  EXPECT_EQ(1, task_runner()->NextPendingTaskDelay().InMilliseconds());

  // Tick with 0ms of jitter
  future_timebase += Interval();
  timer()->SetTimebaseAndInterval(future_timebase, Interval());
  task_runner()->AdvanceMockTickClock(Interval() - jitter15);
  task_runner()->RunUntilIdle();
  EXPECT_EQ(16, task_runner()->NextPendingTaskDelay().InMilliseconds());

  // Tick with -15ms of jitter
  future_timebase += Interval();
  timer()->SetTimebaseAndInterval(future_timebase, Interval());
  task_runner()->AdvanceMockTickClock(Interval() - jitter15);
  task_runner()->RunUntilIdle();
  EXPECT_EQ(15, task_runner()->NextPendingTaskDelay().InMilliseconds());

  // Tick with 0ms of jitter
  future_timebase += Interval();
  timer()->SetTimebaseAndInterval(future_timebase, Interval());
  task_runner()->AdvanceMockTickClock(Interval() + jitter15);
  task_runner()->RunUntilIdle();
  EXPECT_EQ(16, task_runner()->NextPendingTaskDelay().InMilliseconds());
}

TEST_F(DelayBasedTimeSourceTest, AchievesTargetRateWithNoNoise) {
  int num_iterations = 10;

  timer()->SetTimebaseAndInterval(base::TimeTicks(), Interval());
  timer()->SetActive(true);

  double total_frame_time = 0.0;
  for (int i = 0; i < num_iterations; ++i) {
    int64_t delay_ms = task_runner()->NextPendingTaskDelay().InMilliseconds();

    // accumulate the "delay"
    total_frame_time += delay_ms / 1000.0;

    // Run the callback exactly when asked
    task_runner()->AdvanceMockTickClock(base::Milliseconds(delay_ms));
    task_runner()->RunUntilIdle();
  }
  double average_interval =
      total_frame_time / static_cast<double>(num_iterations);
  EXPECT_NEAR(1.0 / 60.0, average_interval, 0.1);
}

TEST_F(DelayBasedTimeSourceTest, TestDeactivateWhilePending) {
  timer()->SetTimebaseAndInterval(base::TimeTicks(), Interval());
  timer()->SetActive(true);  // Should post a task.
  timer()->SetActive(false);
  // Should run the posted task without crashing.
  EXPECT_FALSE(task_runner()->HasPendingTask());
  task_runner()->RunUntilIdle();
}

TEST_F(DelayBasedTimeSourceTest,
       TestDeactivateAndReactivateBeforeNextTickTime) {
  timer()->SetTimebaseAndInterval(base::TimeTicks(), Interval());

  // Should run the activate task, and pick up a new timebase.
  timer()->SetActive(true);
  task_runner()->RunUntilIdle();

  // Stop the timer()
  timer()->SetActive(false);

  // Task will be pending anyway, run it
  task_runner()->RunUntilIdle();

  // Start the timer() again, but before the next tick time the timer()
  // previously planned on using. That same tick time should still be targeted.
  task_runner()->AdvanceMockTickClock(base::Milliseconds(4));
  timer()->SetActive(true);
  EXPECT_EQ(12, task_runner()->NextPendingTaskDelay().InMilliseconds());
}

TEST_F(DelayBasedTimeSourceTest, TestDeactivateAndReactivateAfterNextTickTime) {
  timer()->SetTimebaseAndInterval(base::TimeTicks(), Interval());

  // Should run the activate task, and pick up a new timebase.
  timer()->SetActive(true);
  task_runner()->RunUntilIdle();

  // Stop the timer().
  timer()->SetActive(false);

  // Task will be pending anyway, run it.
  task_runner()->RunUntilIdle();

  // Start the timer() again, but before the next tick time the timer()
  // previously planned on using. That same tick time should still be targeted.
  task_runner()->AdvanceMockTickClock(base::Milliseconds(20));
  timer()->SetActive(true);
  EXPECT_EQ(13, task_runner()->NextPendingTaskDelay().InMilliseconds());
}

}  // namespace
}  // namespace viz
