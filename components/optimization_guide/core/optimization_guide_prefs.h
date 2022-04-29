// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OPTIMIZATION_GUIDE_CORE_OPTIMIZATION_GUIDE_PREFS_H_
#define COMPONENTS_OPTIMIZATION_GUIDE_CORE_OPTIMIZATION_GUIDE_PREFS_H_

class PrefRegistrySimple;

namespace optimization_guide {
namespace prefs {

extern const char kHintsFetcherLastFetchAttempt[];
extern const char kModelAndFeaturesLastFetchAttempt[];
extern const char kModelLastFetchSuccess[];
extern const char kHintsFetcherHostsSuccessfullyFetched[];
extern const char kPendingHintsProcessingVersion[];
extern const char kPreviouslyRegisteredOptimizationTypes[];
extern const char kStoreFilePathsToDelete[];

// Registers the optimization guide's prefs.
void RegisterProfilePrefs(PrefRegistrySimple* registry);

}  // namespace prefs
}  // namespace optimization_guide

#endif  // COMPONENTS_OPTIMIZATION_GUIDE_CORE_OPTIMIZATION_GUIDE_PREFS_H_
