// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/signin/public/base/persistent_repeating_timer.h"

#include "base/test/task_environment.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/testing_pref_service.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace signin {

namespace {

const char kLastUpdatedTimePref[] = "test.last_updated_time";
constexpr base::TimeDelta kTestDelay = base::Hours(2);

}  // namespace

class PersistentRepeatingTimerTest : public ::testing::Test {
 public:
  PersistentRepeatingTimerTest() {
    pref_service_.registry()->RegisterTimePref(kLastUpdatedTimePref,
                                               base::Time());
  }

  void RunTask() { ++call_count_; }

  void CheckCallCount(int call_count) {
    base::RunLoop().RunUntilIdle();
    EXPECT_EQ(call_count, call_count_);
  }

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
  CheckCallCount(0);

  // The task is run immediately on start.
  timer.Start();
  CheckCallCount(1);

  task_environment_.FastForwardBy(base::Minutes(1));
  CheckCallCount(1);

  // And after the delay.
  task_environment_.FastForwardBy(kTestDelay);
  CheckCallCount(2);
}

// Checks that spurious calls to Start() have no effect.
TEST_F(PersistentRepeatingTimerTest, MultipleStarts) {
  PersistentRepeatingTimer timer(
      &pref_service_, kLastUpdatedTimePref, kTestDelay,
      base::BindRepeating(&PersistentRepeatingTimerTest::RunTask,
                          base::Unretained(this)));
  CheckCallCount(0);

  // The task is run immediately on start.
  timer.Start();
  CheckCallCount(1);
  timer.Start();
  CheckCallCount(1);

  task_environment_.FastForwardBy(base::Minutes(1));
  CheckCallCount(1);
  task_environment_.FastForwardBy(base::Minutes(1));
  timer.Start();
  CheckCallCount(1);

  // And after the delay.
  task_environment_.FastForwardBy(kTestDelay);
  CheckCallCount(2);
  timer.Start();
  CheckCallCount(2);
}

TEST_F(PersistentRepeatingTimerTest, RecentPref) {
  pref_service_.SetTime(kLastUpdatedTimePref,
                        base::Time::Now() - base::Hours(1));

  PersistentRepeatingTimer timer(
      &pref_service_, kLastUpdatedTimePref, kTestDelay,
      base::BindRepeating(&PersistentRepeatingTimerTest::RunTask,
                          base::Unretained(this)));
  CheckCallCount(0);

  // The task is NOT run immediately on start.
  timer.Start();
  CheckCallCount(0);

  task_environment_.FastForwardBy(base::Minutes(1));
  CheckCallCount(0);

  // It is run after te delay.
  task_environment_.FastForwardBy(base::Hours(1));
  CheckCallCount(1);
  task_environment_.FastForwardBy(base::Hours(1));
  CheckCallCount(1);

  task_environment_.FastForwardBy(base::Hours(1));
  CheckCallCount(2);
}

TEST_F(PersistentRepeatingTimerTest, OldPref) {
  pref_service_.SetTime(kLastUpdatedTimePref,
                        base::Time::Now() - base::Hours(10));

  PersistentRepeatingTimer timer(
      &pref_service_, kLastUpdatedTimePref, kTestDelay,
      base::BindRepeating(&PersistentRepeatingTimerTest::RunTask,
                          base::Unretained(this)));
  CheckCallCount(0);

  // The task is run immediately on start.
  timer.Start();
  CheckCallCount(1);

  task_environment_.FastForwardBy(base::Minutes(1));
  CheckCallCount(1);

  // And after the delay.
  task_environment_.FastForwardBy(kTestDelay);
  CheckCallCount(2);
}

}  // namespace signin
