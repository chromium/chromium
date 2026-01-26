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

  // Sets the last known vsync interval, used to calculate simple cadence.
  void SetLastKnownVsync(base::TimeDelta interval,
                         base::TimeDelta unthrottled_interval);

  // Returns the calculated begin frame interval.
  base::TimeDelta begin_frame_interval() const { return begin_frame_interval_; }

  bool throttling_allowed() const { return throttling_allowed_; }

  // Returns true if the current begin frame interval is throttled by a simple
  // cadence. A simple cadence means the throttled interval is a perfect
  // multiple of the display refresh rate.
  bool IsThrottledBySimpleCadence() const;

 private:
  void UpdateBeginFrameInterval();

  // The interval to which this frame sink will be throttled to. This will be
  // used unless the interaction throttling results in a larger value, in
  // which case that will be used instead.
  base::TimeDelta throttle_interval_;

  // If false, the begin frame interval will be base::TimeDelta() regardless of
  // any other throttling.
  bool throttling_allowed_ = true;

  // The interval to which this frame sink will be throttled to if it's
  // possible to do it with a simple cadence. A simple cadence means that we
  // can go from the original frame rate to a target frame rate without
  // introducing stutter. For example we can go from 60hz to 30hz without
  // issues, but going from 60hz to 24hz would cause a problem as it does not
  // evenly divide.
  base::TimeDelta cadence_throttle_interval_;

  // This value represents throttling on sending a BeginFrame. If non-zero, it
  // represents the duration of time in between sending two consecutive
  // frames. If zero, no throttling would be applied.
  base::TimeDelta begin_frame_interval_;

  // The interval of the last vsync - potentially this value is unknown & if
  // that is the case this value will be zero.
  base::TimeDelta last_known_vsync_interval_;

  // The unthrottled interval of the last vsync - which is the shortest
  // interval possible for the current display
  base::TimeDelta last_known_vsync_unthrottled_interval_;
};

}  // namespace viz

#endif  // COMPONENTS_VIZ_SERVICE_FRAME_SINKS_FRAME_SINK_THROTTLER_H_
