// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/optimization_guide/core/optimization_guide_prefs.h"

#include "components/optimization_guide/core/optimization_guide_features.h"
#include "components/prefs/pref_registry_simple.h"

namespace optimization_guide {
namespace prefs {

// A pref that stores the last time a hints fetch was attempted. This limits the
// frequency that hints are fetched and prevents a crash loop that continually
// fetches hints on startup.
const char kHintsFetcherLastFetchAttempt[] =
    "optimization_guide.hintsfetcher.last_fetch_attempt";

// A pref that stores the last time a prediction model and host model features
// fetch was attempted. This limits the frequency of fetching for updates and
// prevents a crash loop that continually fetches prediction models and host
// model features on startup.
const char kModelAndFeaturesLastFetchAttempt[] =
    "optimization_guide.predictionmodelfetcher.last_fetch_attempt";

// A pref that stores the last time a prediction model fetch was successful.
// This helps determine when to schedule the next fetch.
const char kModelLastFetchSuccess[] =
    "optimization_guide.predictionmodelfetcher.last_fetch_success";

// A dictionary pref that stores hosts that have had hints successfully fetched
// from the remote Optimization Guide Server. The entry for each host contains
// the time that the fetch that covered this host expires, i.e., any hints
// from the fetch would be considered stale.
const char kHintsFetcherHostsSuccessfullyFetched[] =
    "optimization_guide.hintsfetcher.hosts_successfully_fetched";

// A string pref that stores the version of the Optimization Hints component
// that is currently being processed. This pref is cleared once processing
// completes. It is used for detecting a potential crash loop on processing a
// version of hints.
const char kPendingHintsProcessingVersion[] =
    "optimization_guide.pending_hints_processing_version";

// A dictionary pref that stores optimization type was previously
// registered so that the first run of optimization types can be identified.
// The entry is the OptimizationType enum. The value of the key-value pair will
// not be used.
const char kPreviouslyRegisteredOptimizationTypes[] =
    "optimization_guide.previously_registered_optimization_types";

// A boolean pref that stores whether fetching is enabled. True by default.
const char kOptimizationGuideFetchingEnabled[] =
    "optimization_guide.fetching_enabled";

void RegisterProfilePrefs(PrefRegistrySimple* registry) {
  registry->RegisterInt64Pref(
      kHintsFetcherLastFetchAttempt,
      base::Time().ToDeltaSinceWindowsEpoch().InMicroseconds(),
      PrefRegistry::LOSSY_PREF);
  registry->RegisterInt64Pref(
      kModelAndFeaturesLastFetchAttempt,
      base::Time().ToDeltaSinceWindowsEpoch().InMicroseconds(),
      PrefRegistry::LOSSY_PREF);
  registry->RegisterInt64Pref(kModelLastFetchSuccess, 0,
                              PrefRegistry::LOSSY_PREF);
  registry->RegisterDictionaryPref(kHintsFetcherHostsSuccessfullyFetched,
                                   PrefRegistry::LOSSY_PREF);

  registry->RegisterStringPref(kPendingHintsProcessingVersion, "",
                               PrefRegistry::LOSSY_PREF);
  registry->RegisterDictionaryPref(kPreviouslyRegisteredOptimizationTypes,
                                   PrefRegistry::LOSSY_PREF);
  registry->RegisterBooleanPref(kOptimizationGuideFetchingEnabled, true);
}

}  // namespace prefs
}  // namespace optimization_guide
