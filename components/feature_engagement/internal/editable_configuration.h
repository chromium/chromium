// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_FEATURE_ENGAGEMENT_INTERNAL_EDITABLE_CONFIGURATION_H_
#define COMPONENTS_FEATURE_ENGAGEMENT_INTERNAL_EDITABLE_CONFIGURATION_H_

#include "base/macros.h"
#include "components/feature_engagement/public/configuration.h"

namespace base {
struct Feature;
}  // namespace base

namespace feature_engagement {

// An EditableConfiguration provides a configuration that can be configured
// by calling SetConfiguration(...) for each feature, which makes it well
// suited for simple setup and tests.
class EditableConfiguration : public Configuration {
 public:
  EditableConfiguration();
  ~EditableConfiguration() override;

  // Configuration implementation.
  const FeatureConfig& GetFeatureConfig(
      const base::Feature& feature) const override;
  const FeatureConfig& GetFeatureConfigByName(
      const std::string& feature_name) const override;
  const Configuration::ConfigMap& GetRegisteredFeatureConfigs() const override;
  const std::vector<std::string> GetRegisteredFeatures() const override;

  // Adds a new FeatureConfig to the current configurations. If it already
  // exists, the contents are replaced.
  void SetConfiguration(const base::Feature* feature,
                        const FeatureConfig& feature_config);

 private:
  // The current configurations.
  ConfigMap configs_;

  DISALLOW_COPY_AND_ASSIGN(EditableConfiguration);
};

}  // namespace feature_engagement

#endif  // COMPONENTS_FEATURE_ENGAGEMENT_INTERNAL_EDITABLE_CONFIGURATION_H_
