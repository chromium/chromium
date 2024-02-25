// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/feature_engagement/internal/once_condition_validator.h"

#include <optional>

#include "components/feature_engagement/internal/event_model.h"
#include "components/feature_engagement/internal/time_provider.h"
#include "components/feature_engagement/public/configuration.h"

namespace feature_engagement {

OnceConditionValidator::OnceConditionValidator() = default;

OnceConditionValidator::~OnceConditionValidator() = default;

ConditionValidator::Result OnceConditionValidator::MeetsConditions(
    const base::Feature& feature,
    const FeatureConfig& config,
    const std::vector<GroupConfig>& group_configs,
    const EventModel& event_model,
    const AvailabilityModel& availability_model,
    const DisplayLockController& display_lock_controller,
    const Configuration* configuration,
    const TimeProvider& time_provider) const {
  ConditionValidator::Result result(true);
  result.event_model_ready_ok = event_model.IsReady();

  result.currently_showing_ok =
      allows_multiple_features_ || currently_showing_features_.empty();

  result.config_ok = config.valid;

  result.trigger_ok =
      shown_features_.find(feature.name) == shown_features_.end();
  result.session_rate_ok =
      shown_features_.find(feature.name) == shown_features_.end();

  result.snooze_expiration_ok =
      !event_model.IsSnoozeDismissed(config.trigger.name) &&
      (event_model.GetLastSnoozeTimestamp(config.trigger.name) <
       time_provider.Now() - base::Days(config.snooze_params.snooze_interval));

  result.priority_notification_ok =
      !pending_priority_notification_.has_value() ||
      pending_priority_notification_.value() == feature.name;

  result.should_show_snooze =
      result.snooze_expiration_ok &&
      event_model.GetSnoozeCount(config.trigger.name, config.trigger.window,
                                 time_provider.GetCurrentDay()) <
          config.snooze_params.max_limit;

  return result;
}

void OnceConditionValidator::NotifyIsShowing(
    const base::Feature& feature,
    const FeatureConfig& config,
    const std::vector<std::string>& all_feature_names) {
  DCHECK(allows_multiple_features_ || currently_showing_features_.empty());
  DCHECK(currently_showing_features_.find(feature.name) ==
         currently_showing_features_.end());
  DCHECK(shown_features_.find(feature.name) == shown_features_.end());
  shown_features_.insert(feature.name);
  currently_showing_features_.insert(feature.name);
}

void OnceConditionValidator::NotifyDismissed(const base::Feature& feature) {
  DCHECK(currently_showing_features_.find(feature.name) !=
         currently_showing_features_.end());
  currently_showing_features_.erase(feature.name);
}

void OnceConditionValidator::SetPriorityNotification(
    const std::optional<std::string>& feature) {
  pending_priority_notification_ = feature;
}

std::optional<std::string>
OnceConditionValidator::GetPendingPriorityNotification() {
  return pending_priority_notification_;
}

void OnceConditionValidator::AllowMultipleFeaturesForTesting(
    bool allows_multiple_features) {
  allows_multiple_features_ = allows_multiple_features;
}

void OnceConditionValidator::ResetSession() {
  shown_features_.clear();
}

}  // namespace feature_engagement
