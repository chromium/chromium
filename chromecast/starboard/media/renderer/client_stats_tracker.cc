// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/starboard/media/renderer/client_stats_tracker.h"

#include "base/check.h"
#include "base/logging.h"
#include "base/time/time.h"
#include "chromecast/base/metrics/cast_metrics_helper.h"

namespace chromecast {
namespace media {

constexpr base::TimeDelta kStatsUpdateFrequency = base::Seconds(5);
constexpr int kBitsPerByte = 8;

ClientStatsTracker::ClientStatsTracker(
    ::media::RendererClient* client,
    chromecast::metrics::CastMetricsHelper* cast_metrics_helper)
    : client_(client), cast_metrics_helper_(cast_metrics_helper) {
  CHECK(client_);
  CHECK(cast_metrics_helper_);
}

ClientStatsTracker::~ClientStatsTracker() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void ClientStatsTracker::UpdateStats(const StarboardPlayerInfo& player_info,
                                     const StarboardSampleInfo& sample_info) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (sample_info.type == StarboardMediaType::kStarboardMediaTypeAudio) {
    UpdateAudioStats(sample_info);
  } else if (sample_info.type == StarboardMediaType::kStarboardMediaTypeVideo) {
    UpdateVideoStats(player_info, sample_info);
  } else {
    LOG(ERROR) << "Unsupported starboard media type: " << sample_info.type;
  }
}

void ClientStatsTracker::UpdateAudioStats(
    const StarboardSampleInfo& sample_info) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // Per the documentation of RendererClient, *_decoded is a delta when passed
  // to OnStatisticsUpdate.
  pending_stats_.audio_bytes_decoded += sample_info.buffer_size;

  MaybeSendStatsUpdate();
}

void ClientStatsTracker::UpdateVideoStats(
    const StarboardPlayerInfo& player_info,
    const StarboardSampleInfo& sample_info) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // Per the documentation of RendererClient, *_decoded and *_dropped are
  // deltas when passed to OnStatisticsUpdate.
  ::media::PipelineStatistics stats;
  pending_stats_.video_bytes_decoded += sample_info.buffer_size;

  // The pending frame counts should be the delta from the last frame counts we
  // reported.
  pending_stats_.video_frames_decoded =
      player_info.total_video_frames -
      total_reported_stats_.video_frames_decoded;
  pending_stats_.video_frames_dropped =
      player_info.dropped_video_frames -
      total_reported_stats_.video_frames_dropped;

  MaybeSendStatsUpdate();
}

void ClientStatsTracker::MaybeSendStatsUpdate() {
  const base::TimeDelta elapsed_time = last_update_timer_.Elapsed();
  if (elapsed_time < kStatsUpdateFrequency) {
    return;
  }

  client_->OnStatisticsUpdate(pending_stats_);

  const int audio_bitrate_kbps = kBitsPerByte *
                                 pending_stats_.audio_bytes_decoded /
                                 elapsed_time.InMilliseconds();
  const int video_bitrate_kbps = kBitsPerByte *
                                 pending_stats_.video_bytes_decoded /
                                 elapsed_time.InMilliseconds();

  if (audio_bitrate_kbps > 0) {
    LOG(INFO) << "Estimated audio bitrate is " << audio_bitrate_kbps << " kbps";
    cast_metrics_helper_->RecordApplicationEventWithValue(
        "Cast.Platform.AudioBitrate", audio_bitrate_kbps);
  }
  if (video_bitrate_kbps > 0) {
    LOG(INFO) << "Estimated video bitrate is " << video_bitrate_kbps << " kbps";
    cast_metrics_helper_->RecordApplicationEventWithValue(
        "Cast.Platform.VideoBitrate", video_bitrate_kbps);
  }

  // Update total_reported_stats_ and reset pending_stats_.
  total_reported_stats_.video_bytes_decoded +=
      pending_stats_.video_bytes_decoded;
  total_reported_stats_.video_frames_decoded +=
      pending_stats_.video_frames_decoded;
  total_reported_stats_.video_frames_dropped +=
      pending_stats_.video_frames_dropped;
  total_reported_stats_.audio_bytes_decoded +=
      pending_stats_.audio_bytes_decoded;

  pending_stats_ = ::media::PipelineStatistics();

  // Reset the timer.
  last_update_timer_ = base::ElapsedTimer();
}

}  // namespace media
}  // namespace chromecast
