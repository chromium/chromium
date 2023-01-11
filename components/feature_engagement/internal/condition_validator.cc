// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/feature_engagement/internal/condition_validator.h"

#include <ostream>

namespace feature_engagement {

ConditionValidator::Result::Result(bool initial_values)
    : event_model_ready_ok(initial_values),
      currently_showing_ok(initial_values),
      feature_enabled_ok(initial_values),
      config_ok(initial_values),
      used_ok(initial_values),
      trigger_ok(initial_values),
      preconditions_ok(initial_values),
      session_rate_ok(initial_values),
      availability_model_ready_ok(initial_values),
      availability_ok(initial_values),
      display_lock_ok(initial_values),
      snooze_expiration_ok(initial_values),
      priority_notification_ok(initial_values),
      groups_ok(initial_values),
      should_show_snooze(initial_values) {}

ConditionValidator::Result::Result(const Result& other) = default;

ConditionValidator::Result& ConditionValidator::Result::operator=(
    const Result& other) = default;

bool ConditionValidator::Result::NoErrors() const {
  return event_model_ready_ok && currently_showing_ok && feature_enabled_ok &&
         config_ok && used_ok && trigger_ok && preconditions_ok &&
         session_rate_ok && availability_model_ready_ok && availability_ok &&
         display_lock_ok && snooze_expiration_ok && priority_notification_ok &&
         groups_ok;
}

std::ostream& operator<<(std::ostream& os,
                         const ConditionValidator::Result& result) {
  return os << "{ event_model_ready_ok=" << result.event_model_ready_ok
            << ", currently_showing_ok=" << result.currently_showing_ok
            << ", feature_enabled_ok=" << result.feature_enabled_ok
            << ", config_ok=" << result.config_ok
            << ", used_ok=" << result.used_ok
            << ", trigger_ok=" << result.trigger_ok
            << ", preconditions_ok=" << result.preconditions_ok
            << ", session_rate_ok=" << result.session_rate_ok
            << ", availability_model_ready_ok="
            << result.availability_model_ready_ok
            << ", availability_ok=" << result.availability_ok
            << ", display_lock_ok=" << result.display_lock_ok
            << ", snooze_expiration_ok=" << result.snooze_expiration_ok
            << ", priority_notification_ok=" << result.priority_notification_ok
            << ", groups_ok=" << result.groups_ok
            << ", should_show_snooze=" << result.should_show_snooze << " }";
}

}  // namespace feature_engagement
