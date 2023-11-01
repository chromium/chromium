// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OPTIMIZATION_GUIDE_CORE_OPTIMIZATION_GUIDE_PREFS_H_
#define COMPONENTS_OPTIMIZATION_GUIDE_CORE_OPTIMIZATION_GUIDE_PREFS_H_

#include "components/optimization_guide/proto/model_execution.pb.h"

class PrefRegistrySimple;

namespace optimization_guide {
namespace prefs {

// User profile prefs.
extern const char kHintsFetcherLastFetchAttempt[];
extern const char kModelAndFeaturesLastFetchAttempt[];
extern const char kModelLastFetchSuccess[];
extern const char kHintsFetcherHostsSuccessfullyFetched[];
extern const char kPendingHintsProcessingVersion[];
extern const char kPreviouslyRegisteredOptimizationTypes[];
extern const char kStoreFilePathsToDelete[];
extern const char kFeatureOptInMainToggleSettingState[];

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
std::string GetSettingEnabledPrefName(proto::ModelExecutionFeature feature);

namespace localstate {

// Local state prefs.
extern const char kModelStoreMetadata[];
extern const char kModelCacheKeyMapping[];
extern const char kStoreFilePathsToDelete[];

}  // namespace localstate

// Registers the optimization guide's prefs.
void RegisterProfilePrefs(PrefRegistrySimple* registry);

// Registers the local state prefs.
void RegisterLocalStatePrefs(PrefRegistrySimple* registry);

}  // namespace prefs
}  // namespace optimization_guide

#endif  // COMPONENTS_OPTIMIZATION_GUIDE_CORE_OPTIMIZATION_GUIDE_PREFS_H_
