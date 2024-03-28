// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/feature_engagement/internal/single_invalid_configuration.h"

#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "components/feature_engagement/public/configuration.h"

namespace feature_engagement {

SingleInvalidConfiguration::SingleInvalidConfiguration() {
  invalid_feature_config_.valid = false;
  invalid_feature_config_.used.name = "nothing_to_see_here";

  invalid_group_config_.valid = false;
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

const GroupConfig& SingleInvalidConfiguration::GetGroupConfig(
    const base::Feature& group) const {
  return invalid_group_config_;
}

const GroupConfig& SingleInvalidConfiguration::GetGroupConfigByName(
    const std::string& group_name) const {
  return invalid_group_config_;
}

const Configuration::GroupConfigMap&
SingleInvalidConfiguration::GetRegisteredGroupConfigs() const {
  return group_configs_;
}

const std::vector<std::string> SingleInvalidConfiguration::GetRegisteredGroups()
    const {
  return {};
}

#if BUILDFLAG(IS_CHROMEOS_ASH)
void SingleInvalidConfiguration::UpdateConfig(
    const base::Feature& feature,
    const ConfigurationProvider* provider) {}

const Configuration::EventPrefixSet&
SingleInvalidConfiguration::GetRegisteredAllowedEventPrefixes() const {
  return event_prefixes_;
}
#endif

}  // namespace feature_engagement
