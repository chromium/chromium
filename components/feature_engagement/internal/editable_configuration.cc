// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/feature_engagement/internal/editable_configuration.h"

#include <map>

#include "base/check.h"
#include "base/feature_list.h"
#include "components/feature_engagement/public/configuration.h"

namespace feature_engagement {

EditableConfiguration::EditableConfiguration() = default;

EditableConfiguration::~EditableConfiguration() = default;

void EditableConfiguration::SetConfiguration(
    const base::Feature* feature,
    const FeatureConfig& feature_config) {
  configs_[feature->name] = feature_config;
}

const FeatureConfig& EditableConfiguration::GetFeatureConfig(
    const base::Feature& feature) const {
  auto it = configs_.find(feature.name);
  DCHECK(it != configs_.end());
  return it->second;
}

const FeatureConfig& EditableConfiguration::GetFeatureConfigByName(
    const std::string& feature_name) const {
  auto it = configs_.find(feature_name);
  DCHECK(it != configs_.end());
  return it->second;
}

const Configuration::ConfigMap&
EditableConfiguration::GetRegisteredFeatureConfigs() const {
  return configs_;
}

const std::vector<std::string> EditableConfiguration::GetRegisteredFeatures()
    const {
  std::vector<std::string> features;
  for (const auto& element : configs_)
    features.push_back(element.first);
  return features;
}

}  // namespace feature_engagement
