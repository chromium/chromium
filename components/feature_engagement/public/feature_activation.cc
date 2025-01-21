// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/feature_engagement/public/feature_activation.h"

#include "base/check.h"

namespace feature_engagement {

FeatureActivation::FeatureActivation(std::string unique_feature_name)
    : state_(State::kSingleFeatureEnabled),
      unique_feature_name_(unique_feature_name) {}

FeatureActivation::FeatureActivation(bool all_enabled)
    : state_(all_enabled ? State::kAllEnabled : State::kAllDisabled),
      unique_feature_name_(std::nullopt) {}

FeatureActivation::FeatureActivation(FeatureActivation&& feature_activation) =
    default;
FeatureActivation::FeatureActivation(FeatureActivation& feature_activation) =
    default;

FeatureActivation::~FeatureActivation() = default;

// static
FeatureActivation FeatureActivation::AllEnabled() {
  return FeatureActivation(true);
}

// static
FeatureActivation FeatureActivation::AllDisabled() {
  return FeatureActivation(false);
}

FeatureActivation::State FeatureActivation::get_state() {
  return state_;
}

const std::string& FeatureActivation::get_unique_feature_name() {
  CHECK(state_ == FeatureActivation::State::kSingleFeatureEnabled);
  CHECK(unique_feature_name_.has_value());
  return unique_feature_name_.value();
}

}  // namespace feature_engagement
