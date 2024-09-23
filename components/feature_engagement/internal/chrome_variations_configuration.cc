// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/feature_engagement/internal/chrome_variations_configuration.h"

#include <map>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

#include "base/check_op.h"
#include "base/containers/contains.h"
#include "base/feature_list.h"
#include "base/logging.h"
#include "base/not_fatal_until.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "components/feature_engagement/public/configuration.h"
#include "components/feature_engagement/public/configuration_provider.h"
#include "components/feature_engagement/public/feature_list.h"
#include "components/feature_engagement/public/group_constants.h"
#include "components/feature_engagement/public/stats.h"

namespace feature_engagement {

namespace {

template <typename T>
void RecordParseResult(std::string name,
                       T config,
                       const ConfigurationProvider& provider) {
  const char* const source = provider.GetConfigurationSourceDescription();
  if (config.valid) {
    stats::RecordConfigParsingEvent(provider.GetOnSuccessEvent());
    DVLOG(2) << "Config from " << source << " for " << name << " is valid.";
    DVLOG(3) << "Config from " << source << " for " << name << " = " << config;
  } else {
    stats::RecordConfigParsingEvent(stats::ConfigParsingEvent::FAILURE);
    DVLOG(2) << "Config from " << source << " for " << name << " is invalid.";
  }
}

// Takes a list of |original_names| and expands any groups in the list into the
// features that make up that group, using |group_mapping| and |all_groups|.
std::vector<std::string> FlattenGroupsAndFeatures(
    std::vector<std::string> original_names,
    std::map<std::string, std::vector<std::string>> group_mapping,
    const GroupVector& all_groups) {
  // Use set to make sure feature names don't occur twice.
  std::set<std::string> flattened_feature_names;
  for (auto name : original_names) {
    if (ContainsFeature(name, all_groups)) {
      auto it = group_mapping.find(name);
      if (it == group_mapping.end()) {
        continue;
      }
      // Group is known and can be replaced by the features in it.
      for (auto feature_name : it->second) {
        flattened_feature_names.insert(feature_name);
      }
    } else {
      // Otherwise, the name is a feature name already.
      flattened_feature_names.insert(name);
    }
  }

  std::vector<std::string> result(flattened_feature_names.begin(),
                                  flattened_feature_names.end());
  return result;
}

}  // namespace

ChromeVariationsConfiguration::ChromeVariationsConfiguration() = default;
ChromeVariationsConfiguration::~ChromeVariationsConfiguration() = default;

void ChromeVariationsConfiguration::LoadConfigs(
    const ConfigurationProviderList& configuration_providers,
    const FeatureVector& features,
    const GroupVector& groups) {
  // This method should only be called once.
  CHECK(configs_.empty());
  CHECK(group_configs_.empty());

  for (auto* feature : features) {
    LoadFeatureConfig(*feature, configuration_providers, features, groups);
#if BUILDFLAG(IS_CHROMEOS_ASH)
    LoadAllowedEventPrefixes(*feature, configuration_providers);
#endif
  }

  ExpandGroupNamesInFeatures(groups);

  for (auto* group : groups) {
    LoadGroupConfig(*group, configuration_providers);
  }
}

#if BUILDFLAG(IS_CHROMEOS_ASH)
void ChromeVariationsConfiguration::UpdateConfig(
    const base::Feature& feature,
    const ConfigurationProvider* provider) {
  FeatureConfig& config = configs_[feature.name];

  // Clear existing configs.
  config = FeatureConfig();
  provider->MaybeProvideFeatureConfiguration(feature, config, {}, {});
}

const Configuration::EventPrefixSet&
ChromeVariationsConfiguration::GetRegisteredAllowedEventPrefixes() const {
  return event_prefixes_;
}
#endif

void ChromeVariationsConfiguration::LoadFeatureConfig(
    const base::Feature& feature,
    const ConfigurationProviderList& configuration_providers,
    const FeatureVector& all_features,
    const GroupVector& all_groups) {
  DCHECK(!base::Contains(configs_, feature.name));

  DVLOG(3) << "Loading feature config for " << feature.name;
  bool loaded = false;
  FeatureConfig& config = configs_[feature.name];
  for (const auto& provider : configuration_providers) {
    const bool result = provider->MaybeProvideFeatureConfiguration(
        feature, config, all_features, all_groups);
    if (result) {
      RecordParseResult(feature.name, config, *provider);
    }
    loaded |= result;
  }

  if (!loaded) {
    stats::RecordConfigParsingEvent(
        stats::ConfigParsingEvent::FAILURE_NO_FIELD_TRIAL);
    // Returns early. If no field trial, ConfigParsingEvent::FAILURE will not be
    // recorded.
    DVLOG(3) << "No field trial or checked in config for " << feature.name;
  }
}

void ChromeVariationsConfiguration::LoadGroupConfig(
    const base::Feature& group,
    const ConfigurationProviderList& configuration_providers) {
  DCHECK(!base::Contains(group_configs_, group.name));

  DVLOG(3) << "Parsing group config for " << group.name;

  bool loaded = false;
  GroupConfig& config = group_configs_[group.name];
  for (const auto& provider : configuration_providers) {
    const bool result = provider->MaybeProvideGroupConfiguration(group, config);
    if (result) {
      RecordParseResult(group.name, config, *provider);
    }
    loaded |= result;
  }

  if (!loaded) {
    stats::RecordConfigParsingEvent(
        stats::ConfigParsingEvent::FAILURE_NO_FIELD_TRIAL);
    // Returns early. If no field trial, ConfigParsingEvent::FAILURE will not be
    // recorded.
    DVLOG(3) << "No field trial or checked in config for " << group.name;
  }
}

#if BUILDFLAG(IS_CHROMEOS_ASH)
void ChromeVariationsConfiguration::LoadAllowedEventPrefixes(
    const base::Feature& feature,
    const ConfigurationProviderList& configuration_providers) {
  // Allowed prefixes from different providers are inserted in a set, therefore,
  // it could potential affect other features.
  for (const auto& provider : configuration_providers) {
    const auto prefixes = provider->MaybeProvideAllowedEventPrefixes(feature);
    for (auto it = prefixes.begin(); it != prefixes.end(); ++it) {
      // Do not insert empty prefix.
      if (it->empty()) {
        continue;
      }

      // Check if there are duplicate prefixes.
      const auto inserted = event_prefixes_.insert(*it);
      CHECK(inserted.second)
          << "Configuration has duplicate event prefixes: " << *it;
    }
  }
}
#endif

void ChromeVariationsConfiguration::ExpandGroupNamesInFeatures(
    const GroupVector& all_groups) {
  // Create mapping of groups to their constituent features.
  std::map<std::string, std::vector<std::string>> group_to_feature_mapping;
  for (const auto& [feature_name, feature] : configs_) {
    for (auto group_name : feature.groups) {
      group_to_feature_mapping[group_name].push_back(feature_name);
    }
  }

  // Flatten any group names in each feature's blocked by or session rate impact
  // list into the constituent features.
  for (auto& [feature_name, feature] : configs_) {
    if (feature.blocked_by.type == BlockedBy::Type::EXPLICIT) {
      auto original_blocked_by_items =
          feature.blocked_by.affected_features.value();
      feature.blocked_by.affected_features = FlattenGroupsAndFeatures(
          original_blocked_by_items, group_to_feature_mapping, all_groups);
    }
    if (feature.session_rate_impact.type == SessionRateImpact::Type::EXPLICIT) {
      feature.session_rate_impact.affected_features = FlattenGroupsAndFeatures(
          feature.session_rate_impact.affected_features.value(),
          group_to_feature_mapping, all_groups);
    }
  }
}

const FeatureConfig& ChromeVariationsConfiguration::GetFeatureConfig(
    const base::Feature& feature) const {
  auto it = configs_.find(feature.name);
  CHECK(it != configs_.end(), base::NotFatalUntil::M130);
  return it->second;
}

const FeatureConfig& ChromeVariationsConfiguration::GetFeatureConfigByName(
    const std::string& feature_name) const {
  auto it = configs_.find(feature_name);
  CHECK(it != configs_.end(), base::NotFatalUntil::M130);
  return it->second;
}

const Configuration::ConfigMap&
ChromeVariationsConfiguration::GetRegisteredFeatureConfigs() const {
  return configs_;
}

const std::vector<std::string>
ChromeVariationsConfiguration::GetRegisteredFeatures() const {
  std::vector<std::string> features;
  for (const auto& element : configs_)
    features.push_back(element.first);
  return features;
}

const GroupConfig& ChromeVariationsConfiguration::GetGroupConfig(
    const base::Feature& group) const {
  auto it = group_configs_.find(group.name);
  CHECK(it != group_configs_.end(), base::NotFatalUntil::M130);
  return it->second;
}

const GroupConfig& ChromeVariationsConfiguration::GetGroupConfigByName(
    const std::string& group_name) const {
  auto it = group_configs_.find(group_name);
  CHECK(it != group_configs_.end(), base::NotFatalUntil::M130);
  return it->second;
}

const Configuration::GroupConfigMap&
ChromeVariationsConfiguration::GetRegisteredGroupConfigs() const {
  return group_configs_;
}

const std::vector<std::string>
ChromeVariationsConfiguration::GetRegisteredGroups() const {
  std::vector<std::string> groups;
  for (const auto& element : group_configs_)
    groups.push_back(element.first);
  return groups;
}

}  // namespace feature_engagement
