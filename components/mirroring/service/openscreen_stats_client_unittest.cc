// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/mirroring/service/openscreen_stats_client.h"

#include "base/logging.h"
#include "media/cast/logging/stats_event_subscriber.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/openscreen/src/cast/streaming/public/sender_session.h"
#include "third_party/openscreen/src/cast/streaming/public/statistics.h"

namespace mirroring {

using openscreen::cast::SenderStats;
using openscreen::cast::StatisticType;
using HistogramsList = openscreen::cast::SenderStats::HistogramsList;
using StatisticsList = openscreen::cast::SenderStats::StatisticsList;
using SimpleHistogram = openscreen::cast::SimpleHistogram;
using CastStat = media::cast::StatsEventSubscriber::CastStat;

class OpenscreenStatsClientTest : public ::testing::Test {
 public:
  OpenscreenStatsClientTest()
      : test_statistics_list_(ConstructTestStatisticsList()),
        test_histogram_(ConstructTestHistogram()),
        test_histograms_list_(ConstructTestHistogramsList()),
        test_sender_stats_(ConstructTestSenderStats()),
        openscreen_stats_client_(std::make_unique<OpenscreenStatsClient>()) {}

  OpenscreenStatsClientTest(const OpenscreenStatsClientTest&) = delete;
  OpenscreenStatsClientTest& operator=(const OpenscreenStatsClientTest&) =
      delete;

  ~OpenscreenStatsClientTest() override = default;

  StatisticsList ConstructTestStatisticsList() {
    StatisticsList ret;
    for (std::size_t i = 0; i < ret.size(); ++i) {
      ret[i] = static_cast<double>(i);
    }
    return ret;
  }

  HistogramsList ConstructTestHistogramsList() {
    HistogramsList ret;
    for (auto& histogram : ret) {
      histogram = ConstructTestHistogram();
    }
    return ret;
  }

  SimpleHistogram ConstructTestHistogram() {
    SimpleHistogram ret = SimpleHistogram(0, 10, 2);
    ret.Add(1);
    ret.Add(4);
    ret.Add(7);
    return ret;
  }

  SenderStats ConstructTestSenderStats() {
    return SenderStats{.audio_statistics = ConstructTestStatisticsList(),
                       .audio_histograms = ConstructTestHistogramsList(),
                       .video_statistics = ConstructTestStatisticsList(),
                       .video_histograms = ConstructTestHistogramsList()};
  }

  SenderStats ConstructDefaultSenderStats() {
    return SenderStats{.audio_statistics = StatisticsList(),
                       .audio_histograms = HistogramsList(),
                       .video_statistics = StatisticsList(),
                       .video_histograms = HistogramsList()};
  }

 protected:
  StatisticsList test_statistics_list_;
  SimpleHistogram test_histogram_;
  HistogramsList test_histograms_list_;
  SenderStats test_sender_stats_;

