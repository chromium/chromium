// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/installer/util/experiment_metrics.h"

namespace installer {

bool ExperimentMetrics::InInitialState() const {
  return state < kGroupAssigned;
}

bool ExperimentMetrics::InTerminalState() const {
  static_assert(NUM_STATES == 18,
                "update the list of terminal states when adding a new state.");
  return state == kSelectedNoThanks || state == kSelectedOpenChromeAndCrash ||
         state == kSelectedOpenChromeAndNoCrash || state == kSelectedClose ||
         state == kUserLogOff || state == kOtherLaunch || state == kOtherClose;
}

bool ExperimentMetrics::operator==(const ExperimentMetrics& other) const {
  return group == other.group && state == other.state &&
         toast_location == other.toast_location &&
         toast_count == other.toast_count &&
         first_toast_offset_days == other.first_toast_offset_days &&
         toast_hour == other.toast_hour &&
         last_used_bucket == other.last_used_bucket &&
         action_delay_bucket == other.action_delay_bucket &&
         session_length_bucket == other.session_length_bucket;
}

}  // namespace installer
