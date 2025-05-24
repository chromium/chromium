// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/signin/public/base/persistent_repeating_timer.h"

#include "base/feature_list.h"
#include "base/test/bind.h"
#include "base/test/power_monitor_test.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/test/test_mock_time_task_runner.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/testing_pref_service.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace signin {

namespace {

constexpr char kLastUpdatedTimePref[] = "test.last_updated_time";
constexpr base::TimeDelta kTestDelay = base::Hours(2);

}  // namespace

class PersistentRepeatingTimerTest : public ::testing::Test {
 public:
  PersistentRepeatingTimerTest() {
    pref_service_.registry()->RegisterTimePref(kLastUpdatedTimePref,
                                               base::Time());
  }

  void RunTask() { ++call_count_; }

  base::test::ScopedFeatureList feature_list_;
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  TestingPrefServiceSimple pref_service_;
  int call_count_ = 0;
};

// Checks that the missing pref is treated like an old one.
TEST_F(PersistentRepeatingTimerTest, MissingPref) {
  PersistentRepeatingTimer timer(
      &pref_service_, kLastUpdatedTimePref, kTestDelay,
      base::BindRepeating(&PersistentRepeatingTimerTest::RunTask,
                          base::Unretained(this)));
  EXPECT_EQ(0, call_count_);

  // The task is run immediately on start.
  timer.Start();
  EXPECT_EQ(1, call_count_);

  task_environment_.FastForwardBy(base::Minutes(1));
  EXPECT_EQ(1, call_count_);

  // And after the delay.
  task_environment_.FastForwardBy(kTestDelay);
  EXPECT_EQ(2, call_count_);
}

// Checks that spurious calls to Start() have no effect.
TEST_F(PersistentRepeatingTimerTest, MultipleStarts) {
  PersistentRepeatingTimer timer(
      &pref_service_, kLastUpdatedTimePref, kTestDelay,
      base::BindRepeating(&PersistentRepeatingTimerTest::RunTask,
                          base::Unretained(this)));
  EXPECT_EQ(0, call_count_);

  // The task is run immediately on start.
  timer.Start();
  EXPECT_EQ(1, call_count_);
  timer.Start();
  EXPECT_EQ(1, call_count_);

  task_environment_.FastForwardBy(base::Minutes(1));
  EXPECT_EQ(1, call_count_);
  task_environment_.FastForwardBy(base::Minutes(1));
  timer.Start();
  EXPECT_EQ(1, call_count_);

  // And after the delay.
  task_environment_.FastForwardBy(kTestDelay);
  EXPECT_EQ(2, call_count_);
  timer.Start();
  EXPECT_EQ(2, call_count_);
}

TEST_F(PersistentRepeatingTimerTest, RecentPref) {
  pref_service_.SetTime(kLastUpdatedTimePref,
                        base::Time::Now() - base::Hours(1));

  PersistentRepeatingTimer timer(
      &pref_service_, kLastUpdatedTimePref, kTestDelay,
      base::BindRepeating(&PersistentRepeatingTimerTest::RunTask,
                          base::Unretained(this)));
  EXPECT_EQ(0, call_count_);

  // The task is NOT run immediately on start.
  timer.Start();
  EXPECT_EQ(0, call_count_);

  task_environment_.FastForwardBy(base::Minutes(1));
  EXPECT_EQ(0, call_count_);

  // It is run after the delay.
  task_environment_.FastForwardBy(base::Hours(1));
  EXPECT_EQ(1, call_count_);
  task_environment_.FastForwardBy(base::Hours(1));
  EXPECT_EQ(1, call_count_);

  task_environment_.FastForwardBy(base::Hours(1));
  EXPECT_EQ(2, call_count_);
}

TEST_F(PersistentRepeatingTimerTest, OldPref) {
  pref_service_.SetTime(kLastUpdatedTimePref,
                        base::Time::Now() - base::Hours(10));

  PersistentRepeatingTimer timer(
      &pref_service_, kLastUpdatedTimePref, kTestDelay,
      base::BindRepeating(&PersistentRepeatingTimerTest::RunTask,
                          base::Unretained(this)));
  EXPECT_EQ(0, call_count_);

  // The task is run immediately on start.
  timer.Start();
  EXPECT_EQ(1, call_count_);

  task_environment_.FastForwardBy(base::Minutes(1));
  EXPECT_EQ(1, call_count_);

  // And after the delay.
  task_environment_.FastForwardBy(kTestDelay);
  EXPECT_EQ(2, call_count_);
}

// Note: This test can't use base::test::TaskEnvironment (and thus doesn't use
// the fixture) because TaskEnvironment doesn't allow advancing base::Time
// without advancing base::TimeTicks, which is what's needed to simulate
// suspend/resume.
TEST(PersistentRepeatingTimerStandaloneTest, SuspendAndResume) {
  base::test::ScopedPowerMonitorTestSource fake_power_monitor_source;
  scoped_refptr<base::TestMockTimeTaskRunner> task_runner =
      base::MakeRefCounted<base::TestMockTimeTaskRunner>(
          base::TestMockTimeTaskRunner::Type::kBoundToThread);
  TestingPrefServiceSimple pref_service;
  pref_service.registry()->RegisterTimePref(kLastUpdatedTimePref, base::Time());
  pref_service.SetTime(kLastUpdatedTimePref,
                       task_runner->Now() - base::Hours(1));

  int call_count = 0;
  PersistentRepeatingTimer timer(
      &pref_service, kLastUpdatedTimePref, kTestDelay,
      base::BindLambdaForTesting([&]() { ++call_count; }),
      task_runner->GetMockClock(), task_runner->GetMockTickClock());

  // The timer gets started, and runs for a little while before the device gets
  // suspended.
  timer.Start();
  task_runner->FastForwardBy(base::Minutes(1));
  ASSERT_EQ(0, call_count);

  // Simulate that the device gets suspended across the trigger time, then woken
  // up again. The task should get run immediately after the wakeup.
  fake_power_monitor_source.Suspend();
  task_runner->AdvanceWallClock(base::Hours(1));
  fake_power_monitor_source.Resume();
  // The timer should now be scheduled to run with a zero delay. Poke the task
  // runner to actually run it. Note that this won't run tasks with a non-zero
  // delay, and will not advance time.
  task_runner->RunUntilIdle();
  EXPECT_EQ(1, call_count);
}

}  // namespace signin
