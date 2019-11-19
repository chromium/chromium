// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/relaunch_notification/wall_clock_timer.h"

#include <memory>
#include <utility>

#include "base/macros.h"
#include "base/power_monitor/power_monitor.h"
#include "base/power_monitor/power_monitor_source.h"
#include "base/test/mock_callback.h"
#include "base/test/simple_test_clock.h"
#include "base/test/task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

class StubPowerMonitorSource : public base::PowerMonitorSource {
 public:
  // Use this method to send a power resume event.
  void Resume() { ProcessPowerEvent(RESUME_EVENT); }

  // Use this method to send a power suspend event.
  void Suspend() { ProcessPowerEvent(SUSPEND_EVENT); }

  // base::PowerMonitorSource:
  bool IsOnBatteryPowerImpl() override { return false; }
};

}  // namespace

class WallClockTimerTest : public ::testing::Test {
 protected:
  WallClockTimerTest()
      : task_environment_(base::test::TaskEnvironment::TimeSource::MOCK_TIME) {
    auto mock_power_monitor_source = std::make_unique<StubPowerMonitorSource>();
    mock_power_monitor_source_ = mock_power_monitor_source.get();
    base::PowerMonitor::Initialize(std::move(mock_power_monitor_source));
  }

  ~WallClockTimerTest() override { base::PowerMonitor::ShutdownForTesting(); }

  // Owned by power_monitor_. Use this to simulate a power suspend and resume.
  StubPowerMonitorSource* mock_power_monitor_source_ = nullptr;
  base::test::TaskEnvironment task_environment_;

 private:
  DISALLOW_COPY_AND_ASSIGN(WallClockTimerTest);
};

TEST_F(WallClockTimerTest, PowerResume) {
  ::testing::StrictMock<base::MockCallback<base::OnceClosure>> callback;
  base::SimpleTestClock clock;
  // Set up a WallClockTimer that will fire in one minute.
  WallClockTimer wall_clock_timer(&clock, task_environment_.GetMockTickClock());
  const auto delay = base::TimeDelta::FromMinutes(1);
  const auto start_time = base::Time::Now();
  const auto run_time = start_time + delay;
  clock.SetNow(start_time);
  wall_clock_timer.Start(FROM_HERE, run_time, callback.Get());
  EXPECT_EQ(wall_clock_timer.desired_run_time(), start_time + delay);

  mock_power_monitor_source_->Suspend();
  // Pretend that time jumps forward 30 seconds while the machine is suspended.
  auto past_time = base::TimeDelta::FromSeconds(30);
  clock.SetNow(start_time + past_time);
  mock_power_monitor_source_->Resume();
  task_environment_.RunUntilIdle();
  // Ensure that the timer has not yet fired.
  ::testing::Mock::VerifyAndClearExpectations(&callback);
  EXPECT_EQ(wall_clock_timer.desired_run_time(), start_time + delay);

  // Expect that the timer fires at the desired run time.
  EXPECT_CALL(callback, Run());
  // Both Time::Now() and |task_environment_| MockTickClock::Now()
  // go forward by (|delay| - |past_time|):
  clock.SetNow(start_time + delay);
  task_environment_.FastForwardBy(delay - past_time);
  ::testing::Mock::VerifyAndClearExpectations(&callback);
  EXPECT_FALSE(wall_clock_timer.IsRunning());
}
