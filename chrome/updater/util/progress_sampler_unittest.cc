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
  ASSERT_EQ(*progress_sampler.GetRemainingTime(15000),
            base::Milliseconds(14700));

  // The first sample value was added at timeline 0 is now out of
  // range (500ms) and thus discarded.
  // Samples in queue now: [(20, 200), (200, 300), (520, 1200)].
  // Average speed: (1200-200) / (520-20) = 2.
  // Time remaining: (15000-1200)/2 = 6900.
  progress_sampler.AddSample(start_time + base::Milliseconds(520), 1200);
  ASSERT_EQ(progress_sampler.GetAverageSpeedPerMs(), 2);
  ASSERT_EQ(*progress_sampler.GetRemainingTime(15000),
            base::Milliseconds(6900));
}

}  // namespace updater
