// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/starboard/media/renderer/client_stats_tracker.h"

#include <array>
#include <cstdint>

#include "base/containers/span.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "chromecast/base/metrics/mock_cast_metrics_helper.h"
#include "chromecast/starboard/media/media/starboard_api_wrapper.h"
#include "media/base/mock_filters.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromecast {
namespace media {
namespace {

using ::base::test::SingleThreadTaskEnvironment;
using ::chromecast::metrics::MockCastMetricsHelper;
using ::media::PipelineStatistics;
using ::testing::_;
using ::testing::AllOf;
using ::testing::Eq;
using ::testing::Field;
using ::testing::InSequence;

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
  SingleThreadTaskEnvironment task_environment{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  constexpr auto kAudioData = std::to_array<uint8_t>({1, 2, 3});
  StarboardPlayerInfo player_info = {};
  StarboardSampleInfo sample_info = CreateAudioSample(kAudioData);
  PipelineStatistics expected_stats;
  expected_stats.audio_bytes_decoded = kAudioData.size();
  expected_stats.video_bytes_decoded = 0;
  expected_stats.video_frames_decoded = 0;
  expected_stats.video_frames_dropped = 0;

  MockCastMetricsHelper metrics_helper;
  ::media::MockRendererClient client;
  EXPECT_CALL(client, OnStatisticsUpdate(MatchesStats(expected_stats)))
      .Times(1);

  ClientStatsTracker stats_tracker(&client, &metrics_helper);

  task_environment.FastForwardBy(base::Seconds(5));
  stats_tracker.UpdateStats(player_info, sample_info);
}

TEST(ClientStatsTrackerTest, UpdatesStatsForVideoBuffer) {
  SingleThreadTaskEnvironment task_environment{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
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

  MockCastMetricsHelper metrics_helper;
  ::media::MockRendererClient client;
  EXPECT_CALL(client, OnStatisticsUpdate(MatchesStats(expected_stats_1)))
      .Times(1);
  EXPECT_CALL(client, OnStatisticsUpdate(MatchesStats(expected_stats_2)))
      .Times(1);

  ClientStatsTracker stats_tracker(&client, &metrics_helper);

  task_environment.FastForwardBy(base::Seconds(5));
  stats_tracker.UpdateStats(player_info_1, sample_info_1);

  task_environment.FastForwardBy(base::Seconds(5));
  stats_tracker.UpdateStats(player_info_2, sample_info_2);
}

TEST(ClientStatsTrackerTest, DoesNotUpdateStatsIfNotEnoughTimeHasPassed) {
  SingleThreadTaskEnvironment task_environment{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  constexpr auto kAudioData = std::to_array<uint8_t>({1, 2, 3});
  StarboardPlayerInfo player_info = {};
  StarboardSampleInfo sample_info = CreateAudioSample(kAudioData);
  PipelineStatistics expected_stats;
  expected_stats.audio_bytes_decoded = kAudioData.size();
  expected_stats.video_bytes_decoded = 0;
  expected_stats.video_frames_decoded = 0;
  expected_stats.video_frames_dropped = 0;

  MockCastMetricsHelper metrics_helper;
  ::media::MockRendererClient client;
  EXPECT_CALL(client, OnStatisticsUpdate(_)).Times(0);

  ClientStatsTracker stats_tracker(&client, &metrics_helper);

  // No time has elapsed between the creation of stats_tracker and the call to
  // UpdateStats.
  stats_tracker.UpdateStats(player_info, sample_info);
}

TEST(ClientStatsTrackerTest, AccumulatesStatsFromMultipleBuffers) {
  SingleThreadTaskEnvironment task_environment{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  // The size of the buffer is all that matters here.
  const std::vector<uint8_t> video_data_1(10000);
  const std::vector<uint8_t> video_data_2(5000);
  const std::vector<uint8_t> audio_data(5000);

  StarboardPlayerInfo player_info_1 = {};
  player_info_1.total_video_frames = 3;
  player_info_1.dropped_video_frames = 1;

  StarboardPlayerInfo player_info_2 = {};
  player_info_2.total_video_frames = 5;
  player_info_2.dropped_video_frames = 1;

  // Not relevant to audio stats.
  StarboardPlayerInfo player_info_3 = {};

  StarboardSampleInfo sample_info_1 = CreateVideoSample(video_data_1);
  StarboardSampleInfo sample_info_2 = CreateVideoSample(video_data_2);
  StarboardSampleInfo sample_info_3 = CreateAudioSample(audio_data);

  PipelineStatistics expected_stats;
  expected_stats.audio_bytes_decoded = audio_data.size();
  expected_stats.video_bytes_decoded =
      video_data_1.size() + video_data_2.size();
  expected_stats.video_frames_decoded = 5;
  expected_stats.video_frames_dropped = 1;

  MockCastMetricsHelper metrics_helper;
  EXPECT_CALL(metrics_helper,
              RecordApplicationEventWithValue("Cast.Platform.AudioBitrate", 8));
  EXPECT_CALL(metrics_helper, RecordApplicationEventWithValue(
                                  "Cast.Platform.VideoBitrate", 24));
  ::media::MockRendererClient client;
  EXPECT_CALL(client, OnStatisticsUpdate(MatchesStats(expected_stats)))
      .Times(1);

  ClientStatsTracker stats_tracker(&client, &metrics_helper);

  stats_tracker.UpdateStats(player_info_1, sample_info_1);
  stats_tracker.UpdateStats(player_info_2, sample_info_2);

  task_environment.FastForwardBy(base::Seconds(5));
  // Now that enough time has passed, the stats should be updated with info from
  // all 3 buffers.
  stats_tracker.UpdateStats(player_info_3, sample_info_3);
}

TEST(ClientStatsTrackerTest, UpdatesAreDeltasAndNotCumulative) {
  // This test verifies that ClientStatsTracker resets its stats after each
  // update to clients.
  SingleThreadTaskEnvironment task_environment{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};

  // The size of the buffer is all that matters here.
  const std::vector<uint8_t> video_data_1(10000);

  // These buffers will be pushed after the first update.
  const std::vector<uint8_t> video_data_2(5000);
  const std::vector<uint8_t> audio_data(5000);

  StarboardPlayerInfo player_info_1 = {};
  player_info_1.total_video_frames = 3;
  player_info_1.dropped_video_frames = 1;

  StarboardPlayerInfo player_info_2 = {};
  player_info_2.total_video_frames = 5;
  player_info_2.dropped_video_frames = 1;

  // Not relevant to audio stats.
  StarboardPlayerInfo player_info_3 = {};

  StarboardSampleInfo sample_info_1 = CreateVideoSample(video_data_1);
  StarboardSampleInfo sample_info_2 = CreateVideoSample(video_data_2);
  StarboardSampleInfo sample_info_3 = CreateAudioSample(audio_data);

  // stats for just sample_info_1 and player_info_1.
  PipelineStatistics expected_stats_1;
  expected_stats_1.audio_bytes_decoded = 0;
  expected_stats_1.video_bytes_decoded = video_data_1.size();
  expected_stats_1.video_frames_decoded = 3;
  expected_stats_1.video_frames_dropped = 1;

  // stats for sample_info_2, player_info_2, sample_info_3, and player_info_3.
  // Note that these stats should be deltas, not totals.
  PipelineStatistics expected_stats_2;
  expected_stats_2.audio_bytes_decoded = audio_data.size();
  expected_stats_2.video_bytes_decoded = video_data_2.size();
  expected_stats_2.video_frames_decoded = 2;
  expected_stats_2.video_frames_dropped = 0;

  MockCastMetricsHelper metrics_helper;

  // The ordering of audio and video stats updates is irrelevant.
  EXPECT_CALL(metrics_helper,
              RecordApplicationEventWithValue("Cast.Platform.AudioBitrate", 4))
      .Times(1);

  {
    InSequence seq;
    EXPECT_CALL(metrics_helper, RecordApplicationEventWithValue(
                                    "Cast.Platform.VideoBitrate", 16))
        .Times(1);
    EXPECT_CALL(metrics_helper, RecordApplicationEventWithValue(
                                    "Cast.Platform.VideoBitrate", 4))
        .Times(1);
  }

  ::media::MockRendererClient client;
  {
    InSequence seq;
    EXPECT_CALL(client, OnStatisticsUpdate(MatchesStats(expected_stats_1)))
        .Times(1);
    EXPECT_CALL(client, OnStatisticsUpdate(MatchesStats(expected_stats_2)))
        .Times(1);
  }

  ClientStatsTracker stats_tracker(&client, &metrics_helper);

  task_environment.FastForwardBy(base::Seconds(5));
  // Enough time has passed, so the stats should be sent to clients.
  stats_tracker.UpdateStats(player_info_1, sample_info_1);

  task_environment.FastForwardBy(base::Seconds(1));
  // This update does not get immediately pushed, since not enough time has
  // passed.
  stats_tracker.UpdateStats(player_info_2, sample_info_2);

  // Note that the delta is 10s total here. This affects the bitrate that gets
  // reported to the metrics helper.
  task_environment.FastForwardBy(base::Seconds(9));

  // Now that enough time has passed, stats for the second and third buffer
  // should be pushed.
  stats_tracker.UpdateStats(player_info_3, sample_info_3);
}

}  // namespace
}  // namespace media
}  // namespace chromecast
