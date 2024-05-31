// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_INPUT_TOUCHSCREEN_TAP_SUPPRESSION_CONTROLLER_H_
#define COMPONENTS_INPUT_TOUCHSCREEN_TAP_SUPPRESSION_CONTROLLER_H_

#include "components/input/event_with_latency_info.h"
#include "components/input/tap_suppression_controller.h"
#include "base/component_export.h"

namespace input {

// Controls the suppression of touchscreen taps immediately following the
// dispatch of a GestureFlingCancel event.
class COMPONENT_EXPORT(INPUT) TouchscreenTapSuppressionController :
    public TapSuppressionController {
 public:
  TouchscreenTapSuppressionController(
      const TapSuppressionController::Config& config);

  TouchscreenTapSuppressionController(
      const TouchscreenTapSuppressionController&) = delete;
  TouchscreenTapSuppressionController& operator=(
      const TouchscreenTapSuppressionController&) = delete;

  ~TouchscreenTapSuppressionController() override;

  // Should be called on arrival of any tap-related events. Returns true if the
  // caller should stop normal handling of the gesture.
  bool FilterTapEvent(const GestureEventWithLatencyInfo& event);
};

}  // namespace input

#endif  // COMPONENTS_INPUT_TOUCHSCREEN_TAP_SUPPRESSION_CONTROLLER_H_
