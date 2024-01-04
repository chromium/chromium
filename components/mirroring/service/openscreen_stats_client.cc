// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/mirroring/service/openscreen_stats_client.h"

#include <stdint.h>

#include "base/format_macros.h"
#include "base/strings/stringprintf.h"
#include "media/cast/logging/stats_event_subscriber.h"

namespace mirroring {

namespace {

media::cast::StatsEventSubscriber::CastStat StatisticTypeToCastStat(
    const openscreen::cast::StatisticType& stat_type) {
  switch (stat_type) {
    case openscreen::cast::StatisticType::kEnqueueFps:
      return media::cast::StatsEventSubscriber::CastStat::ENQUEUE_FPS;
    case openscreen::cast::StatisticType::kAvgCaptureLatencyMs:
      return media::cast::StatsEventSubscriber::CastStat::
          AVG_CAPTURE_LATENCY_MS;
    case openscreen::cast::StatisticType::kAvgEncodeTimeMs:
      return media::cast::StatsEventSubscriber::CastStat::AVG_ENCODE_TIME_MS;
    case openscreen::cast::StatisticType::kAvgQueueingLatencyMs:
      return media::cast::StatsEventSubscriber::CastStat::
          AVG_QUEUEING_LATENCY_MS;
    case openscreen::cast::StatisticType::kAvgNetworkLatencyMs:
      return media::cast::StatsEventSubscriber::CastStat::
          AVG_NETWORK_LATENCY_MS;
    case openscreen::cast::StatisticType::kAvgPacketLatencyMs:
      return media::cast::StatsEventSubscriber::CastStat::AVG_PACKET_LATENCY_MS;
    case openscreen::cast::StatisticType::kAvgFrameLatencyMs:
      return media::cast::StatsEventSubscriber::CastStat::AVG_FRAME_LATENCY_MS;
    case openscreen::cast::StatisticType::kAvgEndToEndLatencyMs:
      return media::cast::StatsEventSubscriber::CastStat::AVG_E2E_LATENCY_MS;
    case openscreen::cast::StatisticType::kEncodeRateKbps:
      return media::cast::StatsEventSubscriber::CastStat::ENCODE_KBPS;
    case openscreen::cast::StatisticType::kPacketTransmissionRateKbps:
      return media::cast::StatsEventSubscriber::CastStat::TRANSMISSION_KBPS;
    case openscreen::cast::StatisticType::kTimeSinceLastReceiverResponseMs:
      return media::cast::StatsEventSubscriber::CastStat::
          MS_SINCE_LAST_RECEIVER_RESPONSE;
    case openscreen::cast::StatisticType::kNumFramesCaptured:
      return media::cast::StatsEventSubscriber::CastStat::NUM_FRAMES_CAPTURED;
    case openscreen::cast::StatisticType::kNumFramesDroppedByEncoder:
      return media::cast::StatsEventSubscriber::CastStat::
          NUM_FRAMES_DROPPED_BY_ENCODER;
    case openscreen::cast::StatisticType::kNumLateFrames:
      return media::cast::StatsEventSubscriber::CastStat::NUM_FRAMES_LATE;
    case openscreen::cast::StatisticType::kNumPacketsSent:
      return media::cast::StatsEventSubscriber::CastStat::NUM_PACKETS_SENT;
    case openscreen::cast::StatisticType::kNumPacketsReceived:
      return media::cast::StatsEventSubscriber::CastStat::NUM_PACKETS_RECEIVED;
    case openscreen::cast::StatisticType::kFirstEventTimeMs:
      return media::cast::StatsEventSubscriber::CastStat::FIRST_EVENT_TIME_MS;
    case openscreen::cast::StatisticType::kLastEventTimeMs:
      return media::cast::StatsEventSubscriber::CastStat::LAST_EVENT_TIME_MS;
    default:
      return media::cast::StatsEventSubscriber::CastStat::
          UNKNOWN_OPEN_SCREEN_STAT;
  }
}

media::cast::StatsEventSubscriber::CastStat HistogramTypeToCastStat(
    const openscreen::cast::HistogramType& histogram_type) {
  switch (histogram_type) {
    case openscreen::cast::HistogramType::kCaptureLatencyMs:
      return media::cast::StatsEventSubscriber::CastStat::
          CAPTURE_LATENCY_MS_HISTO;
    case openscreen::cast::HistogramType::kEncodeTimeMs:
      return media::cast::StatsEventSubscriber::CastStat::ENCODE_TIME_MS_HISTO;
    case openscreen::cast::HistogramType::kQueueingLatencyMs:
      return media::cast::StatsEventSubscriber::CastStat::
          QUEUEING_LATENCY_MS_HISTO;
    case openscreen::cast::HistogramType::kNetworkLatencyMs:
      return media::cast::StatsEventSubscriber::CastStat::
          NETWORK_LATENCY_MS_HISTO;
    case openscreen::cast::HistogramType::kPacketLatencyMs:
      return media::cast::StatsEventSubscriber::CastStat::
          PACKET_LATENCY_MS_HISTO;
    case openscreen::cast::HistogramType::kEndToEndLatencyMs:
      return media::cast::StatsEventSubscriber::CastStat::E2E_LATENCY_MS_HISTO;
    case openscreen::cast::HistogramType::kFrameLatenessMs:
      return media::cast::StatsEventSubscriber::CastStat::LATE_FRAME_MS_HISTO;
    default:
      return media::cast::StatsEventSubscriber::CastStat::
          UNKNOWN_OPEN_SCREEN_HISTO;
  }
}
}  // namespace

OpenscreenStatsClient::OpenscreenStatsClient() = default;
OpenscreenStatsClient::~OpenscreenStatsClient() = default;

base::Value::Dict OpenscreenStatsClient::GetStats() const {
  return most_recent_stats_.Clone();
}

base::Value::Dict OpenscreenStatsClient::ConvertSenderStatsToDict(
    const openscreen::cast::SenderStats& updated_stats) const {
  base::Value::Dict ret;

  base::Value::Dict audio_stats =
      ConvertStatisticsListToDict(updated_stats.audio_statistics);
  audio_stats.Merge(
      ConvertHistogramsListToDict(updated_stats.audio_histograms));
  ret.Set(media::cast::StatsEventSubscriber::kAudioStatsDictKey,
          std::move(audio_stats));

  base::Value::Dict video_stats =
      ConvertStatisticsListToDict(updated_stats.video_statistics);
  video_stats.Merge(
      ConvertHistogramsListToDict(updated_stats.video_histograms));
  ret.Set(media::cast::StatsEventSubscriber::kVideoStatsDictKey,
          std::move(video_stats));
  return ret;
}

base::Value::Dict OpenscreenStatsClient::ConvertStatisticsListToDict(
    const openscreen::cast::SenderStats::StatisticsList& stats_list) const {
  base::Value::Dict ret;
  for (std::size_t i = 0; i < stats_list.size(); ++i) {
    const char* key = media::cast::StatsEventSubscriber::CastStatToString(
        StatisticTypeToCastStat(
            static_cast<openscreen::cast::StatisticType>(i)));
    if (std::isfinite(stats_list[i])) {
      ret.Set(key, stats_list[i]);
    } else {
      ret.Set(key, "null");
    }
  }
  return ret;
}

base::Value::Dict OpenscreenStatsClient::ConvertHistogramsListToDict(
    const openscreen::cast::SenderStats::HistogramsList& histograms_list)
    const {
  base::Value::Dict ret;
  for (std::size_t i = 0; i < histograms_list.size(); ++i) {
    ret.Set(media::cast::StatsEventSubscriber::CastStatToString(
                HistogramTypeToCastStat(
                    static_cast<openscreen::cast::HistogramType>(i))),
            ConvertOpenscreenHistogramToList(histograms_list[i]));
  }
  return ret;
}

base::Value::List OpenscreenStatsClient::ConvertOpenscreenHistogramToList(
    const openscreen::cast::SimpleHistogram& histogram) const {
  // If there are no buckets then we return an empty list.
  if (histogram.buckets.empty()) {
    return base::Value::List();
  }

  base::Value::List histo;

  if (histogram.buckets.front()) {
    base::Value::Dict bucket;
    bucket.Set(base::StringPrintf("<%" PRId64, histogram.min),
               histogram.buckets.front());
    histo.Append(std::move(bucket));
  }

  for (std::size_t i = 1; i < histogram.buckets.size() - 1; ++i) {
    if (!histogram.buckets[i]) {
      continue;
    }
    base::Value::Dict bucket;
    int64_t lower = histogram.min + (i - 1) * histogram.width;
    int64_t upper = lower + histogram.width - 1;
    bucket.Set(base::StringPrintf("%" PRId64 "-%" PRId64, lower, upper),
               histogram.buckets[i]);
    histo.Append(std::move(bucket));
  }

  if (histogram.buckets.back()) {
    base::Value::Dict bucket;
    bucket.Set(base::StringPrintf(">=%" PRId64, histogram.max),
               histogram.buckets.back());
    histo.Append(std::move(bucket));
  }
  return histo;
}

void OpenscreenStatsClient::OnStatisticsUpdated(
    const openscreen::cast::SenderStats& updated_stats) {
  most_recent_stats_ = ConvertSenderStatsToDict(std::move(updated_stats));
}

}  // namespace mirroring
