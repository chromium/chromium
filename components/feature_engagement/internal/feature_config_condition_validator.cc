// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/feature_engagement/internal/feature_config_condition_validator.h"

#include <algorithm>
#include <optional>
#include <string>
#include <vector>

#include "base/check.h"
#include "base/containers/contains.h"
#include "base/feature_list.h"
#include "base/notreached.h"
#include "components/feature_engagement/internal/availability_model.h"
#include "components/feature_engagement/internal/display_lock_controller.h"
#include "components/feature_engagement/internal/event_model.h"
#include "components/feature_engagement/internal/proto/feature_event.pb.h"
#include "components/feature_engagement/internal/time_provider.h"
#include "components/feature_engagement/public/configuration.h"
#include "components/feature_engagement/public/feature_list.h"

namespace feature_engagement {

FeatureConfigConditionValidator::FeatureConfigConditionValidator() = default;

FeatureConfigConditionValidator::~FeatureConfigConditionValidator() = default;

ConditionValidator::Result FeatureConfigConditionValidator::MeetsConditions(
    const base::Feature& feature,
    const FeatureConfig& config,
    const std::vector<GroupConfig>& group_configs,
    const EventModel& event_model,
    const AvailabilityModel& availability_model,
    const DisplayLockController& display_lock_controller,
    const Configuration* configuration,
    const TimeProvider& time_provider) const {
  uint32_t current_day = time_provider.GetCurrentDay();
  ConditionValidator::Result result(true);
  result.event_model_ready_ok = event_model.IsReady();
  result.currently_showing_ok = !IsBlocked(feature, config, configuration);
  result.feature_enabled_ok = base::FeatureList::IsEnabled(feature);
  result.config_ok = config.valid;
  result.used_ok =
      EventConfigMeetsConditions(config.used, event_model, current_day);
  result.trigger_ok =
      EventConfigMeetsConditions(config.trigger, event_model, current_day);

  for (const auto& event_config : config.event_configs) {
    result.preconditions_ok &=
        EventConfigMeetsConditions(event_config, event_model, current_day);
  }

  result.session_rate_ok =
      SessionRateMeetsConditions(config.session_rate, feature);

  result.availability_model_ready_ok = availability_model.IsReady();

  result.availability_ok = AvailabilityMeetsConditions(
      feature, config.availability, availability_model, current_day);

  result.display_lock_ok = !display_lock_controller.IsDisplayLocked();

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
                                 current_day) < config.snooze_params.max_limit;

  // Add on group additions
  for (const auto& group_config : group_configs) {
    bool valid = group_config.valid;
    result.config_ok &= valid;
    result.groups_ok &= valid;

    bool trigger_ok = EventConfigMeetsConditions(group_config.trigger,
                                                 event_model, current_day);
    result.trigger_ok &= trigger_ok;
    result.groups_ok &= trigger_ok;

    for (const auto& event_config : group_config.event_configs) {
      bool precondition_ok =
          EventConfigMeetsConditions(event_config, event_model, current_day);
      result.preconditions_ok &= precondition_ok;
      result.groups_ok &= precondition_ok;
    }

    bool session_rate_ok =
        SessionRateMeetsConditions(group_config.session_rate, feature);
    result.session_rate_ok &= session_rate_ok;
    result.groups_ok &= session_rate_ok;
  }

  return result;
}

void FeatureConfigConditionValidator::NotifyIsShowing(
    const base::Feature& feature,
    const FeatureConfig& config,
    const std::vector<std::string>& all_feature_names) {
  DCHECK(base::FeatureList::IsEnabled(feature));

  currently_showing_features_.insert(feature.name);

  switch (config.session_rate_impact.type) {
    case SessionRateImpact::Type::ALL:
      for (const std::string& feature_name : all_feature_names)
        ++times_shown_for_feature_[feature_name];
      break;
    case SessionRateImpact::Type::NONE:
      // Intentionally ignore, since no features should be impacted.
      break;
    case SessionRateImpact::Type::EXPLICIT:
      DCHECK(config.session_rate_impact.affected_features.has_value());
      for (const std::string& feature_name :
           config.session_rate_impact.affected_features.value()) {
        DCHECK(base::Contains(all_feature_names, feature_name));
        ++times_shown_for_feature_[feature_name];
      }
      break;
    default:
      // All cases should be covered.
      NOTREACHED_IN_MIGRATION();
  }
}

void FeatureConfigConditionValidator::NotifyDismissed(
    const base::Feature& feature) {
  currently_showing_features_.erase(feature.name);
}

bool FeatureConfigConditionValidator::EventConfigMeetsConditions(
    const EventConfig& event_config,
    const EventModel& event_model,
    uint32_t current_day) const {
  uint32_t event_count = event_model.GetEventCount(
      event_config.name, current_day, event_config.window);
  return event_config.comparator.MeetsCriteria(event_count);
}

void FeatureConfigConditionValidator::SetPriorityNotification(
    const std::optional<std::string>& feature) {
  DCHECK(!pending_priority_notification_.has_value() || !feature.has_value());
  pending_priority_notification_ = feature;
}

std::optional<std::string>
FeatureConfigConditionValidator::GetPendingPriorityNotification() {
  return pending_priority_notification_;
}

void FeatureConfigConditionValidator::ResetSession() {
  times_shown_for_feature_.clear();
}

bool FeatureConfigConditionValidator::AvailabilityMeetsConditions(
    const base::Feature& feature,
    Comparator comparator,
    const AvailabilityModel& availability_model,
    uint32_t current_day) const {
  if (comparator.type == ANY)
    return true;

  std::optional<uint32_t> availability_day =
      availability_model.GetAvailability(feature);
  if (!availability_day.has_value())
    return false;

  uint32_t days_available = current_day - availability_day.value();

  // Ensure that availability days never wrap around.
  if (availability_day.value() > current_day)
    days_available = 0u;

  return comparator.MeetsCriteria(days_available);
}

bool FeatureConfigConditionValidator::SessionRateMeetsConditions(
    const Comparator session_rate,
    const base::Feature& feature) const {
  const auto it = times_shown_for_feature_.find(feature.name);
  if (it == times_shown_for_feature_.end())
    return session_rate.MeetsCriteria(0u);
  return session_rate.MeetsCriteria(it->second);
}

bool FeatureConfigConditionValidator::IsBlocked(
    const base::Feature& feature,
    const FeatureConfig& config,
    const Configuration* configuration) const {
  switch (config.blocked_by.type) {
    case BlockedBy::Type::NONE:
      return false;

    case BlockedBy::Type::ALL: {
      bool is_blocked = false;
      for (const std::string& currently_showing_feature :
           currently_showing_features_) {
        auto currently_showing_feature_config =
            configuration->GetFeatureConfigByName(currently_showing_feature);
        if (currently_showing_feature_config.blocking.type ==
            Blocking::Type::NONE)
          continue;
        is_blocked = true;
      }
      return is_blocked;
    }
    case BlockedBy::Type::EXPLICIT:
      for (const std::string& feature_name :
           *config.blocked_by.affected_features) {
        if (base::Contains(currently_showing_features_, feature_name))
          return true;
      }
      return false;
    default:
      // All cases should be covered.
      NOTREACHED_IN_MIGRATION();
  }
}

}  // namespace feature_engagement
