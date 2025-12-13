// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/starboard/media/renderer/starboard_buffering_tracker.h"

#include <optional>
#include <utility>

#include "base/logging.h"
#include "chromecast/base/metrics/cast_metrics_helper.h"

namespace chromecast {
namespace media {

// If the "buffering time" is under this threshold, assume that it is simply a
// measuring error. We do not want to over-report buffering.
constexpr base::TimeDelta kBufferingLeniency = base::Milliseconds(500);

StarboardBufferingTracker::StarboardBufferingTracker(
    GetMediaTimeFn get_media_time,
    chromecast::metrics::CastMetricsHelper* metrics_helper)
    : get_media_time_(std::move(get_media_time)),
      metrics_helper_(metrics_helper) {
  CHECK(metrics_helper_ != nullptr);
}

StarboardBufferingTracker::~StarboardBufferingTracker() = default;

void StarboardBufferingTracker::OnPlayerStatus(
    chromecast::media::StarboardPlayerState state) {
  switch (state) {
    case StarboardPlayerState::kStarboardPlayerStatePrerolling: {
      // This counts as the start of initial buffering.
      prerolling_timer_.emplace();
      between_buffering_timer_ = std::nullopt;
      buffering_timer_ = std::nullopt;
      break;
    }
    case StarboardPlayerState::kStarboardPlayerStatePresenting: {
      // This counts as the end of initial buffering.
      if (!prerolling_timer_.has_value()) {
        LOG(ERROR)
            << "SbPlayer entered state kStarboardPlayerStatePresenting without "
               "first entering state kStarboardPlayerStatePrerolling.";
        break;
      }
      const base::TimeDelta preroll_duration = prerolling_timer_->Elapsed();
      LOG(INFO) << "Prerolling duration: " << preroll_duration;
      metrics_helper_->LogTimeToBufferAv(
          chromecast::metrics::CastMetricsHelper::kInitialBuffering,
          preroll_duration);
      metrics_helper_->RecordApplicationEventWithValue(
          "Cast.Platform.InitialBufferingTime",
          preroll_duration.InMilliseconds());
      prerolling_timer_ = std::nullopt;

      // Start tracking buffering after the initial buffering.
      buffering_timer_.emplace();
      between_buffering_timer_.emplace();
      media_time_ = get_media_time_.Run();
      break;
    }
    default: {
      break;
    }
  }
}

void StarboardBufferingTracker::OnBufferPush() {
  // This means the SbPlayer is not playing.
  if (!buffering_timer_.has_value() || playback_rate_ == 0.0) {
    return;
  }

  const base::TimeDelta current_media_time = get_media_time_.Run();

  if (current_media_time == media_time_) {
    // Buffering. Time has elapsed, but playback is not progressing. This will
    // be reported once media begins progressing.
    return;
  }

  const base::TimeDelta elapsed_real_time = buffering_timer_->Elapsed();
  const base::TimeDelta elapsed_media_time = current_media_time - media_time_;
  const base::TimeDelta adjusted_elapsed_media_time =
      elapsed_media_time / playback_rate_;

  const base::TimeDelta buffering_duration =
      elapsed_real_time - adjusted_elapsed_media_time;

  // Log a warning for large negative buffering durations.
  if (buffering_duration < base::Seconds(0) &&
      buffering_duration + kBufferingLeniency < base::Seconds(0)) {
    LOG(WARNING) << "Media is playing faster than expected. Elapsed real time: "
                 << elapsed_real_time
                 << ", elapsed media time (adjusted for playback rate="
                 << playback_rate_ << "): " << adjusted_elapsed_media_time
                 << ", difference: " << buffering_duration;
  }

  if (buffering_duration < kBufferingLeniency) {
    // Not buffering.
    buffering_timer_.emplace();
    media_time_ = current_media_time;
    return;
  }

  LOG(INFO) << "Buffering detected. Elapsed real time: " << elapsed_real_time
            << ", elapsed media time (adjusted for playback rate="
            << playback_rate_ << "): " << adjusted_elapsed_media_time
            << ", buffering duration: " << buffering_duration;

  metrics_helper_->LogTimeToBufferAv(
      chromecast::metrics::CastMetricsHelper::kBufferingAfterUnderrun,
      buffering_duration);
  metrics_helper_->RecordApplicationEventWithValue(
      "Cast.Platform.AutoPauseTime", buffering_duration.InMilliseconds());

  const base::TimeDelta time_between_buffering =
      between_buffering_timer_->Elapsed();
  LOG(INFO) << "Time since last buffering event: " << time_between_buffering;
  metrics_helper_->RecordApplicationEventWithValue(
      "Cast.Platform.PlayTimeBeforeAutoPause",
      time_between_buffering.InMilliseconds());

  // Reset the timers.
  buffering_timer_.emplace();
  between_buffering_timer_.emplace();
}

void StarboardBufferingTracker::SetPlaybackRate(double rate) {
  if (rate < 0.0) {
    LOG(ERROR) << "Invalid playback rate: " << rate;
    rate = 0.0;
  }

  if (playback_rate_ == 0.0 && rate > 0.0) {
    // Plabyack is starting after a pause. Reset the timer.
    buffering_timer_.emplace();
  }
  playback_rate_ = rate;
}

}  // namespace media
}  // namespace chromecast
