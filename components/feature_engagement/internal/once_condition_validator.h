// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_FEATURE_ENGAGEMENT_INTERNAL_ONCE_CONDITION_VALIDATOR_H_
#define COMPONENTS_FEATURE_ENGAGEMENT_INTERNAL_ONCE_CONDITION_VALIDATOR_H_

#include <optional>
#include <unordered_set>

#include "base/feature_list.h"
#include "components/feature_engagement/internal/condition_validator.h"
#include "components/feature_engagement/public/feature_list.h"

namespace feature_engagement {
class AvailabilityModel;
class DisplayLockController;
class EventModel;
class TimeProvider;

// An ConditionValidator that will ensure that each base::Feature will meet
// conditions maximum one time for any given session.
// It has the following requirements:
// - The EventModel is ready.
// - No other in-product help is currently showing.
// - FeatureConfig for the feature is valid.
// - This is the first time the given base::Feature meets all above stated
//   conditions.
//
// NOTE: This ConditionValidator fully ignores whether the base::Feature is
// enabled or not and any other configuration specified in the FeatureConfig.
// In practice this leads this ConditionValidator to be well suited for a
// demonstration mode of in-product help.
class OnceConditionValidator : public ConditionValidator {
 public:
  OnceConditionValidator();

  OnceConditionValidator(const OnceConditionValidator&) = delete;
  OnceConditionValidator& operator=(const OnceConditionValidator&) = delete;

  ~OnceConditionValidator() override;

  // ConditionValidator implementation.
  ConditionValidator::Result MeetsConditions(
      const base::Feature& feature,
      const FeatureConfig& config,
      const std::vector<GroupConfig>& group_configs,
      const EventModel& event_model,
      const AvailabilityModel& availability_model,
      const DisplayLockController& display_lock_controller,
      const Configuration* configuration,
      const TimeProvider& time_provider) const override;
  void NotifyIsShowing(
      const base::Feature& feature,
      const FeatureConfig& config,
      const std::vector<std::string>& all_feature_names) override;
  void NotifyDismissed(const base::Feature& feature) override;
  void SetPriorityNotification(
      const std::optional<std::string>& feature) override;
  std::optional<std::string> GetPendingPriorityNotification() override;
  void AllowMultipleFeaturesForTesting(bool allow_multiple_features);
  void ResetSession() override;

 private:
  // Contains all features that have met conditions within the current session.
  std::unordered_set<std::string> shown_features_;

  // Which features that are currently being shown.
  std::unordered_set<std::string> currently_showing_features_;

  // Pending priority notification to be shown if any.
  std::optional<std::string> pending_priority_notification_;

  // Whether to allow multiple features shown at the same time.
  bool allows_multiple_features_ = false;
};

}  // namespace feature_engagement

#endif  // COMPONENTS_FEATURE_ENGAGEMENT_INTERNAL_ONCE_CONDITION_VALIDATOR_H_
