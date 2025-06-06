// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/starboard/media/renderer/client_stats_tracker.h"

#include <array>
#include <cstdint>

#include "base/containers/span.h"
#include "chromecast/starboard/media/media/starboard_api_wrapper.h"
#include "media/base/mock_filters.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromecast {
namespace media {
namespace {

using ::media::PipelineStatistics;
using ::testing::AllOf;
using ::testing::Eq;
using ::testing::Field;

auto MatchesStats(const PipelineStatistics& stats) {
  return AllOf(Field(&PipelineStatistics::audio_bytes_decoded,
                     Eq(stats.audio_bytes_decoded)),
               Field(&PipelineStatistics::video_bytes_decoded,
                     Eq(stats.video_bytes_decoded)),
               Field(&PipelineStatistics::video_frames_decoded,
                     Eq(stats.video_frames_decoded)),
               Field(&PipelineStatistics::video_frames_dropped,
                     Eq(stats.video_frames_dropped)));
}

// Creates an audio sample containing the given data.
StarboardSampleInfo CreateAudioSample(base::span<const uint8_t> data) {
  StarboardSampleInfo sample_info;
  sample_info.type = kStarboardMediaTypeAudio;
  sample_info.buffer = data.data();
  sample_info.buffer_size = data.size();
  sample_info.timestamp = 0;
  sample_info.side_data = base::span<const StarboardSampleSideData>();
  sample_info.audio_sample_info = {};
  sample_info.drm_info = nullptr;

  return sample_info;
}

// Creates a video sample containing the given data.
StarboardSampleInfo CreateVideoSample(base::span<const uint8_t> data) {
  StarboardSampleInfo sample_info;
  sample_info.type = kStarboardMediaTypeVideo;
  sample_info.buffer = data.data();
  sample_info.buffer_size = data.size();
  sample_info.timestamp = 0;
  sample_info.side_data = base::span<const StarboardSampleSideData>();
  sample_info.video_sample_info = {};
  sample_info.drm_info = nullptr;

  return sample_info;
}

TEST(ClientStatsTrackerTest, UpdatesStatsForAudioBuffer) {
  constexpr auto kAudioData = std::to_array<uint8_t>({1, 2, 3});
  StarboardPlayerInfo player_info = {};
  StarboardSampleInfo sample_info = CreateAudioSample(kAudioData);
  PipelineStatistics expected_stats;
  expected_stats.audio_bytes_decoded = kAudioData.size();
  expected_stats.video_bytes_decoded = 0;
  expected_stats.video_frames_decoded = 0;
  expected_stats.video_frames_dropped = 0;

  ::media::MockRendererClient client;
  EXPECT_CALL(client, OnStatisticsUpdate(MatchesStats(expected_stats)))
      .Times(1);

  ClientStatsTracker stats_tracker(&client);

  stats_tracker.UpdateStats(player_info, sample_info);
}

TEST(ClientStatsTrackerTest, UpdatesStatsForVideoBuffer) {
  constexpr auto kVideoData1 = std::to_array<uint8_t>({1, 2, 3, 4, 5});
  constexpr auto kVideoData2 = std::to_array<uint8_t>({6, 7, 8});

  StarboardPlayerInfo player_info_1 = {};
  player_info_1.total_video_frames = 2;
  player_info_1.dropped_video_frames = 0;

  StarboardSampleInfo sample_info_1 = CreateVideoSample(kVideoData1);
  PipelineStatistics expected_stats_1;
  expected_stats_1.audio_bytes_decoded = 0;
  expected_stats_1.video_bytes_decoded = kVideoData1.size();
  expected_stats_1.video_frames_decoded = 2;
  expected_stats_1.video_frames_dropped = 0;

  StarboardPlayerInfo player_info_2 = {};
  player_info_2.total_video_frames = 3;
  player_info_2.dropped_video_frames = 0;

  StarboardSampleInfo sample_info_2 = CreateVideoSample(kVideoData2);
  PipelineStatistics expected_stats_2;
  expected_stats_2.audio_bytes_decoded = 0;
  expected_stats_2.video_bytes_decoded = kVideoData2.size();
  expected_stats_2.video_frames_decoded = 1;
  expected_stats_2.video_frames_dropped = 0;

  ::media::MockRendererClient client;
  EXPECT_CALL(client, OnStatisticsUpdate(MatchesStats(expected_stats_1)))
      .Times(1);
  EXPECT_CALL(client, OnStatisticsUpdate(MatchesStats(expected_stats_2)))
      .Times(1);

  ClientStatsTracker stats_tracker(&client);

  stats_tracker.UpdateStats(player_info_1, sample_info_1);
  stats_tracker.UpdateStats(player_info_2, sample_info_2);
}

}  // namespace
}  // namespace media
}  // namespace chromecast
