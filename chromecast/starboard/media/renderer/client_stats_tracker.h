// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_STARBOARD_MEDIA_RENDERER_CLIENT_STATS_TRACKER_H_
#define CHROMECAST_STARBOARD_MEDIA_RENDERER_CLIENT_STATS_TRACKER_H_

#include <cstdint>

#include "base/memory/raw_ptr.h"
#include "base/sequence_checker.h"
#include "chromecast/starboard/media/media/starboard_api_wrapper.h"
#include "media/base/renderer_client.h"

namespace chromecast {
namespace media {

// Tracks media stats and reports them to a RendererClient.
//
// This class is not threadsafe, and must only be used on a single sequence.
class ClientStatsTracker {
 public:
  // `client` must not be null.
  explicit ClientStatsTracker(::media::RendererClient* client);
  ~ClientStatsTracker();

  // Updates stats based on a buffer pushed to starboard.
  void UpdateStats(const StarboardPlayerInfo& player_info,
                   const StarboardSampleInfo& sample_info);

 private:
  // Updates stats for an audio buffer.
  void UpdateAudioStats(const StarboardSampleInfo& sample_info);

  // Updates stats for a video buffer.
  void UpdateVideoStats(const StarboardPlayerInfo& player_info,
                        const StarboardSampleInfo& sample_info);

  SEQUENCE_CHECKER(sequence_checker_);

  raw_ptr<::media::RendererClient> client_ = nullptr;
  uint32_t total_video_frames_decoded_ = 0;
  uint32_t total_video_frames_dropped_ = 0;
};

}  // namespace media
}  // namespace chromecast

#endif  // CHROMECAST_STARBOARD_MEDIA_RENDERER_CLIENT_STATS_TRACKER_H_
