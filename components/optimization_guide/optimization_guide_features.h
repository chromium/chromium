// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OPTIMIZATION_GUIDE_OPTIMIZATION_GUIDE_FEATURES_H_
#define COMPONENTS_OPTIMIZATION_GUIDE_OPTIMIZATION_GUIDE_FEATURES_H_

#include <string>
#include <utility>

#include "base/containers/flat_set.h"
#include "base/feature_list.h"
#include "base/optional.h"
#include "base/time/time.h"
#include "components/optimization_guide/proto/models.pb.h"
#include "net/nqe/effective_connection_type.h"
#include "url/gurl.h"

namespace optimization_guide {
namespace features {

extern const base::Feature kOptimizationHints;
extern const base::Feature kOptimizationHintsFieldTrials;
extern const base::Feature kRemoteOptimizationGuideFetching;
extern const base::Feature kRemoteOptimizationGuideFetchingAnonymousDataConsent;
extern const base::Feature kOptimizationTargetPrediction;
extern const base::Feature kOptimizationTargetPredictionUsingMLService;

// The maximum number of hosts that can be stored in the
// |kHintsFetcherTopHostBlacklist| dictionary pref when initialized. The top
// hosts will also be returned in order of most engaged. This prevents the most
// engaged hosts in a user's history before DataSaver being enabled from being
// requested until the user navigates to the host again.
size_t MaxHintsFetcherTopHostBlacklistSize();

// Whether hints for top hosts should be batch updated.
bool ShouldBatchUpdateHintsForTopHosts();

// The maximum number of hosts allowed to be requested by the client to the
// remote Optimzation Guide Service.
size_t MaxHostsForOptimizationGuideServiceHintsFetch();

// The maximum number of URLs allowed to be requested by the client to the
// remote Optimzation Guide Service.
size_t MaxUrlsForOptimizationGuideServiceHintsFetch();

// The maximum number of hosts allowed to be stored as covered by the hints
// fetcher.
size_t MaxHostsForRecordingSuccessfullyCovered();

// The minimum score required to be considered a top host and be included in a
// hints fetch request.
double MinTopHostEngagementScoreThreshold();

// The amount of time a fetched hint will be considered fresh enough
// to be used and remain in the OptimizationGuideStore.
base::TimeDelta StoredFetchedHintsFreshnessDuration();

// The duration of time after the blacklist initialization for which the low
// engagement score threshold needs to be applied. If the blacklist was
// initialized more than DurationApplyLowEngagementScoreThreshold() ago, then
// the low engagement score threshold need not be applied.
base::TimeDelta DurationApplyLowEngagementScoreThreshold();

// The API key for the One Platform Optimization Guide Service.
std::string GetOptimizationGuideServiceAPIKey();

// The host for the One Platform Optimization Guide Service for hints.
GURL GetOptimizationGuideServiceGetHintsURL();

// The host for the One Platform Optimization Guide Service for Models and Host
// Model Features.
GURL GetOptimizationGuideServiceGetModelsURL();

// Whether server optimization hints are enabled.
bool IsOptimizationHintsEnabled();

// Returns true if the feature to fetch from the remote Optimization Guide
// Service is enabled.
bool IsRemoteFetchingEnabled();

// Returns true if the feature to fetch data for users that have consented to
// anonymous data collection is enabled but are not Data Saver users.
bool IsRemoteFetchingForAnonymousDataConsentEnabled();

// The maximum data byte size for a server-provided bloom filter. This is
// a client-side safety limit for RAM use in case server sends too large of
// a bloom filter.
int MaxServerBloomFilterByteSize();

// Maximum effective connection type at which hints can be fetched for
// navigations in real-time. Returns null if the hints fetching for navigations
// is disabled.
base::Optional<net::EffectiveConnectionType>
GetMaxEffectiveConnectionTypeForNavigationHintsFetch();

// Returns the duration of the time window before hints expiration during which
// the hosts should be refreshed. Example: If the hints for a host expire at
// time T, then they are eligible for refresh at T -
// GetHintsFetchRefreshDuration().
base::TimeDelta GetHintsFetchRefreshDuration();

// Returns the max number of concurrent fetches to the remote Optimization Guide
// Service that should be allowed.
size_t MaxConcurrentPageNavigationFetches();

// The amount of time host model features will be considered fresh enough
// to be used and remain in the OptimizationGuideStore.
base::TimeDelta StoredHostModelFeaturesFreshnessDuration();

// The amount of time URL-keyed hints within the hint cache will be
// allowed to be used and not be purged.
base::TimeDelta URLKeyedHintValidCacheDuration();

// The maximum number of hosts allowed to be requested by the client to the
// remote Optimzation Guide Service for use by prediction models.
size_t MaxHostsForOptimizationGuideServiceModelsFetch();

// The maximum number of hosts allowed to be maintained in a least-recently-used
// cache by the prediction manager.
size_t MaxHostModelFeaturesCacheSize();

// The maximum number of hints allowed to be maintained in a least-recently-used
// cache for hosts.
size_t MaxHostKeyedHintCacheSize();

// The maximum number of hints allowed to be maintained in a least-recently-used
// cache for URLs.
size_t MaxURLKeyedHintCacheSize();

// Returns true if hints should be persisted to disk. If this is false, hints
// will just be stored in-memory and evicted if not recently used.
bool ShouldPersistHintsToDisk();

// Returns true if the optimization target decision for |optimization_target|
// should not be propagated to the caller in an effort to fully understand the
// statistics for the served model and not taint the resulting data.
bool ShouldOverrideOptimizationTargetDecisionForMetricsPurposes(
    proto::OptimizationTarget optimization_target);

// Returns the minimum number of seconds to randomly delay before starting to
// fetch for prediction models and host model features.
int PredictionModelFetchRandomMinDelaySecs();

// Returns the maximum number of seconds to randomly delay before starting to
// fetch for prediction models and host model features.
int PredictionModelFetchRandomMaxDelaySecs();

// Returns a set of external Android app packages whose predictions have been
// approved for fetching from the remote Optimization Guide Service.
base::flat_set<std::string> ExternalAppPackageNamesApprovedForFetch();

// Returns a set of field trial name hashes that can be sent in the request to
// the remote Optimization Guide Service if the client is in one of the
// specified field trials.
base::flat_set<uint32_t> FieldTrialNameHashesAllowedForFetch();

// Whether out-of-process model evaluation via the ML Service is enabled.
bool ShouldUseMLServiceForPrediction();

}  // namespace features
}  // namespace optimization_guide

#endif  // COMPONENTS_OPTIMIZATION_GUIDE_OPTIMIZATION_GUIDE_FEATURES_H_
