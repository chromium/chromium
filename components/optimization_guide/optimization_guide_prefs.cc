// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/optimization_guide/optimization_guide_prefs.h"

#include "components/optimization_guide/optimization_guide_features.h"
#include "components/prefs/pref_registry_simple.h"

namespace optimization_guide {
namespace prefs {

// A pref that stores the last time a hints fetch was attempted. This limits the
// frequency that hints are fetched and prevents a crash loop that continually
// fetches hints on startup.
const char kHintsFetcherLastFetchAttempt[] =
    "optimization_guide.hintsfetcher.last_fetch_attempt";

// A dictionary pref that stores the set of hosts that cannot have hints fetched
// for until visited again after DataSaver was enabled. If The hash of the host
// is in the dictionary, then it is on the blacklist and should not be used, the
// |value| in the key-value pair is not used.
const char kHintsFetcherDataSaverTopHostBlacklist[] =
    "optimization_guide.hintsfetcher.top_host_blacklist";

// An integer pref that stores the state of the blacklist for the top host
// provider for blacklisting hosts after DataSaver is enabled. The state maps to
// the HintsFetcherTopHostBlacklistState enum.
const char kHintsFetcherDataSaverTopHostBlacklistState[] =
    "optimization_guide.hintsfetcher.top_host_blacklist_state";

// Time when the blacklist was last initialized. Recorded as seconds since
// epoch.
const char kTimeBlacklistLastInitialized[] =
    "optimization_guide.hintsfetcher.time_blacklist_last_initialized";

// If a host has site engagement score less than the value stored in this pref,
// then hints fetcher may not fetch hints for that host.
const char kHintsFetcherDataSaverTopHostBlacklistMinimumEngagementScore[] =
    "optimization_guide.hintsfetcher.top_host_blacklist_min_engagement_score";

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

void RegisterProfilePrefs(PrefRegistrySimple* registry) {
  registry->RegisterInt64Pref(
      kHintsFetcherLastFetchAttempt,
      base::Time().ToDeltaSinceWindowsEpoch().InMicroseconds(),
      PrefRegistry::LOSSY_PREF);
  registry->RegisterDictionaryPref(kHintsFetcherDataSaverTopHostBlacklist,
                                   PrefRegistry::LOSSY_PREF);
  registry->RegisterDictionaryPref(kHintsFetcherHostsSuccessfullyFetched,
                                   PrefRegistry::LOSSY_PREF);
  registry->RegisterIntegerPref(
      kHintsFetcherDataSaverTopHostBlacklistState,
      static_cast<int>(HintsFetcherTopHostBlacklistState::kNotInitialized),
      PrefRegistry::LOSSY_PREF);
  registry->RegisterDoublePref(kTimeBlacklistLastInitialized, 0,
                               PrefRegistry::LOSSY_PREF);

  // Use a default value of MinTopHostEngagementScoreThreshold() for the
  // threshold. This ensures that the users for which this pref can't be
  // computed (possibly because they had the blacklist initialized before this
  // pref was added to the code) use the default value for the site engagement
  // threshold.
  registry->RegisterDoublePref(
      kHintsFetcherDataSaverTopHostBlacklistMinimumEngagementScore,
      optimization_guide::features::MinTopHostEngagementScoreThreshold(),
      PrefRegistry::LOSSY_PREF);

  registry->RegisterStringPref(kPendingHintsProcessingVersion, "",
                               PrefRegistry::LOSSY_PREF);
}

}  // namespace prefs
}  // namespace optimization_guide
