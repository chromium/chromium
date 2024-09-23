// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OPTIMIZATION_GUIDE_CORE_FEATURE_REGISTRY_SETTINGS_UI_REGISTRY_H_
#define COMPONENTS_OPTIMIZATION_GUIDE_CORE_FEATURE_REGISTRY_SETTINGS_UI_REGISTRY_H_

#include <vector>

#include "components/optimization_guide/core/feature_registry/enterprise_policy_registry.h"
#include "components/optimization_guide/core/model_execution/feature_keys.h"

namespace optimization_guide {

// SettingsUiMetadata holds metadata for a feature that uses the
// chrome://settings/ai page.
class SettingsUiMetadata {
 public:
  COMPONENT_EXPORT(OPTIMIZATION_GUIDE_FEATURES)
  SettingsUiMetadata(std::string name,
                     UserVisibleFeatureKey user_visible_feature_key,
                     EnterprisePolicyPref enterprise_policy);

  COMPONENT_EXPORT(OPTIMIZATION_GUIDE_FEATURES)
  ~SettingsUiMetadata();

  std::string name() const { return name_; }

  EnterprisePolicyPref enterprise_policy() const { return enterprise_policy_; }

  UserVisibleFeatureKey user_visible_feature_key() const {
    return user_visible_feature_key_;
  }

 private:
  // Name of the feature for histograms. This should only contain ASCII
  // characters.
  std::string name_;

  // The enum representing the feature.
  UserVisibleFeatureKey user_visible_feature_key_;

  // The pref to control the enterprise policy setting of the feature.
  EnterprisePolicyPref enterprise_policy_;
};

class SettingsUiRegistry {
 public:
  SettingsUiRegistry();
  ~SettingsUiRegistry();

  COMPONENT_EXPORT(OPTIMIZATION_GUIDE_FEATURES)
  static SettingsUiRegistry& GetInstance();

  // Registers a feature to use the chrome://settings/ai page. Features that
  // want to use this should register themselves in
  // components/optimization_guide/core/feature_registry/feature_registration.cc.
  COMPONENT_EXPORT(OPTIMIZATION_GUIDE_FEATURES)
  void Register(std::unique_ptr<SettingsUiMetadata> metadata);

  // Get the metadata for the given feature. Returns nullptr if there is no
  // registered feature matching the enum value.
  COMPONENT_EXPORT(OPTIMIZATION_GUIDE_FEATURES)
  const SettingsUiMetadata* GetFeature(UserVisibleFeatureKey feature) const;

  COMPONENT_EXPORT(OPTIMIZATION_GUIDE_FEATURES)
  void ClearForTesting();

 private:
  std::vector<std::unique_ptr<SettingsUiMetadata>> features_;
};

}  // namespace optimization_guide

#endif  // COMPONENTS_OPTIMIZATION_GUIDE_CORE_FEATURE_REGISTRY_SETTINGS_UI_REGISTRY_H_
