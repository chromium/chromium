// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_STARBOARD_MEDIA_RENDERER_CLIENT_STATS_TRACKER_H_
#define CHROMECAST_STARBOARD_MEDIA_RENDERER_CLIENT_STATS_TRACKER_H_

#include <cstdint>

#include "base/memory/raw_ptr.h"
#include "base/sequence_checker.h"
#include "base/timer/elapsed_timer.h"
#include "chromecast/starboard/media/media/starboard_api_wrapper.h"
#include "media/base/pipeline_status.h"
#include "media/base/renderer_client.h"

namespace chromecast {

namespace metrics {
class CastMetricsHelper;
}  // namespace metrics

namespace media {

// Tracks media stats and reports them to a RendererClient.
//
// In order to prevent flooding logs, this class is rate limited to once every
// 5s at most. In other words, if UpdateStats is called, subsequent calls within
// the next 5s will be ignored (though stats will be tracked internally).
//
// 5s was chosen to match the logic of CMA:
// https://source.chromium.org/chromium/chromium/src/+/main:chromecast/media/cma/pipeline/media_pipeline_impl.cc;l=494;drc=3dd1b27a7cb34cc30ee4d8ddc2146972b5254201
//
// This class is not threadsafe, and must only be used on a single sequence.
class ClientStatsTracker {
 public:
  // `client` and `cast_metrics_helper` must not be null.
  explicit ClientStatsTracker(
      ::media::RendererClient* client,
      chromecast::metrics::CastMetricsHelper* cast_metrics_helper);
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

  // If enough time has passed, notifies clients of new stats and resets the
  // timer. Otherwise, this is a no-op.
  void MaybeSendStatsUpdate();

  SEQUENCE_CHECKER(sequence_checker_);

  raw_ptr<::media::RendererClient> client_ = nullptr;
  raw_ptr<chromecast::metrics::CastMetricsHelper> cast_metrics_helper_ =
      nullptr;

  // These stats track values that we have already reported.
  ::media::PipelineStatistics total_reported_stats_;
  // These stats track values that we have not yet reported, due to rate
  // limiting.
  ::media::PipelineStatistics pending_stats_;

  // Tracks the amount of time that has elapsed since our last update to client_
  // and cast_metrics_helper_. Used to rate-limit updates.
  base::ElapsedTimer last_update_timer_;
};

}  // namespace media
}  // namespace chromecast

#endif  // CHROMECAST_STARBOARD_MEDIA_RENDERER_CLIENT_STATS_TRACKER_H_
