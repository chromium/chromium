// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/feature_engagement/internal/single_invalid_configuration.h"

#include "components/feature_engagement/public/configuration.h"

namespace feature_engagement {

SingleInvalidConfiguration::SingleInvalidConfiguration() {
  invalid_feature_config_.valid = false;
  invalid_feature_config_.used.name = "nothing_to_see_here";
}

SingleInvalidConfiguration::~SingleInvalidConfiguration() = default;

const FeatureConfig& SingleInvalidConfiguration::GetFeatureConfig(
    const base::Feature& feature) const {
  return invalid_feature_config_;
}

const FeatureConfig& SingleInvalidConfiguration::GetFeatureConfigByName(
    const std::string& feature_name) const {
  return invalid_feature_config_;
}

const Configuration::ConfigMap&
SingleInvalidConfiguration::GetRegisteredFeatureConfigs() const {
  return configs_;
}

const std::vector<std::string>
SingleInvalidConfiguration::GetRegisteredFeatures() const {
  return {};
}

}  // namespace feature_engagement
