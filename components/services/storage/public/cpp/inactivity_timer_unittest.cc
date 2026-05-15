// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/services/storage/public/cpp/inactivity_timer.h"

#include "base/functional/callback_helpers.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace storage {
namespace {

class InactivityTimerTest : public testing::Test {
 protected:
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
};

TEST_F(InactivityTimerTest, FiresAndStopsAfterDelay) {
  bool fired = false;
  InactivityTimer timer;
  timer.Start(FROM_HERE, base::Seconds(30),
              base::BindRepeating([](bool* fired) { *fired = true; },
                                  base::Unretained(&fired)));
  EXPECT_TRUE(timer.IsRunning());

  task_environment_.FastForwardBy(base::Seconds(10));
  EXPECT_FALSE(fired);

  task_environment_.FastForwardBy(base::Seconds(10));
  EXPECT_FALSE(fired);

  task_environment_.FastForwardBy(base::Seconds(10));
  EXPECT_TRUE(fired);
  EXPECT_FALSE(timer.IsRunning());

  fired = false;
  task_environment_.FastForwardBy(base::Seconds(100));
  EXPECT_FALSE(fired);
}

TEST_F(InactivityTimerTest, ResetRestartsCountdown) {
  bool fired = false;
  InactivityTimer timer(FROM_HERE, base::Seconds(30),
                        base::BindRepeating([](bool* fired) { *fired = true; },
                                            base::Unretained(&fired)));

  // Simply constructing the timer shouldn't start it.
  EXPECT_FALSE(timer.IsRunning());
  task_environment_.FastForwardBy(base::Seconds(100));
  EXPECT_FALSE(fired);

  // Start and advance partway through.
  timer.Reset();
  EXPECT_TRUE(timer.IsRunning());
  task_environment_.FastForwardBy(base::Seconds(20));

  // Reset and expect the full delay again.
  timer.Reset();
  task_environment_.FastForwardBy(base::Seconds(20));
  EXPECT_FALSE(fired);
  task_environment_.FastForwardBy(base::Seconds(10));
  EXPECT_TRUE(fired);
}

TEST_F(InactivityTimerTest, StopAndRestart) {
  bool fired = false;
  InactivityTimer timer;
  timer.Start(FROM_HERE, base::Seconds(30),
              base::BindRepeating([](bool* fired) { *fired = true; },
                                  base::Unretained(&fired)));

  // Stop partway through.
  task_environment_.FastForwardBy(base::Seconds(20));
  timer.Stop();
  EXPECT_FALSE(timer.IsRunning());

  // Restart with a different delay.
  timer.Start(FROM_HERE, base::Seconds(9),
              base::BindRepeating([](bool* fired) { *fired = true; },
                                  base::Unretained(&fired)));
  task_environment_.FastForwardBy(base::Seconds(9));
  EXPECT_TRUE(fired);
}

TEST_F(InactivityTimerTest, ResetFromWithinAction) {
  int fire_count = 0;
  InactivityTimer timer;
  timer.Start(FROM_HERE, base::Seconds(30),
              base::BindRepeating(
                  [](InactivityTimer* timer, int* count) {
                    ++(*count);
                    EXPECT_FALSE(timer->IsRunning());
                    timer->Reset();
                  },
                  base::Unretained(&timer), base::Unretained(&fire_count)));

  task_environment_.FastForwardBy(base::Seconds(30));
  EXPECT_EQ(1, fire_count);

  // Fires again because the action restarted the timer.
  task_environment_.FastForwardBy(base::Seconds(30));
  EXPECT_EQ(2, fire_count);
}

TEST_F(InactivityTimerTest, ExpectedFiringTime) {
  InactivityTimer timer;
  timer.Start(FROM_HERE, base::Seconds(30), base::DoNothing());
  EXPECT_EQ(base::TimeTicks::Now() + base::Seconds(30),
            timer.ExpectedFiringTimeForTesting());

  task_environment_.FastForwardBy(base::Seconds(5));
  EXPECT_EQ(base::TimeTicks::Now() + base::Seconds(25),
            timer.ExpectedFiringTimeForTesting());

  task_environment_.FastForwardBy(base::Seconds(10));
  EXPECT_EQ(base::TimeTicks::Now() + base::Seconds(15),
            timer.ExpectedFiringTimeForTesting());

  task_environment_.FastForwardBy(base::Seconds(10));
  EXPECT_EQ(base::TimeTicks::Now() + base::Seconds(5),
            timer.ExpectedFiringTimeForTesting());

  task_environment_.FastForwardBy(base::Seconds(5));
  EXPECT_FALSE(timer.IsRunning());
}

}  // namespace
}  // namespace storage
