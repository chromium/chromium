// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/service/frame_sinks/frame_sink_throttler.h"

#include <algorithm>

#include "components/viz/common/constants.h"
#include "components/viz/common/frame_sinks/begin_frame_args.h"
#include "media/filters/video_cadence_estimator.h"

namespace viz {

void FrameSinkThrottler::SetThrottleInterval(base::TimeDelta interval) {
  if (throttle_interval_ == interval) {
    return;
  }
  throttle_interval_ = interval;
  UpdateBeginFrameInterval();
}

void FrameSinkThrottler::SetAllowThrottling(bool allowed) {
  if (throttling_allowed_ == allowed) {
    return;
  }
  throttling_allowed_ = allowed;
  UpdateBeginFrameInterval();
}

void FrameSinkThrottler::SetCadenceThrottleInterval(base::TimeDelta interval) {
  if (cadence_throttle_interval_ == interval) {
    return;
  }
  cadence_throttle_interval_ = interval;
  UpdateBeginFrameInterval();
}

void FrameSinkThrottler::SetThrottledDueToInteraction(bool throttled) {
  if (throttled_due_to_interaction_ == throttled) {
    return;
  }
  throttled_due_to_interaction_ = throttled;
  UpdateBeginFrameInterval();
}

void FrameSinkThrottler::SetLastKnownVsync(
    base::TimeDelta interval,
    base::TimeDelta unthrottled_interval) {
  if (last_known_vsync_interval_ == interval &&
      last_known_vsync_unthrottled_interval_ == unthrottled_interval) {
    return;
  }
  last_known_vsync_interval_ = interval;
  last_known_vsync_unthrottled_interval_ = unthrottled_interval;

  UpdateBeginFrameInterval();
}

void FrameSinkThrottler::UpdateBeginFrameInterval() {
  if (!throttling_allowed_) {
    begin_frame_interval_ = base::TimeDelta();
    return;
  }

  // Fall back to explicitly-requested throttle interval if neither
  // simple cadence throttling nor interaction throttling is
  // possible.
  base::TimeDelta interval = throttle_interval_;

  if (IsThrottledBySimpleCadence()) {
    // TODO(crbug.com/477624461): Re-evaluate if simple cadence should take
    // priority over background/battery throttling to prevent video stuttering.
    //
    // For now, simple cadence wins to avoid stuttering.
    interval = cadence_throttle_interval_;
  } else if (throttled_due_to_interaction_) {
    // Halve the framerate of the last known vsync interval
    constexpr int kInteractiveThrottleScalar = 2;
    base::TimeDelta vsync_interval = BeginFrameArgs::DefaultInterval();

    // If the unthrottled vsync interval is known, use it instead of the
    // default.
    if (last_known_vsync_unthrottled_interval_.is_positive()) {
      vsync_interval = last_known_vsync_unthrottled_interval_;
    }

    // Longest interval wins
    interval = std::max(interval, vsync_interval * kInteractiveThrottleScalar);
  }

  begin_frame_interval_ = interval;
}

bool FrameSinkThrottler::IsThrottledBySimpleCadence() const {
  if (!cadence_throttle_interval_.is_positive()) {
    return false;
  }

  // If we don't know the vsync interval we cannot check for simple cadence,
  // so we just assume it is safe to throttle.
  if (!last_known_vsync_interval_.is_positive()) {
    return true;
  }

  return media::VideoCadenceEstimator::HasSimpleCadence(
      last_known_vsync_interval_, cadence_throttle_interval_,
      kMaxTimeUntilNextGlitch);
}

}  // namespace viz
