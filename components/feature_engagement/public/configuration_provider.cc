// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/feature_engagement/public/configuration_provider.h"

namespace feature_engagement {

ConfigurationProvider::ConfigurationProvider() = default;
ConfigurationProvider::~ConfigurationProvider() = default;

bool ConfigurationProvider::MaybeProvideFeatureConfiguration(
    const base::Feature& feature,
    FeatureConfig& config,
    const FeatureVector& known_features,
    const GroupVector& known_groups) const {
  return false;
}

bool ConfigurationProvider::MaybeProvideGroupConfiguration(
    const base::Feature& feature,
    GroupConfig& config) const {
  return false;
}

stats::ConfigParsingEvent ConfigurationProvider::GetOnSuccessEvent() const {
  return stats::ConfigParsingEvent::SUCCESS;
}

}  // namespace feature_engagement
