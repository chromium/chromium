// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_FEATURE_ENGAGEMENT_INTERNAL_SINGLE_INVALID_CONFIGURATION_H_
#define COMPONENTS_FEATURE_ENGAGEMENT_INTERNAL_SINGLE_INVALID_CONFIGURATION_H_

#include <string>
#include <unordered_set>

#include "base/feature_list.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "components/feature_engagement/public/configuration.h"

namespace feature_engagement {

// An Configuration that always returns the same single invalid configuration,
// regardless of which feature or group. Also holds an empty ConfigMap.
class SingleInvalidConfiguration : public Configuration {
 public:
  SingleInvalidConfiguration();

  SingleInvalidConfiguration(const SingleInvalidConfiguration&) = delete;
  SingleInvalidConfiguration& operator=(const SingleInvalidConfiguration&) =
      delete;

  ~SingleInvalidConfiguration() override;

  // Configuration implementation.
  const FeatureConfig& GetFeatureConfig(
      const base::Feature& feature) const override;
  const FeatureConfig& GetFeatureConfigByName(
      const std::string& feature_name) const override;
  const Configuration::ConfigMap& GetRegisteredFeatureConfigs() const override;
  const std::vector<std::string> GetRegisteredFeatures() const override;
  const GroupConfig& GetGroupConfig(const base::Feature& group) const override;
  const GroupConfig& GetGroupConfigByName(
      const std::string& group_name) const override;
  const Configuration::GroupConfigMap& GetRegisteredGroupConfigs()
      const override;
  const std::vector<std::string> GetRegisteredGroups() const override;
#if BUILDFLAG(IS_CHROMEOS_ASH)
  void UpdateConfig(const base::Feature& feature,
                    const ConfigurationProvider* provider) override;
  const Configuration::EventPrefixSet& GetRegisteredAllowedEventPrefixes()
      const override;
#endif

 private:
  // The invalid configuration to always return.
  FeatureConfig invalid_feature_config_;

  // The invalid group configuration to always return.
  GroupConfig invalid_group_config_;

  // An empty map.
  ConfigMap configs_;

  // An empty map.
  GroupConfigMap group_configs_;

#if BUILDFLAG(IS_CHROMEOS_ASH)
  // An empty set.
  EventPrefixSet event_prefixes_;
#endif
};

}  // namespace feature_engagement

#endif  // COMPONENTS_FEATURE_ENGAGEMENT_INTERNAL_SINGLE_INVALID_CONFIGURATION_H_
