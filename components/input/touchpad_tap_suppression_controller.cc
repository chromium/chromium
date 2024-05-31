// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/input/touchpad_tap_suppression_controller.h"

namespace input {

TouchpadTapSuppressionController::TouchpadTapSuppressionController(
    const TapSuppressionController::Config& config)
    : TapSuppressionController(config) {}

TouchpadTapSuppressionController::~TouchpadTapSuppressionController() {}

bool TouchpadTapSuppressionController::ShouldSuppressMouseDown(
    const MouseEventWithLatencyInfo& event) {
  return ShouldSuppressTapDown();
}

bool TouchpadTapSuppressionController::ShouldSuppressMouseUp() {
  return ShouldSuppressTapEnd();
}

}  // namespace input
