// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_FEATURE_ENGAGEMENT_INTERNAL_CHROME_VARIATIONS_CONFIGURATION_H_
#define COMPONENTS_FEATURE_ENGAGEMENT_INTERNAL_CHROME_VARIATIONS_CONFIGURATION_H_

#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"
#include "components/feature_engagement/public/configuration.h"
#include "components/feature_engagement/public/feature_list.h"
#include "components/feature_engagement/public/group_list.h"

namespace feature_engagement {

// A ChromeVariationsConfiguration provides a configuration that is parsed from
// Chrome variations feature params. It is required to call
// ParseFeatureConfigs(...) with all the features that should be parsed.
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

  // Parses the variations configuration for all of the given |features| and
  // |groups| and stores the result. It is only valid to call ParseConfigs once.
  void ParseConfigs(const FeatureVector& features, const GroupVector& groups);

 private:
  void ParseFeatureConfig(const base::Feature* feature,
                          const FeatureVector& all_features,
                          const GroupVector& all_groups);
  void ParseGroupConfig(const base::Feature* group,
                        const FeatureVector& all_features,
                        const GroupVector& all_groups);

  // Checks whether |feature| should use a client side config and fills in
  // |params| the parameters for later parsing if necessary.
  bool ShouldUseClientSideConfig(const base::Feature* feature,
                                 base::FieldTrialParams* params);
  // Attempts to get the client side config for |feature| and add its config to
  // the config store. If |is_group| is true, then |feature| refers to a group
  // configuration instead of a feature configuration.
  void TryAddingClientSideConfig(const base::Feature* feature, bool is_group);
  // Returns true if FeatureConfig was found with a local hard coded
  // configuration.
  bool MaybeAddClientSideFeatureConfig(const base::Feature* feature);
  // Returns true if GroupConfig was found with a local hard coded
  // configuration.
  bool MaybeAddClientSideGroupConfig(const base::Feature* group);

  // The current configurations.
  ConfigMap configs_;

  // The current group configurations.
  GroupConfigMap group_configs_;
};

}  // namespace feature_engagement

#endif  // COMPONENTS_FEATURE_ENGAGEMENT_INTERNAL_CHROME_VARIATIONS_CONFIGURATION_H_
