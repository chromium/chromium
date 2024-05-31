// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_INPUT_TAP_SUPPRESSION_CONTROLLER_H_
#define COMPONENTS_INPUT_TAP_SUPPRESSION_CONTROLLER_H_

#include "base/time/time.h"
#include "base/timer/timer.h"
#include "base/component_export.h"

namespace input {

// The core controller for suppression of taps (touchpad or touchscreen)
// immediately following a GestureFlingCancel event (caused by the same tap).
// Only taps of sufficient speed and within a specified time window after a
// GestureFlingCancel are suppressed.
class COMPONENT_EXPORT(INPUT) TapSuppressionController {
 public:
  struct COMPONENT_EXPORT(INPUT) Config {
    Config();

    // Defaults to false, in which case no suppression is performed.
    bool enabled;

    // The maximum time allowed between a GestureFlingCancel and its
    // corresponding tap down.
    base::TimeDelta max_cancel_to_down_time;
  };

  TapSuppressionController(const Config& config);

  TapSuppressionController(const TapSuppressionController&) = delete;
  TapSuppressionController& operator=(const TapSuppressionController&) = delete;

  virtual ~TapSuppressionController();

  // Should be called whenever a GestureFlingCancel actually stopped a fling and
  // therefore the controller should suppress the forwarding of the following
  // tap.
  void GestureFlingCancelStoppedFling();

  // Should be called whenever a tap down (touchpad or touchscreen) is received.
  // Returns true if the tap down should be suppressed.
  bool ShouldSuppressTapDown();

  // Should be called whenever a tap ending event is received. Returns true if
  // the tap event should be suppressed.
  bool ShouldSuppressTapEnd();

 protected:
  virtual base::TimeTicks Now();

 private:
  friend class MockTapSuppressionController;

  enum State {
    DISABLED,
    NOTHING,
    LAST_CANCEL_STOPPED_FLING,
    // When the stashed TapDown event is dropped, the controller enters the
    // SUPPRESSING_TAPS state. This state shows that the controller will
    // suppress LongTap, TwoFingerTap, and TapCancel gesture events until the
    // next tapDown event arrives.
    SUPPRESSING_TAPS,
  };

  State state_;

  base::TimeDelta max_cancel_to_down_time_;

  // TODO(rjkroege): During debugging, the event times did not prove reliable.
  // Replace the use of base::TimeTicks with an accurate event time when they
  // become available post http://crbug.com/119556.
  base::TimeTicks fling_cancel_time_;
};

}  // namespace input

#endif  // COMPONENTS_INPUT_TAP_SUPPRESSION_CONTROLLER_H_
