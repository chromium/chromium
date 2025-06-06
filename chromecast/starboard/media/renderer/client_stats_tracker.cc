// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/starboard/media/renderer/client_stats_tracker.h"

#include "base/check.h"
#include "base/logging.h"

namespace chromecast {
namespace media {

ClientStatsTracker::ClientStatsTracker(::media::RendererClient* client)
    : client_(client) {
  CHECK(client_);
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
  ::media::PipelineStatistics stats;
  stats.audio_bytes_decoded = sample_info.buffer_size;

  client_->OnStatisticsUpdate(stats);
}

void ClientStatsTracker::UpdateVideoStats(
    const StarboardPlayerInfo& player_info,
    const StarboardSampleInfo& sample_info) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // Per the documentation of RendererClient, *_decoded and *_dropped are
  // deltas when passed to OnStatisticsUpdate.
  ::media::PipelineStatistics stats;
  stats.video_bytes_decoded = sample_info.buffer_size;
  stats.video_frames_decoded =
      player_info.total_video_frames - total_video_frames_decoded_;
  stats.video_frames_dropped =
      player_info.dropped_video_frames - total_video_frames_dropped_;

  total_video_frames_decoded_ = player_info.total_video_frames;
  total_video_frames_dropped_ = player_info.dropped_video_frames;

  client_->OnStatisticsUpdate(stats);
}

}  // namespace media
}  // namespace chromecast
