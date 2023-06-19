// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/file_manager/speedometer.h"

#include <cmath>
#include <limits>

#include "base/test/scoped_mock_clock_override.h"
#include "base/time/time.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace file_manager {

namespace {

TEST(SpeedometerTest, RemainingTime) {
  base::ScopedMockClockOverride mock_clock;

  Speedometer meter;

  // Testing without setting the total bytes:
  EXPECT_EQ(0u, meter.GetSampleCount());
  EXPECT_EQ(0, meter.GetRemainingSeconds());

  meter.SetTotalBytes(2000);
  EXPECT_EQ(0u, meter.GetSampleCount());
  EXPECT_EQ(0, meter.GetRemainingSeconds());

  // 1st sample.
  // 1st sample, but not enough to calculate the remaining time.
  mock_clock.Advance(base::Milliseconds(11000));

  meter.Update(100);
  EXPECT_EQ(1u, meter.GetSampleCount());
  EXPECT_EQ(0, meter.GetRemainingSeconds());

  // Sample received less than 1 second after the previous one should be
  // ignored.
  mock_clock.Advance(base::Milliseconds(999));
  meter.Update(300);
  EXPECT_EQ(1u, meter.GetSampleCount());
  EXPECT_EQ(0, meter.GetRemainingSeconds());

  // 2nd sample, the remaining time can be computed.
  mock_clock.Advance(base::Milliseconds(1));
  meter.Update(300);
  EXPECT_EQ(2u, meter.GetSampleCount());
  EXPECT_EQ(9, round(meter.GetRemainingSeconds()));

  // 3rd sample. +1 second and still only processed 300 bytes.
  mock_clock.Advance(base::Milliseconds(1000));
  meter.Update(300);
  EXPECT_EQ(3u, meter.GetSampleCount());
  EXPECT_EQ(17, round(meter.GetRemainingSeconds()));

  // 4th sample, +2 seconds and still only 300 bytes.
  mock_clock.Advance(base::Milliseconds(2000));
  meter.Update(300);
  EXPECT_EQ(4u, meter.GetSampleCount());
  EXPECT_EQ(42, round(meter.GetRemainingSeconds()));

  // 5th sample, +1 second and now bumped from 300 to 600 bytes.
  mock_clock.Advance(base::Milliseconds(1000));
  meter.Update(600);
  EXPECT_EQ(5u, meter.GetSampleCount());
  EXPECT_EQ(20, round(meter.GetRemainingSeconds()));

  // Elapsed time should impact the remaining time.
  mock_clock.Advance(base::Milliseconds(12000));
  EXPECT_EQ(8, round(meter.GetRemainingSeconds()));

  // GetRemainingSeconds() can return negative value.
  mock_clock.Advance(base::Milliseconds(12000));
  EXPECT_EQ(-4, round(meter.GetRemainingSeconds()));
}

TEST(SpeedometerTest, Samples) {
  base::ScopedMockClockOverride mock_clock;

  constexpr size_t kMaxSamples = 20;
  Speedometer meter;
  meter.SetTotalBytes(20000);

  // Slow speed of 100 bytes per second.
  int total_transferred = 0;
  for (size_t i = 0; i < kMaxSamples; i++) {
    EXPECT_EQ(i, meter.GetSampleCount());
    mock_clock.Advance(base::Milliseconds(1000));
    total_transferred = i * 100;
    meter.Update(total_transferred);
  }

  EXPECT_EQ(kMaxSamples, meter.GetSampleCount());
  EXPECT_EQ(181, round(meter.GetRemainingSeconds()));

  // +200 to make it compatible with the values in the unittest in the JS
  // version.
  const int initial_transferred_bytes = total_transferred + 200;
  // Faster speed of 300 bytes per second.
  for (size_t i = 0; i < kMaxSamples; i++) {
    // Check buffer not expanded more than the specified length.
    EXPECT_EQ(kMaxSamples, meter.GetSampleCount());
    mock_clock.Advance(base::Milliseconds(1000));
    total_transferred = initial_transferred_bytes + (i * 300);
    meter.Update(total_transferred);

    // Current speed should be seen as accelerating, thus the remaining time
    // decreasing.
    EXPECT_GT(181, meter.GetRemainingSeconds());
  }

  EXPECT_EQ(kMaxSamples, meter.GetSampleCount());
  EXPECT_EQ(41, round(meter.GetRemainingSeconds()));

  // Stalling.
  for (size_t i = 0; i < kMaxSamples; i++) {
    // Check buffer not expanded more than the specified length.
    EXPECT_EQ(kMaxSamples, meter.GetSampleCount());
    mock_clock.Advance(base::Milliseconds(1000));
    meter.Update(total_transferred);
  }

  // The remaining time should increase from the previous value.
  EXPECT_LT(41, round(meter.GetRemainingSeconds()));

  // When all samples have the same value the remaining time goes to infinity,
  // because the Linear Interpolation expects an inclination/slope, but with all
  // values the same, it becomes a horizontal line, meaning that the bytes will
  // never grow towards the total bytes.
  EXPECT_EQ(std::numeric_limits<double>::infinity(),
            meter.GetRemainingSeconds());
}

}  // namespace
}  // namespace file_manager
