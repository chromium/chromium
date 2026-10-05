// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OPTIMIZATION_GUIDE_CORE_HINTS_PREFS_H_
#define COMPONENTS_OPTIMIZATION_GUIDE_CORE_HINTS_PREFS_H_

class PrefRegistrySimple;

namespace optimization_guide::prefs {

// User profile prefs.
extern const char kHintsFetcherLastFetchAttempt[];
extern const char kHintsFetcherHostsSuccessfullyFetched[];
extern const char kPendingHintsProcessingVersion[];
extern const char kPreviouslyRegisteredOptimizationTypes[];
extern const char kPreviousOptimizationTypesWithFilter[];

// Registers the optimization guide hints profile prefs.
void RegisterProfilePrefs(PrefRegistrySimple* registry);

}  // namespace optimization_guide::prefs

#endif  // COMPONENTS_OPTIMIZATION_GUIDE_CORE_HINTS_PREFS_H_
