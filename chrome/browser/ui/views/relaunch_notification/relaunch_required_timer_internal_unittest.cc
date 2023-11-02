// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/relaunch_notification/relaunch_required_timer_internal.h"

#include "base/time/time.h"
#include "testing/gtest/include/gtest/gtest.h"

TEST(RelaunchRequiredTimerTest, ComputeDeadlineDelta) {
  // A tiny bit over three days: round to three days.
  EXPECT_EQ(relaunch_notification::ComputeDeadlineDelta(base::Days(3) +
                                                        base::Seconds(0.1)),
            base::Days(3));
  // Exactly three days: three days.
  EXPECT_EQ(relaunch_notification::ComputeDeadlineDelta(base::Days(3)),
            base::Days(3));
  // Almost three days: round to three days.
  EXPECT_EQ(relaunch_notification::ComputeDeadlineDelta(base::Days(3) -
                                                        base::Hours(1)),
            base::Days(3));
  // Just shy of two days: round up to two days.
  EXPECT_EQ(relaunch_notification::ComputeDeadlineDelta(base::Days(2) -
                                                        base::Minutes(1)),
            base::Days(2));
  // A bit over 47 hours: round down to 47 hours.
  EXPECT_EQ(relaunch_notification::ComputeDeadlineDelta(base::Days(2) -
                                                        base::Minutes(45)),
            base::Hours(47));
  // Less than one and a half hours: round down to one hour.
  EXPECT_EQ(relaunch_notification::ComputeDeadlineDelta(base::Hours(1) +
                                                        base::Minutes(23)),
            base::Hours(1));
  // Exactly one hour: one hour.
  EXPECT_EQ(relaunch_notification::ComputeDeadlineDelta(base::Hours(1)),
            base::Hours(1));
  // A bit over three minutes: round down to three minutes.
  EXPECT_EQ(relaunch_notification::ComputeDeadlineDelta(base::Minutes(3) +
                                                        base::Seconds(12)),
            base::Minutes(3));
  // Exactly two minutes: two minutes.
  EXPECT_EQ(relaunch_notification::ComputeDeadlineDelta(base::Minutes(2)),
            base::Minutes(2));
  // Nearly one minute: one minute.
  EXPECT_EQ(relaunch_notification::ComputeDeadlineDelta(
                base::Milliseconds(60 * 1000 - 250)),
            base::Minutes(1));
  // Just over two seconds: two seconds.
  EXPECT_EQ(relaunch_notification::ComputeDeadlineDelta(
                base::Milliseconds(2 * 1000 + 250)),
            base::Seconds(2));
  // Exactly one second: one second.
  EXPECT_EQ(relaunch_notification::ComputeDeadlineDelta(base::Seconds(1)),
            base::Seconds(1));
  // Next to nothing: zero.
  EXPECT_EQ(
      relaunch_notification::ComputeDeadlineDelta(base::Milliseconds(250)),
      base::TimeDelta());
}

TEST(RelaunchRequiredTimerTest, ComputeNextRefreshDelta) {
  // Over three days: align to the next smallest day boundary.
  EXPECT_EQ(relaunch_notification::ComputeNextRefreshDelta(base::Days(3) +
                                                           base::Seconds(0.1)),
            base::Days(1) + base::Seconds(0.1));
  // Exactly three days: align to the next smallest day boundary.
  EXPECT_EQ(relaunch_notification::ComputeNextRefreshDelta(base::Days(3)),
            base::Days(1));
  // Almost three days: align to two days.
  EXPECT_EQ(relaunch_notification::ComputeNextRefreshDelta(base::Days(3) -
                                                           base::Hours(1)),
            base::Hours(23));
  // A bit over two days: align to 47 hours.
  EXPECT_EQ(relaunch_notification::ComputeNextRefreshDelta(base::Days(2) +
                                                           base::Hours(1)),
            base::Hours(2));
  // Exactly two days: align to 47 hours.
  EXPECT_EQ(relaunch_notification::ComputeNextRefreshDelta(base::Days(2)),
            base::Hours(1));
  // Less than one and a half hours: align to 59 minutes.
  EXPECT_EQ(relaunch_notification::ComputeNextRefreshDelta(base::Hours(1) +
                                                           base::Minutes(23)),
            base::Minutes(24));
  // Exactly one hour: align to 59 minutes.
  EXPECT_EQ(relaunch_notification::ComputeNextRefreshDelta(base::Hours(1)),
            base::Minutes(1));
  // Between one and two minutes: align to 59 seconds.
  EXPECT_EQ(relaunch_notification::ComputeNextRefreshDelta(base::Minutes(1) +
                                                           base::Seconds(12)),
            base::Seconds(13));
  // Exactly one minute: align to 59 seconds.
  EXPECT_EQ(relaunch_notification::ComputeNextRefreshDelta(base::Minutes(1)),
            base::Seconds(1));
  // One and a half seconds: align to 1 second.
  EXPECT_EQ(relaunch_notification::ComputeNextRefreshDelta(base::Seconds(1.5)),
            base::Seconds(0.5));
  // Just over one second: align to 0 seconds.
  EXPECT_EQ(relaunch_notification::ComputeNextRefreshDelta(base::Seconds(1.1)),
            base::Seconds(1.1));
  // Exactly one second: align to 0 seconds.
  EXPECT_EQ(relaunch_notification::ComputeNextRefreshDelta(base::Seconds(1)),
            base::Seconds(1));
  // Less than one second: align to 0 seconds.
  EXPECT_EQ(relaunch_notification::ComputeNextRefreshDelta(base::Seconds(0.1)),
            base::Seconds(0.1));
}
