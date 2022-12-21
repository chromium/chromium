// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OPTIMIZATION_GUIDE_CORE_OPTIMIZATION_GUIDE_PREFS_H_
#define COMPONENTS_OPTIMIZATION_GUIDE_CORE_OPTIMIZATION_GUIDE_PREFS_H_

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
