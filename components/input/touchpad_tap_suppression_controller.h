// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_INPUT_TOUCHPAD_TAP_SUPPRESSION_CONTROLLER_H_
#define COMPONENTS_INPUT_TOUCHPAD_TAP_SUPPRESSION_CONTROLLER_H_

#include "components/input/event_with_latency_info.h"
#include "components/input/tap_suppression_controller.h"
#include "third_party/blink/public/common/input/web_input_event.h"
#include "base/component_export.h"

namespace input {

// Controls the suppression of touchpad taps immediately following the dispatch
// of a GestureFlingCancel event.
class COMPONENT_EXPORT(INPUT) TouchpadTapSuppressionController :
    public TapSuppressionController {
 public:
  // The |client| must outlive the TouchpadTapSupressionController.
  TouchpadTapSuppressionController(
      const TapSuppressionController::Config& config);

  TouchpadTapSuppressionController(const TouchpadTapSuppressionController&) =
      delete;
  TouchpadTapSuppressionController& operator=(
      const TouchpadTapSuppressionController&) = delete;

  ~TouchpadTapSuppressionController() override;

  // Should be called on arrival of MouseDown events. Returns true if the caller
  // should stop normal handling of the MouseDown.
  bool ShouldSuppressMouseDown(const MouseEventWithLatencyInfo& event);

  // Should be called on arrival of MouseUp events. Returns true if the caller
  // should stop normal handling of the MouseUp.
  bool ShouldSuppressMouseUp();

 private:
  friend class MockRenderWidgetHost;
};

}  // namespace input

#endif  // COMPONENTS_INPUT_TOUCHPAD_TAP_SUPPRESSION_CONTROLLER_H_
