// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/exo/wayland/wayland_keyboard_delegate.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace exo {
namespace wayland {
namespace {

using WaylandKeyboardDelegateTest = testing::Test;

TEST_F(WaylandKeyboardDelegateTest, RepeatRateIsZeroIfRepeatDisabled) {
  EXPECT_EQ(GetWaylandRepeatRateForTesting(false, base::Seconds(1)), 0);
}

TEST_F(WaylandKeyboardDelegateTest, Converts100MsIntervalTo10Hertz) {
  EXPECT_EQ(GetWaylandRepeatRateForTesting(true, base::Milliseconds(100)), 10);
}

TEST_F(WaylandKeyboardDelegateTest, Converts333MsIntervalTo3Hertz) {
  EXPECT_EQ(GetWaylandRepeatRateForTesting(true, base::Milliseconds(333)), 3);
}

TEST_F(WaylandKeyboardDelegateTest, Converts500MsIntervalTo2Hertz) {
  EXPECT_EQ(GetWaylandRepeatRateForTesting(true, base::Milliseconds(500)), 2);
}

TEST_F(WaylandKeyboardDelegateTest, Converts1SecondIntervalTo1Hertz) {
  EXPECT_EQ(GetWaylandRepeatRateForTesting(true, base::Seconds(1)), 1);
}

TEST_F(WaylandKeyboardDelegateTest,
       Converts2SecondIntervalTo1HertzDueToLimitations) {
  // Should really be 0.5Hz, but Wayland only supports integer repeat rates.
  // Make sure we fallback to 1Hz so some repeating occurs, rather than 0Hz
  // which disables key repeat.
  EXPECT_EQ(GetWaylandRepeatRateForTesting(true, base::Seconds(2)), 1);
}

}  // namespace
}  // namespace wayland
}  // namespace exo
