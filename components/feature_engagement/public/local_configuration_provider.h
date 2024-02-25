// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_FEATURE_ENGAGEMENT_PUBLIC_LOCAL_CONFIGURATION_PROVIDER_H_
#define COMPONENTS_FEATURE_ENGAGEMENT_PUBLIC_LOCAL_CONFIGURATION_PROVIDER_H_

#include "components/feature_engagement/public/configuration_provider.h"

namespace base {
struct Feature;
}

namespace feature_engagement {
struct FeatureConfig;

// Provides local, hard-coded configuration for Feature Engagement features.
//
// One of two default providers, along with `FieldTrialConfigurationProvider`;
// this one should go after the field trial provider, as its job is only to fill
// in configuration for features where there is no explicit field trial.
//
// Does not override valid configurations.
class LocalConfigurationProvider : public ConfigurationProvider {
 public:
  explicit LocalConfigurationProvider();
  ~LocalConfigurationProvider() override;

  // FeatureConfigProvider:
  bool MaybeProvideFeatureConfiguration(
      const base::Feature& feature,
      FeatureConfig& config,
      const FeatureVector& known_features,
      const GroupVector& known_groups) const override;
  bool MaybeProvideGroupConfiguration(const base::Feature& feature,
                                      GroupConfig& config) const override;
  const char* GetConfigurationSourceDescription() const override;
  stats::ConfigParsingEvent GetOnSuccessEvent() const override;

  void set_include_disabled_features_for_testing() {
    include_disabled_features_ = true;
  }

 private:
  // Allows configuration to be loaded for disabled features for validation
  // purposes during tests.
  bool include_disabled_features_ = false;
};

}  // namespace feature_engagement

#endif  // COMPONENTS_FEATURE_ENGAGEMENT_PUBLIC_LOCAL_CONFIGURATION_PROVIDER_H_
