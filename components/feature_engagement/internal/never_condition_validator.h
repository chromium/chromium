// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_FEATURE_ENGAGEMENT_INTERNAL_NEVER_CONDITION_VALIDATOR_H_
#define COMPONENTS_FEATURE_ENGAGEMENT_INTERNAL_NEVER_CONDITION_VALIDATOR_H_

#include <optional>

#include "base/feature_list.h"
#include "components/feature_engagement/internal/condition_validator.h"
#include "components/feature_engagement/public/feature_list.h"

namespace feature_engagement {
class AvailabilityModel;
class DisplayLockController;
class EventModel;
class TimeProvider;

// An ConditionValidator that never acknowledges that a feature has met its
// conditions.
class NeverConditionValidator : public ConditionValidator {
 public:
  NeverConditionValidator();

  NeverConditionValidator(const NeverConditionValidator&) = delete;
  NeverConditionValidator& operator=(const NeverConditionValidator&) = delete;

  ~NeverConditionValidator() override;

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
  void ResetSession() override;
};

}  // namespace feature_engagement

#endif  // COMPONENTS_FEATURE_ENGAGEMENT_INTERNAL_NEVER_CONDITION_VALIDATOR_H_
