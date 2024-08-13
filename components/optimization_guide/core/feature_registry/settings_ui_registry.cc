// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/optimization_guide/core/feature_registry/settings_ui_registry.h"

#include "base/no_destructor.h"
#include "base/strings/string_util.h"
#include "components/optimization_guide/core/model_execution/feature_keys.h"

namespace optimization_guide {

SettingsUiMetadata::SettingsUiMetadata(
    std::string name,
    UserVisibleFeatureKey user_visible_feature_key,
    EnterprisePolicyPref enterprise_policy)
    : name_(name),
      user_visible_feature_key_(user_visible_feature_key),
      enterprise_policy_(enterprise_policy) {
  CHECK(base::IsStringASCII(name));
}

SettingsUiMetadata::~SettingsUiMetadata() = default;

SettingsUiRegistry::SettingsUiRegistry() = default;
SettingsUiRegistry::~SettingsUiRegistry() = default;

SettingsUiRegistry& SettingsUiRegistry::GetInstance() {
  static base::NoDestructor<SettingsUiRegistry> registry;
  return *registry;
}

void SettingsUiRegistry::Register(
    std::unique_ptr<SettingsUiMetadata> new_metadata) {
  for (auto& metadata : features_) {
    CHECK(metadata->user_visible_feature_key() !=
          new_metadata->user_visible_feature_key());
    CHECK(metadata->name() != new_metadata->name());
  }
  features_.emplace_back(std::move(new_metadata));
}

const SettingsUiMetadata* SettingsUiRegistry::GetFeature(
    UserVisibleFeatureKey feature) const {
  for (auto& metadata : features_) {
    if (metadata->user_visible_feature_key() == feature) {
      return metadata.get();
    }
  }
  return nullptr;
}

void SettingsUiRegistry::ClearForTesting() {
  features_.clear();
}

}  // namespace optimization_guide
