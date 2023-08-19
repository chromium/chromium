// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/dom_distiller/content/browser/uma_helper.h"

#include <string>

#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace dom_distiller {

class UMAHelperTest : public testing::Test {
 public:
  void FastForwardBy(int milliseconds) {
    task_environment_.FastForwardBy(base::Milliseconds(milliseconds));
  }

 protected:
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
};

TEST_F(UMAHelperTest, TestTimerBasics) {
  UMAHelper::DistillabilityDriverTimer timer;
  ASSERT_FALSE(timer.HasStarted());
  timer.Start(false);
  ASSERT_TRUE(timer.HasStarted());

  FastForwardBy(100);
  ASSERT_EQ(timer.GetElapsedTime().InMilliseconds(), 100);
  FastForwardBy(100);
  ASSERT_EQ(timer.GetElapsedTime().InMilliseconds(), 200);

  // After pausing, the timer should still be running (active), and the
  // value should be unchanged.
  timer.Pause();
  ASSERT_TRUE(timer.HasStarted());
  ASSERT_EQ(timer.GetElapsedTime().InMilliseconds(), 200);
  // Paused timer shouldn't increase value when time changes.
  FastForwardBy(100);
  ASSERT_EQ(timer.GetElapsedTime().InMilliseconds(), 200);

  // Starting the timer again will cause it to move forward again as time
  // changes.
  timer.Start(false);
  ASSERT_TRUE(timer.HasStarted());
  ASSERT_EQ(timer.GetElapsedTime().InMilliseconds(), 200);
  FastForwardBy(100);
  ASSERT_EQ(timer.GetElapsedTime().InMilliseconds(), 300);

  // Pause again, but this time continue with Resume instead of Start. This
  // should have the same effect.
  timer.Pause();
  FastForwardBy(100);
  ASSERT_EQ(timer.GetElapsedTime().InMilliseconds(), 300);
  timer.Resume();
  FastForwardBy(100);
  ASSERT_EQ(timer.GetElapsedTime().InMilliseconds(), 400);

  // Calling start or pause multiple times in a row does not break anything.
  timer.Start(false);
  ASSERT_EQ(timer.GetElapsedTime().InMilliseconds(), 400);
  FastForwardBy(100);
  timer.Start(false);
  timer.Start(false);
  timer.Resume();
  ASSERT_EQ(timer.GetElapsedTime().InMilliseconds(), 500);
  timer.Pause();
  timer.Pause();
  ASSERT_EQ(timer.GetElapsedTime().InMilliseconds(), 500);

  // Reset the timer.
  timer.Reset();
  ASSERT_FALSE(timer.HasStarted());
}

TEST_F(UMAHelperTest, TestTimerForDistilledPage) {
  UMAHelper::DistillabilityDriverTimer timer;

  timer.Start(true);
  ASSERT_TRUE(timer.HasStarted());
  FastForwardBy(100);
  ASSERT_EQ(timer.GetElapsedTime().InMilliseconds(), 100);
  timer.Start(true);
  ASSERT_EQ(timer.GetElapsedTime().InMilliseconds(), 100);
}

}  // namespace dom_distiller
