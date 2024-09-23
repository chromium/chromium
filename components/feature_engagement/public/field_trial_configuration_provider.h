// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_FEATURE_ENGAGEMENT_PUBLIC_FIELD_TRIAL_CONFIGURATION_PROVIDER_H_
#define COMPONENTS_FEATURE_ENGAGEMENT_PUBLIC_FIELD_TRIAL_CONFIGURATION_PROVIDER_H_

#include "components/feature_engagement/public/configuration_provider.h"

namespace feature_engagement {

// Provides Feature Engagement configuration by reading field trial parameters,
// either from Finch, command line, or fieldtrial_testing_config.json.
//
// One of two default supplies for configurations, along with
// `LocalConfigurationProvider`; this one should go first, as the local provider
// should only fill in configurations for which there are not active field
// trials.
//
// This provider does not override valid configurations.
class FieldTrialConfigurationProvider : public ConfigurationProvider {
 public:
  FieldTrialConfigurationProvider();
  ~FieldTrialConfigurationProvider() override;

  // FeatureConfigProvider:
  bool MaybeProvideFeatureConfiguration(
      const base::Feature& feature,
      FeatureConfig& config,
      const FeatureVector& known_features,
      const GroupVector& known_groups) const override;
  bool MaybeProvideGroupConfiguration(const base::Feature& feature,
                                      GroupConfig& config) const override;
  const char* GetConfigurationSourceDescription() const override;
};

}  // namespace feature_engagement

#endif  // COMPONENTS_FEATURE_ENGAGEMENT_PUBLIC_FIELD_TRIAL_CONFIGURATION_PROVIDER_H_
