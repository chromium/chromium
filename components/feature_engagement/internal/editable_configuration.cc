// Copyright 2017 The Chromium Authors
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

void EditableConfiguration::SetConfiguration(const base::Feature* group,
                                             const GroupConfig& group_config) {
  group_configs_[group->name] = group_config;
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

const GroupConfig& EditableConfiguration::GetGroupConfig(
    const base::Feature& group) const {
  auto it = group_configs_.find(group.name);
  DCHECK(it != group_configs_.end());
  return it->second;
}

const GroupConfig& EditableConfiguration::GetGroupConfigByName(
    const std::string& group_name) const {
  auto it = group_configs_.find(group_name);
  DCHECK(it != group_configs_.end());
  return it->second;
}

const Configuration::GroupConfigMap&
EditableConfiguration::GetRegisteredGroupConfigs() const {
  return group_configs_;
}

const std::vector<std::string> EditableConfiguration::GetRegisteredGroups()
    const {
  std::vector<std::string> groups;
  for (const auto& element : group_configs_)
    groups.push_back(element.first);
  return groups;
}

}  // namespace feature_engagement
