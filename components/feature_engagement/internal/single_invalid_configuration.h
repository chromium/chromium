// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_FEATURE_ENGAGEMENT_INTERNAL_SINGLE_INVALID_CONFIGURATION_H_
#define COMPONENTS_FEATURE_ENGAGEMENT_INTERNAL_SINGLE_INVALID_CONFIGURATION_H_

#include <string>
#include <unordered_set>

#include "base/macros.h"
#include "components/feature_engagement/public/configuration.h"

namespace base {
struct Feature;
}  // namespace base

namespace feature_engagement {

// An Configuration that always returns the same single invalid configuration,
// regardless of which feature. Also holds an empty ConfigMap.
class SingleInvalidConfiguration : public Configuration {
 public:
  SingleInvalidConfiguration();
  ~SingleInvalidConfiguration() override;

  // Configuration implementation.
  const FeatureConfig& GetFeatureConfig(
      const base::Feature& feature) const override;
  const FeatureConfig& GetFeatureConfigByName(
      const std::string& feature_name) const override;
  const Configuration::ConfigMap& GetRegisteredFeatureConfigs() const override;
  const std::vector<std::string> GetRegisteredFeatures() const override;

 private:
  // The invalid configuration to always return.
  FeatureConfig invalid_feature_config_;

  // An empty map.
  ConfigMap configs_;

  DISALLOW_COPY_AND_ASSIGN(SingleInvalidConfiguration);
};

}  // namespace feature_engagement

#endif  // COMPONENTS_FEATURE_ENGAGEMENT_INTERNAL_SINGLE_INVALID_CONFIGURATION_H_
