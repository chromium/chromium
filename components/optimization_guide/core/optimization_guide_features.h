// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OPTIMIZATION_GUIDE_CORE_OPTIMIZATION_GUIDE_FEATURES_H_
#define COMPONENTS_OPTIMIZATION_GUIDE_CORE_OPTIMIZATION_GUIDE_FEATURES_H_

#include <string>
#include <utility>

#include "base/containers/flat_set.h"
#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"
#include "base/time/time.h"
#include "components/optimization_guide/proto/hints.pb.h"
#include "components/optimization_guide/proto/models.pb.h"
#include "net/nqe/effective_connection_type.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "url/gurl.h"

namespace optimization_guide {
namespace features {

extern const base::Feature kOptimizationHints;
extern const base::Feature kOptimizationHintsFieldTrials;
extern const base::Feature kRemoteOptimizationGuideFetching;
extern const base::Feature kRemoteOptimizationGuideFetchingAnonymousDataConsent;
extern const base::Feature kContextMenuPerformanceInfoAndRemoteHintFetching;
extern const base::Feature kOptimizationTargetPrediction;
extern const base::Feature kOptimizationGuideModelDownloading;
extern const base::Feature kPageContentAnnotations;
extern const base::Feature kPageTextExtraction;
extern const base::Feature kLoadModelFileForEachExecution;
extern const base::Feature kPushNotifications;

// The grace period duration for how long to give outstanding page text dump
// requests to respond after DidFinishLoad.
base::TimeDelta PageTextExtractionOutstandingRequestsGracePeriod();

// Whether hints for active tabs and top hosts should be batch updated.
bool ShouldBatchUpdateHintsForActiveTabsAndTopHosts();

// The maximum number of hosts allowed to be requested by the client to the
// remote Optimzation Guide Service.
size_t MaxHostsForOptimizationGuideServiceHintsFetch();

// The maximum number of URLs allowed to be requested by the client to the
// remote Optimzation Guide Service.
size_t MaxUrlsForOptimizationGuideServiceHintsFetch();

// The maximum number of hosts allowed to be stored as covered by the hints
// fetcher.
size_t MaxHostsForRecordingSuccessfullyCovered();

// The amount of time a fetched hint will be considered fresh enough
// to be used and remain in the OptimizationGuideStore.
base::TimeDelta StoredFetchedHintsFreshnessDuration();

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

// Returns true if a feature that explicitly allows remote fetching has been
// enabled.
bool IsRemoteFetchingExplicitlyAllowedForPerformanceInfo();

// Returns true if the feature to use push notifications is enabled.
bool IsPushNotificationsEnabled();

// The maximum data byte size for a server-provided bloom filter. This is
// a client-side safety limit for RAM use in case server sends too large of
// a bloom filter.
int MaxServerBloomFilterByteSize();

// Returns the duration of the time window before hints expiration during which
// the hosts should be refreshed. Example: If the hints for a host expire at
// time T, then they are eligible for refresh at T -
// GetHostHintsFetchRefreshDuration().
base::TimeDelta GetHostHintsFetchRefreshDuration();

// Returns the duration of the time window between fetches for hints for the
// URLs opened in active tabs.
base::TimeDelta GetActiveTabsFetchRefreshDuration();

// Returns the max duration since the time a tab has to be shown to be
// considered active for a hints refresh.
base::TimeDelta GetActiveTabsStalenessTolerance();

// Returns the max number of concurrent fetches to the remote Optimization Guide
// Service that should be allowed.
size_t MaxConcurrentPageNavigationFetches();

// Returns the minimum number of seconds to randomly delay before starting to
// fetch for hints for active tabs.
int ActiveTabsHintsFetchRandomMinDelaySecs();

// Returns the maximum number of seconds to randomly delay before starting to
// fetch for hints for active tabs.
int ActiveTabsHintsFetchRandomMaxDelaySecs();

// The amount of time host model features will be considered fresh enough
// to be used and remain in the OptimizationGuideStore.
base::TimeDelta StoredHostModelFeaturesFreshnessDuration();

// The maximum duration for which models can remain in the
// OptimizationGuideStore without being loaded.
base::TimeDelta StoredModelsInactiveDuration();

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

// Returns the time to wait before retrying a failed fetch for prediction
// models.
base::TimeDelta PredictionModelFetchRetryDelay();

// Returns the time to wait after a successful fetch of prediction models to
// refresh models.
base::TimeDelta PredictionModelFetchInterval();

// Returns a set of field trial name hashes that can be sent in the request to
// the remote Optimization Guide Service if the client is in one of the
// specified field trials.
base::flat_set<uint32_t> FieldTrialNameHashesAllowedForFetch();

// Whether the ability to download models is enabled.
bool IsModelDownloadingEnabled();

// Returns whether unrestricted model downloading is enabled. If true, the
// client should download models using highest priority.
bool IsUnrestrictedModelDownloadingEnabled();

// Returns whether the feature to annotate page content is enabled.
bool IsPageContentAnnotationEnabled();

// Returns the max size that should be requested for a page content text dump.
uint64_t MaxSizeForPageContentTextDump();

// Whether we should write content annotations to History Service.
bool ShouldWriteContentAnnotationsToHistoryService();

// Returns the max size of the MRU Cache of content that has been requested
// for annotation.
size_t MaxContentAnnotationRequestsCached();

// Returns whether or not related searches should be extracted from Google SRP
// as part of page content annotations.
bool ShouldExtractRelatedSearches();

// Returns an ordered vector of models to execute on the page content for each
// page load. It is guaranteed that an optimization target will only be present
// at most once in the returned vector. However, it is not guaranteed that it
// will only contain models that the current PageContentAnnotationsService
// supports, so it is up to the caller to ensure that it can execute the
// specified models.
std::vector<optimization_guide::proto::OptimizationTarget>
GetPageContentModelsToExecute();

// Whether the model files that use |OptimizationTargetModelExecutor| should be
// loaded for each execution, and then unloaded once complete.
bool LoadModelFileForEachExecution();

// The time to wait beyond the onload event before sending the hints request for
// link predictions.
base::TimeDelta GetOnloadDelayForHintsFetching();

}  // namespace features
}  // namespace optimization_guide

#endif  // COMPONENTS_OPTIMIZATION_GUIDE_CORE_OPTIMIZATION_GUIDE_FEATURES_H_
