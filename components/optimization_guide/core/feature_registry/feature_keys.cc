// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/optimization_guide/core/feature_registry/feature_keys.h"

#include "components/prefs/pref_registry_simple.h"

namespace optimization_guide::prefs {

// TODO(b/354704993): Move this to the SettingsUiMetadata.
// Pref that contains user opt-in state for different features.
std::string GetSettingEnabledPrefName(UserVisibleFeatureKey feature) {
  switch (feature) {
    case UserVisibleFeatureKey::kCompose:
      return "optimization_guide.compose_setting_state";
    case UserVisibleFeatureKey::kWallpaperSearch:
      return "optimization_guide.wallpaper_search_setting_state";
    case UserVisibleFeatureKey::kHistorySearch:
      return "optimization_guide.history_search_setting_state";
    case UserVisibleFeatureKey::kPasswordChangeSubmission:
      return "optimization_guide.password_change_submission_setting_state";
    case UserVisibleFeatureKey::kFinds:
      return "optimization_guide.finds_setting_state";
    case UserVisibleFeatureKey::kContextualCueing:
      return "optimization_guide.contextual_cueing_setting_state";
  }
}

void RegisterSettingsUiPrefs(PrefRegistrySimple* registry) {
  for (auto key : kAllUserVisibleFeatureKeys) {
    registry->RegisterIntegerPref(
        GetSettingEnabledPrefName(key),
        static_cast<int>(FeatureOptInState::kNotInitialized));
  }
}

}  // namespace optimization_guide::prefs
