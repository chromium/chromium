// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/services/readaloud/prefetch/prefetch_mode_scheduler.h"

#include "base/time/time.h"
#include "chrome/common/readaloud/read_aloud_constants.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace readaloud {

using PrefetchModeSchedulerTest = testing::Test;

TEST_F(PrefetchModeSchedulerTest, DefaultModeIsSpeed) {
  PrefetchModeScheduler scheduler;
  EXPECT_EQ(scheduler.GetPrefetchMode(), PrefetchMode::kSpeed);
  EXPECT_EQ(scheduler.GetTargetPrefetchDuration(),
            kAudioBufferPrefetchWatermark);
}

TEST_F(PrefetchModeSchedulerTest, UpdateModeUpgradesToQualityAtWatermark) {
  PrefetchModeScheduler scheduler;

  EXPECT_EQ(scheduler.UpdateMode(base::Seconds(4)), PrefetchMode::kSpeed);
  EXPECT_EQ(scheduler.UpdateMode(base::Seconds(14)), PrefetchMode::kSpeed);

  EXPECT_EQ(scheduler.UpdateMode(base::Seconds(15)), PrefetchMode::kQuality);
  EXPECT_EQ(scheduler.GetPrefetchMode(), PrefetchMode::kQuality);
  EXPECT_EQ(scheduler.GetTargetPrefetchDuration(), kMaxDecodedAudioDuration);
}

TEST_F(PrefetchModeSchedulerTest, UpdateModeRetainsQualityInHysteresisZone) {
  PrefetchModeScheduler scheduler;
  scheduler.UpdateMode(base::Seconds(15));
  EXPECT_EQ(scheduler.GetPrefetchMode(), PrefetchMode::kQuality);

  EXPECT_EQ(scheduler.UpdateMode(base::Seconds(10)), PrefetchMode::kQuality);
  EXPECT_EQ(scheduler.UpdateMode(base::Seconds(5)), PrefetchMode::kQuality);
  EXPECT_EQ(scheduler.GetPrefetchMode(), PrefetchMode::kQuality);
}

TEST_F(PrefetchModeSchedulerTest, UpdateModeDowngradesToSpeedBelowMinDuration) {
  PrefetchModeScheduler scheduler;
  scheduler.UpdateMode(base::Seconds(15));
  EXPECT_EQ(scheduler.GetPrefetchMode(), PrefetchMode::kQuality);

  EXPECT_EQ(scheduler.UpdateMode(base::Seconds(4.9)), PrefetchMode::kSpeed);
  EXPECT_EQ(scheduler.GetPrefetchMode(), PrefetchMode::kSpeed);
  EXPECT_EQ(scheduler.GetTargetPrefetchDuration(),
            kAudioBufferPrefetchWatermark);
}

TEST_F(PrefetchModeSchedulerTest, ResetRestoresSpeedMode) {
  PrefetchModeScheduler scheduler;
  scheduler.UpdateMode(base::Seconds(15));
  EXPECT_EQ(scheduler.GetPrefetchMode(), PrefetchMode::kQuality);

  scheduler.Reset();
  EXPECT_EQ(scheduler.GetPrefetchMode(), PrefetchMode::kSpeed);
  EXPECT_EQ(scheduler.GetTargetPrefetchDuration(),
            kAudioBufferPrefetchWatermark);
}

}  // namespace readaloud
