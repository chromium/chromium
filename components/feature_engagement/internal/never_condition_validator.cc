// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/feature_engagement/internal/never_condition_validator.h"

#include <optional>

namespace feature_engagement {

NeverConditionValidator::NeverConditionValidator() = default;

NeverConditionValidator::~NeverConditionValidator() = default;

ConditionValidator::Result NeverConditionValidator::MeetsConditions(
    const base::Feature& feature,
    const FeatureConfig& config,
    const std::vector<GroupConfig>& group_configs,
    const EventModel& event_model,
    const AvailabilityModel& availability_model,
    const DisplayLockController& display_lock_controller,
    const Configuration* configuration,
    const TimeProvider& time_provider) const {
  return ConditionValidator::Result(false);
}

void NeverConditionValidator::NotifyIsShowing(
    const base::Feature& feature,
    const FeatureConfig& config,
    const std::vector<std::string>& all_feature_names) {}

void NeverConditionValidator::NotifyDismissed(const base::Feature& feature) {}

void NeverConditionValidator::SetPriorityNotification(
    const std::optional<std::string>& feature) {}

std::optional<std::string>
NeverConditionValidator::GetPendingPriorityNotification() {
  return std::nullopt;
}

void NeverConditionValidator::ResetSession() {}

}  // namespace feature_engagement
