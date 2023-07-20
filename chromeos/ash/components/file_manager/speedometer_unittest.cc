// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/file_manager/speedometer.h"

#include <cmath>
#include <limits>

#include "base/test/scoped_mock_clock_override.h"
#include "base/time/time.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace file_manager {
namespace {

using base::Milliseconds;
using base::Seconds;
using testing::IsNan;

TEST(SpeedometerTest, RemainingTime) {
  base::ScopedMockClockOverride clock;
  Speedometer meter;

  // Testing without setting the total bytes:
  EXPECT_EQ(meter.GetSampleCount(), 0u);
  EXPECT_TRUE(meter.GetRemainingTime().is_max());

  // Sets total number of bytes.
  meter.SetTotalBytes(2000);
  EXPECT_EQ(meter.GetSampleCount(), 0u);
  EXPECT_TRUE(meter.GetRemainingTime().is_max());

  // 1st sample.
  // 1st sample, but not enough to calculate the remaining time.
  clock.Advance(Seconds(11));

  EXPECT_TRUE(meter.Update(100));
  EXPECT_EQ(meter.GetSampleCount(), 1u);
  EXPECT_TRUE(meter.GetRemainingTime().is_max());

  // Sample received less than 3 second after the previous one should be
  // ignored.
  clock.Advance(Milliseconds(2999));
  EXPECT_FALSE(meter.Update(300));
  EXPECT_EQ(meter.GetSampleCount(), 1u);
  EXPECT_TRUE(meter.GetRemainingTime().is_max());

  // 2nd sample, the remaining time can be computed.
  clock.Advance(Milliseconds(1));
  EXPECT_TRUE(meter.Update(300));
  EXPECT_EQ(meter.GetSampleCount(), 2u);
  EXPECT_EQ(meter.GetRemainingTime().InSeconds(), 25);

  // 3rd sample. +3 seconds and still only processed 300 bytes.
  clock.Advance(Seconds(3));
  EXPECT_TRUE(meter.Update(300));
  EXPECT_EQ(meter.GetSampleCount(), 3u);
  EXPECT_EQ(meter.GetRemainingTime().InSeconds(), 50);

  // 4th sample, +5 seconds and still only 300 bytes.
  clock.Advance(Seconds(5));
  EXPECT_TRUE(meter.Update(300));
  EXPECT_EQ(meter.GetSampleCount(), 4u);
  EXPECT_EQ(meter.GetRemainingTime().InSeconds(), 109);

  // 5th sample, +3 seconds and now bumped from 300 to 600 bytes.
  clock.Advance(Seconds(3));
  EXPECT_TRUE(meter.Update(600));
  EXPECT_EQ(meter.GetSampleCount(), 5u);
  EXPECT_EQ(meter.GetRemainingTime().InSeconds(), 55);

  // Elapsed time should impact the remaining time.
  clock.Advance(Seconds(12));
  EXPECT_EQ(meter.GetRemainingTime().InSeconds(), 43);

  // GetRemainingTime() can return negative value.
  clock.Advance(Seconds(60));
  EXPECT_EQ(meter.GetRemainingTime().InSeconds(), -16);
}

TEST(SpeedometerTest, Samples) {
  base::ScopedMockClockOverride clock;
  Speedometer meter;

  constexpr size_t kMaxSamples = 20;
  meter.SetTotalBytes(20000);

  // Slow speed of 100 bytes per second.
  int total_transferred = 0;
  for (size_t i = 0; i < kMaxSamples; i++) {
    EXPECT_EQ(meter.GetSampleCount(), i);
    clock.Advance(Seconds(3));
    total_transferred = i * 100;
    EXPECT_TRUE(meter.Update(total_transferred));
  }

  EXPECT_EQ(meter.GetSampleCount(), kMaxSamples);
  EXPECT_EQ(meter.GetRemainingTime().InSeconds(), 543);

  // +200 to make it compatible with the values in the unittest in the JS
  // version.
  const int initial_transferred_bytes = total_transferred + 200;
  // Faster speed of 300 bytes per second.
  for (size_t i = 0; i < kMaxSamples; i++) {
    // Check buffer not expanded more than the specified length.
    EXPECT_EQ(meter.GetSampleCount(), kMaxSamples);
    clock.Advance(Seconds(3));
    total_transferred = initial_transferred_bytes + (i * 300);
    EXPECT_TRUE(meter.Update(total_transferred));

    // Current speed should be seen as accelerating, thus the remaining time
    // decreasing.
    EXPECT_LT(meter.GetRemainingTime().InSeconds(), 543);
  }

  EXPECT_EQ(meter.GetSampleCount(), kMaxSamples);
  EXPECT_EQ(meter.GetRemainingTime().InSeconds(), 122);

  // Stalling.
  for (size_t i = 0; i < kMaxSamples; i++) {
    // Check buffer not expanded more than the specified length.
    EXPECT_EQ(meter.GetSampleCount(), kMaxSamples);
    clock.Advance(Seconds(3));
    EXPECT_TRUE(meter.Update(total_transferred));
  }

  // When all samples have the same value the remaining time goes to infinity,
  // because the Linear Interpolation expects an inclination/slope, but with all
  // values the same, it becomes a horizontal line, meaning that the bytes will
  // never grow towards the total bytes.
  EXPECT_TRUE(meter.GetRemainingTime().is_max());
}

TEST(SpeedometerTest, ProgressGoingBackwards) {
  base::ScopedMockClockOverride clock;
  Speedometer meter;

  // Sets total number of bytes and a couple of samples.
  meter.SetTotalBytes(2000);
  EXPECT_TRUE(meter.Update(100));
  clock.Advance(Seconds(5));
  EXPECT_TRUE(meter.Update(300));
  EXPECT_EQ(meter.GetSampleCount(), 2u);
  EXPECT_EQ(meter.GetRemainingTime().InSeconds(), 42);

  // Progress going backwards should clear the samples.
  clock.Advance(Seconds(3));
  EXPECT_TRUE(meter.Update(200));
  EXPECT_EQ(meter.GetSampleCount(), 1u);
  EXPECT_TRUE(meter.GetRemainingTime().is_max());

  clock.Advance(Seconds(5));
  EXPECT_TRUE(meter.Update(300));
  EXPECT_EQ(meter.GetSampleCount(), 2u);
  EXPECT_EQ(meter.GetRemainingTime().InSeconds(), 85);
}

TEST(SpeedometerTest, ChangeTotalBytes) {
  base::ScopedMockClockOverride clock;
  Speedometer meter;

  // Sets total number of bytes and a couple of samples.
  meter.SetTotalBytes(2000);
  EXPECT_TRUE(meter.Update(100));
  clock.Advance(Seconds(5));
  EXPECT_TRUE(meter.Update(300));
  EXPECT_EQ(meter.GetSampleCount(), 2u);
  EXPECT_EQ(meter.GetRemainingTime().InSeconds(), 42);

  // Setting the total number of bytes to the existing value shouldn't change
  // anything.
  meter.SetTotalBytes(2000);
  EXPECT_EQ(meter.GetSampleCount(), 2u);
  EXPECT_EQ(meter.GetRemainingTime().InSeconds(), 42);

  // Changing the total number of bytes should clear the samples.
  meter.SetTotalBytes(3000);
  EXPECT_EQ(meter.GetSampleCount(), 0u);
  EXPECT_TRUE(meter.GetRemainingTime().is_max());

  clock.Advance(Seconds(5));
  EXPECT_TRUE(meter.Update(3000));
  EXPECT_EQ(meter.GetSampleCount(), 1u);

  // Go beyond the number of expected bytes.
  clock.Advance(Seconds(5));
  EXPECT_TRUE(meter.Update(4000));
  EXPECT_EQ(meter.GetSampleCount(), 2u);
  EXPECT_EQ(meter.GetRemainingTime().InSeconds(), 0);
}

}  // namespace
}  // namespace file_manager
