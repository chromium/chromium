// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/feature_engagement/internal/editable_configuration.h"

#include <map>

#include "base/check.h"
#include "base/feature_list.h"
#include "base/not_fatal_until.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "components/feature_engagement/public/configuration.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "components/feature_engagement/public/configuration_provider.h"
#endif

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

void EditableConfiguration::AddAllowedEventPrefix(const std::string& prefix) {
  CHECK(!prefix.empty());
  event_prefixes_.insert(prefix);
}

const FeatureConfig& EditableConfiguration::GetFeatureConfig(
    const base::Feature& feature) const {
  auto it = configs_.find(feature.name);
  CHECK(it != configs_.end(), base::NotFatalUntil::M130);
  return it->second;
}

const FeatureConfig& EditableConfiguration::GetFeatureConfigByName(
    const std::string& feature_name) const {
  auto it = configs_.find(feature_name);
  CHECK(it != configs_.end(), base::NotFatalUntil::M130);
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
  CHECK(it != group_configs_.end(), base::NotFatalUntil::M130);
  return it->second;
}

const GroupConfig& EditableConfiguration::GetGroupConfigByName(
    const std::string& group_name) const {
  auto it = group_configs_.find(group_name);
  CHECK(it != group_configs_.end(), base::NotFatalUntil::M130);
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

#if BUILDFLAG(IS_CHROMEOS_ASH)
void EditableConfiguration::UpdateConfig(
    const base::Feature& feature,
    const ConfigurationProvider* provider) {
  FeatureConfig& config = configs_[feature.name];

  // Clear existing configs.
  config = FeatureConfig();
  provider->MaybeProvideFeatureConfiguration(feature, config, {}, {});
}

const Configuration::EventPrefixSet&
EditableConfiguration::GetRegisteredAllowedEventPrefixes() const {
  return event_prefixes_;
}
#endif

}  // namespace feature_engagement
