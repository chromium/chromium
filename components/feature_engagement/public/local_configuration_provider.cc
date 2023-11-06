// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/feature_engagement/public/local_configuration_provider.h"

#include <string>

#include "components/feature_engagement/public/configuration.h"
#include "components/feature_engagement/public/feature_configurations.h"
#include "components/feature_engagement/public/group_configurations.h"
#include "components/feature_engagement/public/group_constants.h"

namespace feature_engagement {

LocalConfigurationProvider::LocalConfigurationProvider() = default;
LocalConfigurationProvider::~LocalConfigurationProvider() = default;

bool LocalConfigurationProvider::MaybeProvideFeatureConfiguration(
    const base::Feature& feature,
    FeatureConfig& config,
    const FeatureVector& known_features,
    const GroupVector& known_groups) const {
  if (!include_disabled_features_ && !base::FeatureList::IsEnabled(feature)) {
    return false;
  }

  if (config.valid) {
    return false;
  }

  const auto result = GetClientSideFeatureConfig(&feature);
  if (!result.has_value()) {
    return false;
  }

  // If the config contains any groups, check if those groups are supported.
  for (const auto& group_name : config.groups) {
    CHECK(ContainsFeature(group_name, known_groups))
        << "Local configuration for " << feature.name
        << " references unknown group " << group_name;
  }

  // TODO(dfried): Validate trigger, used, blocking, blocked_by, etc.

  config = *result;
  return true;
}

bool LocalConfigurationProvider::MaybeProvideGroupConfiguration(
    const base::Feature& feature,
    GroupConfig& config) const {
  if (!include_disabled_features_ && !base::FeatureList::IsEnabled(feature)) {
    return false;
  }

  if (config.valid) {
    return false;
  }

  const auto result = GetClientSideGroupConfig(&feature);
  if (!result.has_value()) {
    return false;
  }

  // TODO(dfried): Validate trigger.

  config = *result;
  return true;
}

const char* LocalConfigurationProvider::GetConfigurationSourceDescription()
    const {
  return "checked-in";
}

stats::ConfigParsingEvent LocalConfigurationProvider::GetOnSuccessEvent()
    const {
  return stats::ConfigParsingEvent::SUCCESS_FROM_SOURCE;
}

}  // namespace feature_engagement
