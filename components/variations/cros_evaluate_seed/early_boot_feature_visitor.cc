// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/variations/cros_evaluate_seed/early_boot_feature_visitor.h"

#include <string>

#include "base/feature_list.h"
#include "base/logging.h"
#include "base/metrics/field_trial_params.h"
#include "base/strings/string_util.h"
#include "chromeos/ash/components/dbus/featured/featured.pb.h"

namespace variations::cros_early_boot::evaluate_seed {

namespace {

// All early-boot CrOS feature names must start with "CrOSEarlyBoot".
constexpr char kEarlyBootFeaturePrefix[] = "CrOSEarlyBoot";

}  // namespace

EarlyBootFeatureVisitor::EarlyBootFeatureVisitor() = default;

EarlyBootFeatureVisitor::~EarlyBootFeatureVisitor() = default;

void EarlyBootFeatureVisitor::Visit(
    const std::string& feature_name,
    const base::FeatureList::OverrideState override_state,
    const base::FieldTrialParams& params,
    const std::string& trial_name,
    const std::string& group_name) {
  if (!base::StartsWith(feature_name, kEarlyBootFeaturePrefix)) {
    // Do not store the feature and params if the feature name does not start
    // with kEarlyBootFeaturePrefix.
    return;
  }

  featured::OverrideState feature_override_state;
  switch (override_state) {
    case base::FeatureList::OverrideState::OVERRIDE_USE_DEFAULT:
      feature_override_state = featured::OVERRIDE_USE_DEFAULT;
      break;
    case base::FeatureList::OverrideState::OVERRIDE_DISABLE_FEATURE:
      feature_override_state = featured::OVERRIDE_DISABLE_FEATURE;
      break;
    case base::FeatureList::OverrideState::OVERRIDE_ENABLE_FEATURE:
      feature_override_state = featured::OVERRIDE_ENABLE_FEATURE;
      break;
  }

  featured::FeatureOverride* feature = overrides_.Add();
  feature->set_name(feature_name);
  feature->set_override_state(feature_override_state);
  feature->set_trial_name(trial_name);
  feature->set_group_name(group_name);

  for (const auto& entry : params) {
    featured::Param* param = feature->add_params();
    param->set_key(entry.first);
    param->set_value(entry.second);
  }
}

}  // namespace variations::cros_early_boot::evaluate_seed
