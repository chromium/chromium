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

// A dictionary pref that stores the set of hosts that cannot have hints fetched
// for until visited again after fetching from the remote Optimization Guide
// Service was first allowed. If The hash of the host is in the dictionary, then
// it is on the blocklist and should not be used, the |value| in the key-value
// pair is not used.
const char kHintsFetcherTopHostBlocklist[] =
    "optimization_guide.hintsfetcher.top_host_blacklist";

// An integer pref that stores the state of the blocklist for the top host
// provider for blocklisting hosts after fetching from the remote Optimization
// Guide Service was first allowed. The state maps to the
// HintsFetcherTopHostBlocklistState enum.
const char kHintsFetcherTopHostBlocklistState[] =
    "optimization_guide.hintsfetcher.top_host_blacklist_state";

// Time when the top host blocklist was last initialized. Recorded as seconds
// since epoch.
const char kTimeHintsFetcherTopHostBlocklistLastInitialized[] =
    "optimization_guide.hintsfetcher.time_blacklist_last_initialized";

// If a host has site engagement score less than the value stored in this pref,
// then hints fetcher may not fetch hints for that host.
const char kHintsFetcherTopHostBlocklistMinimumEngagementScore[] =
    "optimization_guide.hintsfetcher.top_host_blacklist_min_engagement_score";

// A dictionary pref that stores hosts that have had hints successfully fetched
// from the remote Optimization Guide Server. The entry for each host contains
// the time that the fetch that covered this host expires, i.e., any hints
// from the fetch would be considered stale.
const char kHintsFetcherHostsSuccessfullyFetched[] =
    "optimization_guide.hintsfetcher.hosts_successfully_fetched";

// A double pref that stores the running mean FCP.
const char kSessionStatisticFCPMean[] =
    "optimization_guide.session_statistic.fcp_mean";

// A double pref that stores the running FCP standard deviation.
const char kSessionStatisticFCPStdDev[] =
    "optimization_guide.session_statistic.fcp_std_dev";

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

void RegisterProfilePrefs(PrefRegistrySimple* registry) {
  registry->RegisterInt64Pref(
      kHintsFetcherLastFetchAttempt,
      base::Time().ToDeltaSinceWindowsEpoch().InMicroseconds(),
      PrefRegistry::LOSSY_PREF);
  registry->RegisterInt64Pref(
      kModelAndFeaturesLastFetchAttempt,
      base::Time().ToDeltaSinceWindowsEpoch().InMicroseconds(),
      PrefRegistry::LOSSY_PREF);
  registry->RegisterDictionaryPref(kHintsFetcherTopHostBlocklist,
                                   PrefRegistry::LOSSY_PREF);
  registry->RegisterDictionaryPref(kHintsFetcherHostsSuccessfullyFetched,
                                   PrefRegistry::LOSSY_PREF);
  registry->RegisterIntegerPref(
      kHintsFetcherTopHostBlocklistState,
      static_cast<int>(HintsFetcherTopHostBlocklistState::kNotInitialized),
      PrefRegistry::LOSSY_PREF);
  registry->RegisterDoublePref(kTimeHintsFetcherTopHostBlocklistLastInitialized,
                               0, PrefRegistry::LOSSY_PREF);

  registry->RegisterDoublePref(kSessionStatisticFCPMean, 0,
                               PrefRegistry::LOSSY_PREF);
  registry->RegisterDoublePref(kSessionStatisticFCPStdDev, 0,
                               PrefRegistry::LOSSY_PREF);
  // Use a default value of MinTopHostEngagementScoreThreshold() for the
  // threshold. This ensures that the users for which this pref can't be
  // computed (possibly because they had the blocklist initialized before this
  // pref was added to the code) use the default value for the site engagement
  // threshold.
  registry->RegisterDoublePref(
      kHintsFetcherTopHostBlocklistMinimumEngagementScore,
      optimization_guide::features::MinTopHostEngagementScoreThreshold(),
      PrefRegistry::LOSSY_PREF);

  registry->RegisterStringPref(kPendingHintsProcessingVersion, "",
                               PrefRegistry::LOSSY_PREF);
  registry->RegisterDictionaryPref(kPreviouslyRegisteredOptimizationTypes,
                                   PrefRegistry::LOSSY_PREF);
}

}  // namespace prefs
}  // namespace optimization_guide
