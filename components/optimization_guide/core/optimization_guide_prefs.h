// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// TODO: crbug.com/514743962 - All of these prefs should be moved to more
// specific files and out of this file.  Do not add anything here.

#ifndef COMPONENTS_OPTIMIZATION_GUIDE_CORE_OPTIMIZATION_GUIDE_PREFS_H_
#define COMPONENTS_OPTIMIZATION_GUIDE_CORE_OPTIMIZATION_GUIDE_PREFS_H_

#include <string>

#include "base/component_export.h"
#include "components/optimization_guide/core/model_execution/feature_keys.h"

class PrefRegistrySimple;

namespace optimization_guide {
namespace prefs {

// Value stored in the pref.
enum class FeatureOptInState {
  // User has never changed the opt-in state.
  kNotInitialized = 0,

  // User has explicitly opted-in to the feature.
  kEnabled = 1,

  // User has explicitly opted-out of the feature.
  kDisabled = 2
};

// Returns the name of the pref that stores the user's setting opt-in state for
// the given `feature`.
COMPONENT_EXPORT(OPTIMIZATION_GUIDE_FEATURES)
std::string GetSettingEnabledPrefName(UserVisibleFeatureKey feature);

// Registers the setting opt-in state prefs.
COMPONENT_EXPORT(OPTIMIZATION_GUIDE_FEATURES)
void RegisterSettingsEnabledPrefs(PrefRegistrySimple* registry);

}  // namespace prefs
}  // namespace optimization_guide

#endif  // COMPONENTS_OPTIMIZATION_GUIDE_CORE_OPTIMIZATION_GUIDE_PREFS_H_
