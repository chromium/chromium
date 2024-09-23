// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_GROWTH_CAMPAIGNS_CONFIGURATION_PROVIDER_H_
#define CHROMEOS_ASH_COMPONENTS_GROWTH_CAMPAIGNS_CONFIGURATION_PROVIDER_H_

#include <map>
#include <string>

#include "base/component_export.h"
#include "components/feature_engagement/public/configuration.h"
#include "components/feature_engagement/public/configuration_provider.h"

namespace growth {

// Provides feature engagement configuration based on Ash growth campaigns. It
// can set the prefix to allow events with the prefix to be recorded without
// predefining.
class COMPONENT_EXPORT(CHROMEOS_ASH_COMPONENTS_GROWTH_CONFIG_PROVIDER)
    CampaignsConfigurationProvider
    : public feature_engagement::ConfigurationProvider {
 public:
  CampaignsConfigurationProvider();
  ~CampaignsConfigurationProvider() override;

  // feature_engagement::ConfigurationProvider:
  bool MaybeProvideFeatureConfiguration(
      const base::Feature& feature,
      feature_engagement::FeatureConfig& config,
      const feature_engagement::FeatureVector& known_features,
      const feature_engagement::GroupVector& known_groups) const override;
  const char* GetConfigurationSourceDescription() const override;
  std::set<std::string> MaybeProvideAllowedEventPrefixes(
      const base::Feature& feature) const override;

  // Set config by a map of params.
  void SetConfig(std::map<std::string, std::string> params);

 private:
  void SetConfig(const feature_engagement::FeatureConfig& config);

  feature_engagement::FeatureConfig config_;
};

}  // namespace growth

#endif  // CHROMEOS_ASH_COMPONENTS_GROWTH_CAMPAIGNS_CONFIGURATION_PROVIDER_H_
