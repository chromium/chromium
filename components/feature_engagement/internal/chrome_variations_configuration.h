// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_FEATURE_ENGAGEMENT_INTERNAL_CHROME_VARIATIONS_CONFIGURATION_H_
#define COMPONENTS_FEATURE_ENGAGEMENT_INTERNAL_CHROME_VARIATIONS_CONFIGURATION_H_

#include "base/feature_list.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "components/feature_engagement/public/configuration.h"
#include "components/feature_engagement/public/configuration_provider.h"
#include "components/feature_engagement/public/feature_list.h"
#include "components/feature_engagement/public/group_list.h"

namespace feature_engagement {

// A ChromeVariationsConfiguration provides a configuration that is parsed from
// Chrome variations feature params. It is required to call
// LoadFeatureConfigs(...) with all the features that should be parsed or
// specified from code.
class ChromeVariationsConfiguration : public Configuration {
 public:
  ChromeVariationsConfiguration();
  ChromeVariationsConfiguration(const ChromeVariationsConfiguration&) = delete;
  ChromeVariationsConfiguration& operator=(
      const ChromeVariationsConfiguration&) = delete;

  ~ChromeVariationsConfiguration() override;

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

  // Read the variations configuration for all of the given |features| and
  // |groups| using the `configuration_providers`, and stores the result. It is
  // only valid to call this method once.
  void LoadConfigs(const ConfigurationProviderList& configuration_providers,
                   const FeatureVector& features,
                   const GroupVector& groups);

#if BUILDFLAG(IS_CHROMEOS_ASH)
  void UpdateConfig(const base::Feature& feature,
                    const ConfigurationProvider* provider) override;

  const Configuration::EventPrefixSet& GetRegisteredAllowedEventPrefixes()
      const override;
#endif

 private:
  void LoadFeatureConfig(
      const base::Feature& feature,
      const ConfigurationProviderList& configuration_providers,
      const FeatureVector& all_features,
      const GroupVector& all_groups);
  void LoadGroupConfig(
      const base::Feature& group,
      const ConfigurationProviderList& configuration_providers);
#if BUILDFLAG(IS_CHROMEOS_ASH)
  void LoadAllowedEventPrefixes(
      const base::Feature& feature,
      const ConfigurationProviderList& configuration_providers);
#endif

  // Expands any group names in the existing FeatureConfig fields into the
  // feature names that are in the group.
  void ExpandGroupNamesInFeatures(const GroupVector& all_groups);

  // The current configurations.
  ConfigMap configs_;

  // The current group configurations.
  GroupConfigMap group_configs_;

#if BUILDFLAG(IS_CHROMEOS_ASH)
  // The allowed set of prefixes.
  EventPrefixSet event_prefixes_;
#endif
};

}  // namespace feature_engagement

#endif  // COMPONENTS_FEATURE_ENGAGEMENT_INTERNAL_CHROME_VARIATIONS_CONFIGURATION_H_
