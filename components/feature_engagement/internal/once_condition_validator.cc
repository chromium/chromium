// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/feature_engagement/internal/once_condition_validator.h"

#include "components/feature_engagement/internal/event_model.h"
#include "components/feature_engagement/public/configuration.h"

namespace feature_engagement {

OnceConditionValidator::OnceConditionValidator() = default;

OnceConditionValidator::~OnceConditionValidator() = default;

ConditionValidator::Result OnceConditionValidator::MeetsConditions(
    const base::Feature& feature,
    const FeatureConfig& config,
    const EventModel& event_model,
    const AvailabilityModel& availability_model,
    const DisplayLockController& display_lock_controller,
    uint32_t current_day) const {
  ConditionValidator::Result result(true);
  result.event_model_ready_ok = event_model.IsReady();

  result.currently_showing_ok = currently_showing_feature_.empty();

  result.config_ok = config.valid;

  result.trigger_ok =
      shown_features_.find(feature.name) == shown_features_.end();
  result.session_rate_ok =
      shown_features_.find(feature.name) == shown_features_.end();

  return result;
}

void OnceConditionValidator::NotifyIsShowing(
    const base::Feature& feature,
    const FeatureConfig& config,
    const std::vector<std::string>& all_feature_names) {
  DCHECK(currently_showing_feature_.empty());
  DCHECK(shown_features_.find(feature.name) == shown_features_.end());
  shown_features_.insert(feature.name);
  currently_showing_feature_ = feature.name;
}

void OnceConditionValidator::NotifyDismissed(const base::Feature& feature) {
  DCHECK(feature.name == currently_showing_feature_);
  currently_showing_feature_.clear();
}

}  // namespace feature_engagement
