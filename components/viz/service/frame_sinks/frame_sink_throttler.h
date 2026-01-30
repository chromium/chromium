// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIZ_SERVICE_FRAME_SINKS_FRAME_SINK_THROTTLER_H_
#define COMPONENTS_VIZ_SERVICE_FRAME_SINKS_FRAME_SINK_THROTTLER_H_

#include "base/time/time.h"
#include "components/viz/service/viz_service_export.h"

namespace viz {

// Helper class to manage the throttling state for a frame sink. It calculates
// the final begin frame interval based on multiple throttling signals.
//
// This helper protects the invariance of the throttling state by
// encapsulating all inputs and ensuring the computed begin frame interval is
// always updated in sync with any changes, preventing it from being modified
// inconsistently.
class VIZ_SERVICE_EXPORT FrameSinkThrottler {
 public:
  FrameSinkThrottler() = default;
  ~FrameSinkThrottler() = default;

  // Sets a throttling interval to be used. If |interval| is zero, any
  // previously set interval is cleared. Note that the frame sink may still be
  // throttled by other signals (e.g. cadence) unless SetAllowThrottling(false)
  // is called.
  void SetThrottleInterval(base::TimeDelta interval);

  // Disabling throttling will force the begin frame interval to be
  // base::TimeDelta() regardless of any other throttling signals (e.g. cadence,
  // background state, etc.). This is typically used for scenarios like screen
  // capture where high fidelity is required.
  void SetAllowThrottling(bool allowed);

  // Sets the simple cadence the frame sink is allowed to throttle to. This will
  // only be applied if the provided cadence is evenly divisible by the current
  // framerate. For example a 60hz screen will be allowed to throttle to 30hz,
  // but a 144hz screen will not be allowed to throttle as this would introduce
  // stutter.
  void SetCadenceThrottleInterval(base::TimeDelta interval);

  // Sets whether the frame sink should be throttled due to user interaction
  // with another frame sink.
  void SetThrottledDueToInteraction(bool throttled);

  // Sets the last known vsync interval, used to calculate simple cadence.
  void SetLastKnownVsync(base::TimeDelta interval,
                         base::TimeDelta unthrottled_interval);

  // Returns the calculated begin frame interval.
  base::TimeDelta begin_frame_interval() const { return begin_frame_interval_; }

  bool throttling_allowed() const { return throttling_allowed_; }

 private:
  friend class FrameSinkThrottlerTest;
  void UpdateBeginFrameInterval();

  // Returns true if the current begin frame interval is throttled by a simple
  // cadence. A simple cadence means the throttled interval is an integer
  // multiple of the display refresh rate.
  bool IsThrottledBySimpleCadence() const;

  // Explicitly-set throttle_interval, used unless the interaction throttling
  // results in a larger value or cadence throttling is possible.
  base::TimeDelta throttle_interval_;

  // If false, the begin frame interval will be base::TimeDelta() regardless of
  // any other throttling.
  bool throttling_allowed_ = true;

  // The interval that this frame sink will be throttled to if it's
  // possible to do so with a simple cadence. A simple cadence means that we
  // can go from the original frame rate to a target frame rate without
  // introducing stutter because the original frame rate is an integer
  // multiple of the target frame rate.
  //
  // For example, throttling from 60hz to 30hz works while throttling from 60hz
  // to 24hz does not.
  base::TimeDelta cadence_throttle_interval_;

  // True if another frame sink is being interacted with. If so this frame sink
  // will throttle to prioritize giving resources to the interactive frame sink.
  bool throttled_due_to_interaction_ = false;

  // If non-zero, this represents the duration of time in between sending two
  // consecutive frames. If zero, no throttling would be applied.
  base::TimeDelta begin_frame_interval_;

  // The interval of the last vsync. If unknown, the value is 0.
  base::TimeDelta last_known_vsync_interval_;

  // The unthrottled interval of the last vsync, which is the shortest
  // interval possible for the current display.
  base::TimeDelta last_known_vsync_unthrottled_interval_;
};

}  // namespace viz

#endif  // COMPONENTS_VIZ_SERVICE_FRAME_SINKS_FRAME_SINK_THROTTLER_H_