  std::unique_ptr<OpenscreenStatsClient> openscreen_stats_client_;
};

TEST_F(OpenscreenStatsClientTest, OnStatisticsUpdatedValidValues) {
  // Test to verify that statistics are parsed and verified into both stats
  // and histograms.
  openscreen_stats_client_->OnStatisticsUpdated(test_sender_stats_);
  auto stats_dict = openscreen_stats_client_->GetStats();

  // Check that the GetStats() dict has been populated with audio stats and
  // histograms.
  const base::Value::Dict* audio_dict = stats_dict.FindDict(
      media::cast::StatsEventSubscriber::kAudioStatsDictKey);
  EXPECT_TRUE(audio_dict);
  EXPECT_TRUE(audio_dict->contains("NUM_FRAMES_CAPTURED"));
  EXPECT_TRUE(audio_dict->FindDouble("NUM_FRAMES_CAPTURED").has_value());

  EXPECT_TRUE(audio_dict->contains("NUM_FRAMES_DROPPED_BY_ENCODER"));
  EXPECT_TRUE(
      audio_dict->FindDouble("NUM_FRAMES_DROPPED_BY_ENCODER").has_value());

  EXPECT_TRUE(audio_dict->contains("AVG_CAPTURE_LATENCY_MS"));
  EXPECT_TRUE(audio_dict->FindDouble("AVG_CAPTURE_LATENCY_MS").has_value());

  // Check for some histograms.
  EXPECT_TRUE(audio_dict->contains("E2E_LATENCY_MS_HISTO"));
  EXPECT_TRUE(audio_dict->FindList("E2E_LATENCY_MS_HISTO")->size());

  EXPECT_TRUE(audio_dict->contains("NETWORK_LATENCY_MS_HISTO"));
  EXPECT_TRUE(audio_dict->FindList("NETWORK_LATENCY_MS_HISTO")->size());

  // Check that the GetStats() dict has been populated with video stats and
  // histograms.
  const base::Value::Dict* video_dict = stats_dict.FindDict(
      media::cast::StatsEventSubscriber::kVideoStatsDictKey);
  EXPECT_TRUE(video_dict);

  EXPECT_TRUE(video_dict->contains("NUM_FRAMES_CAPTURED"));
  EXPECT_TRUE(video_dict->FindDouble("NUM_FRAMES_CAPTURED").has_value());

  EXPECT_TRUE(video_dict->contains("NUM_FRAMES_DROPPED_BY_ENCODER"));
  EXPECT_TRUE(
      video_dict->FindDouble("NUM_FRAMES_DROPPED_BY_ENCODER").has_value());

  EXPECT_TRUE(video_dict->contains("AVG_CAPTURE_LATENCY_MS"));
  EXPECT_TRUE(video_dict->FindDouble("AVG_CAPTURE_LATENCY_MS").has_value());

  // Check for some histograms.
  EXPECT_TRUE(video_dict->contains("E2E_LATENCY_MS_HISTO"));
  EXPECT_TRUE(video_dict->FindList("E2E_LATENCY_MS_HISTO")->size());

  EXPECT_TRUE(video_dict->contains("NETWORK_LATENCY_MS_HISTO"));
  EXPECT_TRUE(video_dict->FindList("NETWORK_LATENCY_MS_HISTO")->size());
}

TEST_F(OpenscreenStatsClientTest, OnStatisticsUpdatedEmptyValues) {
  // Test to verify that statistics are parsed into both stats
  // and histograms even if they are empty.
  openscreen_stats_client_->OnStatisticsUpdated(ConstructDefaultSenderStats());
  auto stats_dict = openscreen_stats_client_->GetStats();

  // Check that the GetStats() dict has been populated with audio stats and
  // histograms.
  const base::Value::Dict* audio_dict = stats_dict.FindDict(
      media::cast::StatsEventSubscriber::kAudioStatsDictKey);
  EXPECT_TRUE(audio_dict);
  EXPECT_TRUE(audio_dict->contains("NUM_FRAMES_CAPTURED"));
  EXPECT_TRUE(audio_dict->FindDouble("NUM_FRAMES_CAPTURED").has_value());

  // Check for some histograms. They should have no size since they are empty.
  EXPECT_TRUE(audio_dict->contains("E2E_LATENCY_MS_HISTO"));
  EXPECT_FALSE(audio_dict->FindList("E2E_LATENCY_MS_HISTO")->size());

  EXPECT_TRUE(audio_dict->contains("NETWORK_LATENCY_MS_HISTO"));
  EXPECT_FALSE(audio_dict->FindList("NETWORK_LATENCY_MS_HISTO")->size());

  // Check that the GetStats() dict has been populated with video stats and
  // histograms.
  const base::Value::Dict* video_dict = stats_dict.FindDict(
      media::cast::StatsEventSubscriber::kVideoStatsDictKey);
  EXPECT_TRUE(video_dict);
  EXPECT_TRUE(video_dict->contains("NUM_FRAMES_CAPTURED"));
  EXPECT_TRUE(video_dict->FindDouble("NUM_FRAMES_CAPTURED").has_value());

  // Check for some histograms. They should have no size since they are empty.
  EXPECT_TRUE(video_dict->contains("E2E_LATENCY_MS_HISTO"));
  EXPECT_FALSE(video_dict->FindList("E2E_LATENCY_MS_HISTO")->size());

  EXPECT_TRUE(video_dict->contains("NETWORK_LATENCY_MS_HISTO"));
  EXPECT_FALSE(video_dict->FindList("NETWORK_LATENCY_MS_HISTO")->size());
}

}  // namespace mirroring
