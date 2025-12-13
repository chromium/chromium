// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_STARBOARD_MEDIA_RENDERER_STARBOARD_BUFFERING_TRACKER_H_
#define CHROMECAST_STARBOARD_MEDIA_RENDERER_STARBOARD_BUFFERING_TRACKER_H_

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/time/time.h"
#include "base/timer/elapsed_timer.h"
#include "chromecast/starboard/media/media/starboard_api_wrapper.h"

namespace chromecast {

namespace metrics {
class CastMetricsHelper;
}  // namespace metrics

namespace media {

// Tracks when SbPlayer is buffering, and updates Cast metrics accordingly.
//
// An instance of this class should only be used on a single sequence.
class StarboardBufferingTracker {
 public:
  using GetMediaTimeFn = base::RepeatingCallback<base::TimeDelta()>;

  // `get_media_time` must return the current media time when called. It must be
  // valid to call this function at any time.
  //
  // `metrics_helper` must outlive this object, and cannot be null.
  StarboardBufferingTracker(
      GetMediaTimeFn get_media_time,
      chromecast::metrics::CastMetricsHelper* metrics_helper);

  // Disallow copy and assign.
  StarboardBufferingTracker(const StarboardBufferingTracker&) = delete;
  StarboardBufferingTracker& operator=(const StarboardBufferingTracker&) =
      delete;

  ~StarboardBufferingTracker();

  // This must be called when the SbPlayer's state changes.
  void OnPlayerStatus(StarboardPlayerState state);

  // This must be called whenever a buffer is pushed to starboard.
  void OnBufferPush();

  // This must be called when the SbPlayer's playback rate changes.
  void SetPlaybackRate(double rate);

 private:
  GetMediaTimeFn get_media_time_;
  base::TimeDelta media_time_ = base::Seconds(0);
  double playback_rate_ = 1.0;
  raw_ptr<chromecast::metrics::CastMetricsHelper> metrics_helper_;

  // Tracks the duration of prerolling (initial buffering).
  std::optional<base::ElapsedTimer> prerolling_timer_;
  // Tracks the duration of buffering AFTER the initial buffering.
  std::optional<base::ElapsedTimer> buffering_timer_;
  // Tracks the duration between buffering events.
  std::optional<base::ElapsedTimer> between_buffering_timer_;
};

}  // namespace media
}  // namespace chromecast

#endif  // CHROMECAST_STARBOARD_MEDIA_RENDERER_STARBOARD_BUFFERING_TRACKER_H_
