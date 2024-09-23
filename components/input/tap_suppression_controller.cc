// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/input/tap_suppression_controller.h"

#include "base/notreached.h"
#include "base/trace_event/trace_event.h"

namespace input {

TapSuppressionController::Config::Config()
    : enabled(false), max_cancel_to_down_time(base::Milliseconds(180)) {}

TapSuppressionController::TapSuppressionController(const Config& config)
    : state_(config.enabled ? NOTHING : DISABLED),
      max_cancel_to_down_time_(config.max_cancel_to_down_time) {}

TapSuppressionController::~TapSuppressionController() {}

void TapSuppressionController::GestureFlingCancelStoppedFling() {
  base::TimeTicks event_time = Now();
  switch (state_) {
    case DISABLED:
    case SUPPRESSING_TAPS:
      break;
    case NOTHING:
      fling_cancel_time_ = event_time;
      state_ = LAST_CANCEL_STOPPED_FLING;
      break;
    case LAST_CANCEL_STOPPED_FLING:
      break;
  }
}

bool TapSuppressionController::ShouldSuppressTapDown() {
  base::TimeTicks event_time = Now();
  switch (state_) {
    case DISABLED:
    case NOTHING:
      return false;
    case LAST_CANCEL_STOPPED_FLING:
      if ((event_time - fling_cancel_time_) < max_cancel_to_down_time_) {
        state_ = SUPPRESSING_TAPS;
        return true;
      } else {
        state_ = NOTHING;
        return false;
      }
    // Stop suppressing tap end events.
    case SUPPRESSING_TAPS:
      state_ = NOTHING;
      return false;
  }
  NOTREACHED_IN_MIGRATION() << "Invalid state";
  return false;
}

bool TapSuppressionController::ShouldSuppressTapEnd() {
  switch (state_) {
    case DISABLED:
    case NOTHING:
      return false;
    case LAST_CANCEL_STOPPED_FLING:
      return true;
    case SUPPRESSING_TAPS:
      return true;
  }
  return false;
}

base::TimeTicks TapSuppressionController::Now() {
  return base::TimeTicks::Now();
}

}  // namespace input
