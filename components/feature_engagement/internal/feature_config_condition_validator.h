// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_FEATURE_ENGAGEMENT_INTERNAL_FEATURE_CONFIG_CONDITION_VALIDATOR_H_
#define COMPONENTS_FEATURE_ENGAGEMENT_INTERNAL_FEATURE_CONFIG_CONDITION_VALIDATOR_H_

#include <stdint.h>

#include <map>
#include <optional>
#include <set>

#include "components/feature_engagement/internal/condition_validator.h"

namespace feature_engagement {
class AvailabilityModel;
struct Comparator;
struct EventConfig;
class EventModel;
class TimeProvider;

// A ConditionValidator that uses the FeatureConfigs as the source of truth.
class FeatureConfigConditionValidator : public ConditionValidator {
 public:
  FeatureConfigConditionValidator();

  FeatureConfigConditionValidator(const FeatureConfigConditionValidator&) =
      delete;
  FeatureConfigConditionValidator& operator=(
      const FeatureConfigConditionValidator&) = delete;

  ~FeatureConfigConditionValidator() override;

  // ConditionValidator implementation.
  ConditionValidator::Result MeetsConditions(
      const base::Feature& feature,
      const FeatureConfig& config,
      const std::vector<GroupConfig>& group_configs,
      const EventModel& event_model,
      const AvailabilityModel& availability_model,
      const DisplayLockController& display_lock_controller,
      const Configuration* configuration,
      const TimeProvider& current_day) const override;
  void NotifyIsShowing(
      const base::Feature& feature,
      const FeatureConfig& config,
      const std::vector<std::string>& all_feature_names) override;
  void NotifyDismissed(const base::Feature& feature) override;
  void SetPriorityNotification(
      const std::optional<std::string>& feature) override;
  std::optional<std::string> GetPendingPriorityNotification() override;
  void ResetSession() override;

 private:
  bool EventConfigMeetsConditions(const EventConfig& event_config,
                                  const EventModel& event_model,
                                  uint32_t current_day) const;

  bool AvailabilityMeetsConditions(const base::Feature& feature,
                                   Comparator comparator,
                                   const AvailabilityModel& availability_model,
                                   uint32_t current_day) const;

  bool SessionRateMeetsConditions(const Comparator session_rate,
                                  const base::Feature& feature) const;

  bool IsBlocked(const base::Feature& feature,
                 const FeatureConfig& config,
                 const Configuration* configuration) const;

  // A set of currently showing features. Added to the set on
  // ShouldTriggerHelpUi() and cleared during Dismissed() call.
  std::set<std::string> currently_showing_features_;

  // Stores how many times features that impact a given feature have been shown.
  // By default, all features impact each other, but some features override this
  // through the use of |session_rate_impact|.
  std::map<std::string, uint32_t> times_shown_for_feature_;

  // Pending priority notification to be shown if any.
  std::optional<std::string> pending_priority_notification_;
};

}  // namespace feature_engagement

#endif  // COMPONENTS_FEATURE_ENGAGEMENT_INTERNAL_FEATURE_CONFIG_CONDITION_VALIDATOR_H_
