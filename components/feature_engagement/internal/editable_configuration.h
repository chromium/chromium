// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_FEATURE_ENGAGEMENT_INTERNAL_EDITABLE_CONFIGURATION_H_
#define COMPONENTS_FEATURE_ENGAGEMENT_INTERNAL_EDITABLE_CONFIGURATION_H_

#include "base/feature_list.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "components/feature_engagement/public/configuration.h"

namespace feature_engagement {

// An EditableConfiguration provides a configuration that can be configured
// by calling SetConfiguration(...) for each feature, which makes it well
// suited for simple setup and tests.
class EditableConfiguration : public Configuration {
 public:
  EditableConfiguration();

  EditableConfiguration(const EditableConfiguration&) = delete;
  EditableConfiguration& operator=(const EditableConfiguration&) = delete;

  ~EditableConfiguration() override;

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

  // Adds a new FeatureConfig to the current configurations. If it already
  // exists, the contents are replaced.
  void SetConfiguration(const base::Feature* feature,
                        const FeatureConfig& feature_config);

  // Adds a new GroupConfig to the current configuration. If it already
  // exists, the contents are replaced.
  void SetConfiguration(const base::Feature* group,
                        const GroupConfig& group_config);

  // Adds a new allowed prefix to the current configuration.
  void AddAllowedEventPrefix(const std::string& prefix);

 private:
  // The current configurations.
  ConfigMap configs_;

  // The current group configurations.
  GroupConfigMap group_configs_;

  // The allowed set of prefixes.
  Configuration::EventPrefixSet event_prefixes_;
};

}  // namespace feature_engagement

#endif  // COMPONENTS_FEATURE_ENGAGEMENT_INTERNAL_EDITABLE_CONFIGURATION_H_
