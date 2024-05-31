// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OPTIMIZATION_GUIDE_CORE_OPTIMIZATION_GUIDE_PREFS_H_
#define COMPONENTS_OPTIMIZATION_GUIDE_CORE_OPTIMIZATION_GUIDE_PREFS_H_

#include "base/component_export.h"
#include "components/optimization_guide/core/model_execution/feature_keys.h"
#include "components/optimization_guide/proto/model_execution.pb.h"

class PrefRegistrySimple;

namespace optimization_guide {
namespace prefs {

// User profile prefs.
COMPONENT_EXPORT(OPTIMIZATION_GUIDE_FEATURES)
extern const char kHintsFetcherLastFetchAttempt[];
COMPONENT_EXPORT(OPTIMIZATION_GUIDE_FEATURES)
extern const char kModelAndFeaturesLastFetchAttempt[];
COMPONENT_EXPORT(OPTIMIZATION_GUIDE_FEATURES)
extern const char kModelLastFetchSuccess[];
COMPONENT_EXPORT(OPTIMIZATION_GUIDE_FEATURES)
extern const char kHintsFetcherHostsSuccessfullyFetched[];
COMPONENT_EXPORT(OPTIMIZATION_GUIDE_FEATURES)
extern const char kPendingHintsProcessingVersion[];
COMPONENT_EXPORT(OPTIMIZATION_GUIDE_FEATURES)
extern const char kPreviouslyRegisteredOptimizationTypes[];
COMPONENT_EXPORT(OPTIMIZATION_GUIDE_FEATURES)
extern const char kStoreFilePathsToDelete[];
COMPONENT_EXPORT(OPTIMIZATION_GUIDE_FEATURES)
extern const char kModelExecutionMainToggleSettingState[];
COMPONENT_EXPORT(OPTIMIZATION_GUIDE_FEATURES)
extern const char kPreviousOptimizationTypesWithFilter[];

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

namespace localstate {

// Local state prefs.
COMPONENT_EXPORT(OPTIMIZATION_GUIDE_FEATURES)
extern const char kModelStoreMetadata[];
COMPONENT_EXPORT(OPTIMIZATION_GUIDE_FEATURES)
extern const char kModelCacheKeyMapping[];
COMPONENT_EXPORT(OPTIMIZATION_GUIDE_FEATURES)
extern const char kStoreFilePathsToDelete[];

}  // namespace localstate

// Registers the optimization guide's prefs.
COMPONENT_EXPORT(OPTIMIZATION_GUIDE_FEATURES)
void RegisterProfilePrefs(PrefRegistrySimple* registry);

// Registers the local state prefs.
COMPONENT_EXPORT(OPTIMIZATION_GUIDE_FEATURES)
void RegisterLocalStatePrefs(PrefRegistrySimple* registry);

}  // namespace prefs
}  // namespace optimization_guide

#endif  // COMPONENTS_OPTIMIZATION_GUIDE_CORE_OPTIMIZATION_GUIDE_PREFS_H_
