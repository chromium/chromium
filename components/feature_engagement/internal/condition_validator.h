// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_FEATURE_ENGAGEMENT_INTERNAL_CONDITION_VALIDATOR_H_
#define COMPONENTS_FEATURE_ENGAGEMENT_INTERNAL_CONDITION_VALIDATOR_H_

#include <stdint.h>

#include <ostream>
#include <string>
#include <vector>

#include "base/macros.h"
#include "components/feature_engagement/public/feature_list.h"

namespace base {
struct Feature;
}  // namespace base

namespace feature_engagement {
struct FeatureConfig;
class AvailabilityModel;
class DisplayLockController;
class EventModel;

// A ConditionValidator checks the requred conditions for a given feature,
// and checks if all conditions are met.
class ConditionValidator {
 public:
  // The Result struct is used to categorize everything that could have the
  // wrong state. By returning an instance of this where every value is true
  // from MeetsConditions(...), it can be assumed that in-product help will
  // be displayed.
  struct Result {
    explicit Result(bool initial_values);
    Result(const Result& other);
    Result& operator=(const Result& other);

    // Whether the event model was ready.
    bool event_model_ready_ok;

    // Whether no other in-product helps were shown at the time.
    bool currently_showing_ok;

    // Whether the feature is enabled.
    bool feature_enabled_ok;

    // Whether the feature configuration was valid.
    bool config_ok;

    // Whether the used precondition was met.
    bool used_ok;

    // Whether the trigger precondition was met.
    bool trigger_ok;

    // Whether the other preconditions were met.
    bool preconditions_ok;

    // Whether the session rate precondition was met.
    bool session_rate_ok;

    // Whether the availability model was ready.
    bool availability_model_ready_ok;

    // Whether the availability precondition was met.
    bool availability_ok;

    // Whether there are currently held display locks.
    bool display_lock_ok;

    // Whether the current snooze timer has expired.
    bool snooze_expiration_ok;

    // Whether the snooze option should be shown.
    // This value is excluded from the NoErrors() check.
    bool should_show_snooze;

    // Returns true if this result object has no errors, i.e. no values that
    // are false.
    bool NoErrors() const;
  };

  ConditionValidator(const ConditionValidator&) = delete;
  ConditionValidator& operator=(const ConditionValidator&) = delete;

  virtual ~ConditionValidator() = default;

  // Returns a Result object that describes whether each condition has been met.
  virtual Result MeetsConditions(
      const base::Feature& feature,
      const FeatureConfig& config,
      const EventModel& event_model,
      const AvailabilityModel& availability_model,
      const DisplayLockController& display_lock_controller,
      uint32_t current_day) const = 0;

  // Must be called to notify that the |feature| is currently showing.
  virtual void NotifyIsShowing(
      const base::Feature& feature,
      const FeatureConfig& config,
      const std::vector<std::string>& all_feature_names) = 0;

  // Must be called to notify that the |feature| is no longer showing.
  virtual void NotifyDismissed(const base::Feature& feature) = 0;

 protected:
  ConditionValidator() = default;
};

std::ostream& operator<<(std::ostream& os,
                         const ConditionValidator::Result& result);

}  // namespace feature_engagement

#endif  // COMPONENTS_FEATURE_ENGAGEMENT_INTERNAL_CONDITION_VALIDATOR_H_
