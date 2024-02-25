// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/updater/util/progress_sampler.h"

#include <optional>

#include "base/time/time.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace updater {

TEST(ProgressSampler, Samples) {
  // Create a sampler that keep samples within last 500ms and calculates
  // average progress only if minimum time range 100ms is reached.
  ProgressSampler progress_sampler(base::Milliseconds(500),
                                   base::Milliseconds(100));
  ASSERT_FALSE(progress_sampler.HasEnoughSamples());
  ASSERT_FALSE(progress_sampler.GetRemainingTime(15000));

  const base::Time start_time = base::Time::Now();
  progress_sampler.AddSample(start_time, 100);
  ASSERT_FALSE(progress_sampler.HasEnoughSamples());
  ASSERT_FALSE(progress_sampler.GetRemainingTime(15000));

  // Minimum time range 100ms has not been reached yet.
  progress_sampler.AddSample(start_time + base::Milliseconds(20), 200);
  ASSERT_FALSE(progress_sampler.HasEnoughSamples());
  ASSERT_FALSE(progress_sampler.GetRemainingTime(15000));

  // Samples in queue: [(0, 100), (20, 200), (200, 300)].
  // Average speed: (300-100) / (200-0) = 1.
  // Time remaining: (15000-300)/1 = 14700.
  progress_sampler.AddSample(start_time + base::Milliseconds(200), 300);
  ASSERT_TRUE(progress_sampler.HasEnoughSamples());
  ASSERT_EQ(progress_sampler.GetAverageSpeedPerMs(), 1);
  ASSERT_EQ(progress_sampler.GetRemainingTime(15000)->InMilliseconds(), 14700);

  // The first sample value was added at timeline 0 is now out of
  // range (500ms) and thus discarded.
  // Samples in queue now: [(20, 200), (200, 300), (520, 1200)].
  // Average speed: (1200-200) / (520-20) = 2.
  // Time remaining: (15000-1200)/2 = 6900.
  progress_sampler.AddSample(start_time + base::Milliseconds(520), 1200);
  ASSERT_EQ(progress_sampler.GetAverageSpeedPerMs(), 2);
  ASSERT_EQ(progress_sampler.GetRemainingTime(15000)->InMilliseconds(), 6900);
}

TEST(ProgressSampler, PercentageRange) {
  // Create a sampler that keep samples within last 5s and calculates average
  // progress only if minimum time range 1s is reached.
  ProgressSampler progress_sampler(base::Seconds(5), base::Seconds(1));
  ASSERT_FALSE(progress_sampler.HasEnoughSamples());
  ASSERT_FALSE(progress_sampler.GetRemainingTime(100));

  const base::Time start_time = base::Time::Now();
  progress_sampler.AddSample(start_time, 25);
  ASSERT_FALSE(progress_sampler.HasEnoughSamples());
  ASSERT_FALSE(progress_sampler.GetRemainingTime(100));

  // Minimum time range 1s has not been reached yet.
  progress_sampler.AddSample(start_time + base::Seconds(0.2), 30);
  ASSERT_FALSE(progress_sampler.HasEnoughSamples());
  ASSERT_FALSE(progress_sampler.GetRemainingTime(100));

  // Samples in queue: [(0, 25), (200, 30), (2000, 35)].
  // Average speed: (35-25) / (2000-0) = 0.005.
  // Time remaining: (100-35+0.005-1) / 0.005 = 12800.
  progress_sampler.AddSample(start_time + base::Seconds(2), 35);
  ASSERT_TRUE(progress_sampler.HasEnoughSamples());
  ASSERT_EQ(progress_sampler.GetAverageSpeedPerMs(), 0.005);
  ASSERT_EQ(progress_sampler.GetRemainingTime(100)->InMilliseconds(), 12800);

  // The first sample value was added at timeline 0 is now out of range (5s) and
  // thus discarded. Samples in queue now: [(200, 30), (2000, 35), (5200, 60)].
  // Average speed: (60-30) / (5200-200) = 0.006.
  // Time remaining: (100-60+0.006-1) / 0.006 = 6501.
  progress_sampler.AddSample(start_time + base::Seconds(5.2), 60);
  ASSERT_EQ(progress_sampler.GetAverageSpeedPerMs(), 0.006);
  ASSERT_EQ(progress_sampler.GetRemainingTime(100)->InMilliseconds(), 6501);
}
}  // namespace updater
