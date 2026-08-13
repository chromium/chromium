// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/services/readaloud/prefetch/prefetch_mode_scheduler.h"

#include "base/time/time.h"
#include "chrome/common/readaloud/read_aloud_constants.h"
#include "chrome/services/readaloud/chunking/text_chunker.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace readaloud {

using PrefetchModeSchedulerTest = testing::Test;

TEST_F(PrefetchModeSchedulerTest, DefaultModeIsSpeed) {
  PrefetchModeScheduler scheduler;
  EXPECT_EQ(ChunkingMode::kSpeed, scheduler.GetChunkingMode());
  EXPECT_EQ(kAudioBufferPrefetchWatermark,
            scheduler.GetTargetPrefetchDuration());
}

TEST_F(PrefetchModeSchedulerTest, UpdateModeUpgradesToQualityAtWatermark) {
  PrefetchModeScheduler scheduler;

  EXPECT_EQ(ChunkingMode::kSpeed, scheduler.UpdateMode(base::Seconds(4)));
  EXPECT_EQ(ChunkingMode::kSpeed, scheduler.UpdateMode(base::Seconds(14)));

  EXPECT_EQ(ChunkingMode::kQuality, scheduler.UpdateMode(base::Seconds(15)));
  EXPECT_EQ(ChunkingMode::kQuality, scheduler.GetChunkingMode());
  EXPECT_EQ(kMaxDecodedAudioDuration, scheduler.GetTargetPrefetchDuration());
}

TEST_F(PrefetchModeSchedulerTest, UpdateModeRetainsQualityInHysteresisZone) {
  PrefetchModeScheduler scheduler;
  scheduler.UpdateMode(base::Seconds(15));
  EXPECT_EQ(ChunkingMode::kQuality, scheduler.GetChunkingMode());

  EXPECT_EQ(ChunkingMode::kQuality, scheduler.UpdateMode(base::Seconds(10)));
  EXPECT_EQ(ChunkingMode::kQuality, scheduler.UpdateMode(base::Seconds(5)));
  EXPECT_EQ(ChunkingMode::kQuality, scheduler.GetChunkingMode());
}

TEST_F(PrefetchModeSchedulerTest, UpdateModeDowngradesToSpeedBelowMinDuration) {
  PrefetchModeScheduler scheduler;
  scheduler.UpdateMode(base::Seconds(15));
  EXPECT_EQ(ChunkingMode::kQuality, scheduler.GetChunkingMode());

  EXPECT_EQ(ChunkingMode::kSpeed, scheduler.UpdateMode(base::Seconds(4.9)));
  EXPECT_EQ(ChunkingMode::kSpeed, scheduler.GetChunkingMode());
  EXPECT_EQ(kAudioBufferPrefetchWatermark,
            scheduler.GetTargetPrefetchDuration());
}

TEST_F(PrefetchModeSchedulerTest, ResetRestoresSpeedMode) {
  PrefetchModeScheduler scheduler;
  scheduler.UpdateMode(base::Seconds(15));
  EXPECT_EQ(ChunkingMode::kQuality, scheduler.GetChunkingMode());

  scheduler.Reset();
  EXPECT_EQ(ChunkingMode::kSpeed, scheduler.GetChunkingMode());
  EXPECT_EQ(kAudioBufferPrefetchWatermark,
            scheduler.GetTargetPrefetchDuration());
}

}  // namespace readaloud
