// Copyright 2019 The Chromium Authors
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
#include "components/optimization_guide/core/page_content_annotation_type.h"
#include "components/optimization_guide/proto/hints.pb.h"
#include "components/optimization_guide/proto/models.pb.h"
#include "net/nqe/effective_connection_type.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "url/gurl.h"

namespace optimization_guide {
namespace features {

BASE_DECLARE_FEATURE(kOptimizationHints);
BASE_DECLARE_FEATURE(kRemoteOptimizationGuideFetching);
BASE_DECLARE_FEATURE(kRemoteOptimizationGuideFetchingAnonymousDataConsent);
BASE_DECLARE_FEATURE(kOptimizationGuideFetchingForSRP);
BASE_DECLARE_FEATURE(kContextMenuPerformanceInfoAndRemoteHintFetching);
BASE_DECLARE_FEATURE(kOptimizationTargetPrediction);
BASE_DECLARE_FEATURE(kOptimizationGuideModelDownloading);
BASE_DECLARE_FEATURE(kPageContentAnnotations);
BASE_DECLARE_FEATURE(kPageEntitiesPageContentAnnotations);
BASE_DECLARE_FEATURE(kPageEntitiesModelBypassFilters);
BASE_DECLARE_FEATURE(kPageEntitiesModelResetOnShutdown);
BASE_DECLARE_FEATURE(kPageVisibilityPageContentAnnotations);
BASE_DECLARE_FEATURE(kTextEmbeddingPageContentAnnotations);
BASE_DECLARE_FEATURE(kPageTextExtraction);
BASE_DECLARE_FEATURE(kPushNotifications);
BASE_DECLARE_FEATURE(kOptimizationGuideMetadataValidation);
BASE_DECLARE_FEATURE(kPageVisibilityBatchAnnotations);
BASE_DECLARE_FEATURE(kTextEmbeddingBatchAnnotations);
BASE_DECLARE_FEATURE(kPageContentAnnotationsValidation);
BASE_DECLARE_FEATURE(kPreventLongRunningPredictionModels);
BASE_DECLARE_FEATURE(kOverrideNumThreadsForModelExecution);
BASE_DECLARE_FEATURE(kOptGuideEnableXNNPACKDelegateWithTFLite);
BASE_DECLARE_FEATURE(kRemotePageMetadata);
BASE_DECLARE_FEATURE(kOptimizationHintsComponent);
BASE_DECLARE_FEATURE(kOptimizationGuideInstallWideModelStore);
BASE_DECLARE_FEATURE(kExtractRelatedSearchesFromPrefetchedZPSResponse);
BASE_DECLARE_FEATURE(kPageContentAnnotationsPersistSalientImageMetadata);
BASE_DECLARE_FEATURE(kModelStoreUseRelativePath);
BASE_DECLARE_FEATURE(kOptimizationGuidePersonalizedFetching);
BASE_DECLARE_FEATURE(kOptimizationGuideHintsURLKeyedCacheDropFragments);
BASE_DECLARE_FEATURE(kQueryInMemoryTextEmbeddings);
BASE_DECLARE_FEATURE(kOptimizationGuidePredictionModelKillswitch);

// Enables use of task runner with trait CONTINUE_ON_SHUTDOWN for page content
// annotations on-device models.
BASE_DECLARE_FEATURE(
    kOptimizationGuideUseContinueOnShutdownForPageContentAnnotations);

// The maximum number of "related searches" entries allowed to be maintained in
// a least-recently-used cache for "related searches" data obtained via ZPS
// prefetch logic.
size_t MaxRelatedSearchesCacheSize();

// The grace period duration for how long to give outstanding page text dump
// requests to respond after DidFinishLoad.
base::TimeDelta PageTextExtractionOutstandingRequestsGracePeriod();

// Whether hints for active tabs and top hosts should be batch updated.
bool ShouldBatchUpdateHintsForActiveTabsAndTopHosts();

// The maximum number of hosts allowed to be requested by the client to the
// remote Optimization Guide Service.
size_t MaxHostsForOptimizationGuideServiceHintsFetch();

// The maximum number of URLs allowed to be requested by the client to the
// remote Optimization Guide Service.
size_t MaxUrlsForOptimizationGuideServiceHintsFetch();

// Whether hints fetching for search results is enabled.
bool IsSRPFetchingEnabled();
// The maximum number of search results allowed to be requested by the client to
// the remote Optimization Guide Service.
size_t MaxResultsForSRPFetch();

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

// Whether prediction of optimization targets is enabled.
bool IsOptimizationTargetPredictionEnabled();

// Whether server optimization hints are enabled.
bool IsOptimizationHintsEnabled();

// Returns true if the feature to fetch from the remote Optimization Guide
// Service is enabled. This controls the fetching of both hints and models.
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
// Service that should be allowed for batch updates
size_t MaxConcurrentBatchUpdateFetches();

// Returns the max number of concurrent fetches to the remote Optimization Guide
// Service that should be allowed for navigations.
size_t MaxConcurrentPageNavigationFetches();

// Returns the minimum number of seconds to randomly delay before starting to
// fetch for hints for active tabs.
int ActiveTabsHintsFetchRandomMinDelaySecs();

// Returns the maximum number of seconds to randomly delay before starting to
// fetch for hints for active tabs.
int ActiveTabsHintsFetchRandomMaxDelaySecs();

// Returns whether fetching hints for active tabs should happen on deferred
// startup. Otherwise active tabs hints will be fetched after a random interval
// between ActiveTabsHintsFetchRandomMinDelaySecs() and
// ActiveTabsHintsFetchRandomMaxDelaySecs().
bool ShouldDeferStartupActiveTabsHintsFetch();

// The amount of time host model features will be considered fresh enough
// to be used and remain in the OptimizationGuideStore.
base::TimeDelta StoredHostModelFeaturesFreshnessDuration();

// The maximum duration for which models can remain in the
// OptimizationGuideStore without being loaded.
base::TimeDelta StoredModelsValidDuration();

// The amount of time URL-keyed hints within the hint cache will be
// allowed to be used and not be purged.
base::TimeDelta URLKeyedHintValidCacheDuration();

// The amount of time the PCAService will wait for the title of a page to be
// modified.
base::TimeDelta PCAServiceWaitForTitleDelayDuration();

// The maximum number of hosts allowed to be requested by the client to the
// remote Optimization Guide Service for use by prediction models.
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

// Returns whether personalized metadata should be enabled for
// |request_context|.
bool ShouldEnablePersonalizedMetadata(proto::RequestContext request_context);

// Returns the OAuth scopes to use for personalized metadata.
std::set<std::string> GetOAuthScopesForPersonalizedMetadata();

// Returns the minimum number of seconds to randomly delay before starting to
// fetch for prediction models and host model features.
int PredictionModelFetchRandomMinDelaySecs();

// Returns the maximum number of seconds to randomly delay before starting to
// fetch for prediction models and host model features.
int PredictionModelFetchRandomMaxDelaySecs();

// Returns the time to wait before retrying a failed fetch for prediction
// models.
base::TimeDelta PredictionModelFetchRetryDelay();

// Returns the time to wait after browser start before fetching prediciton
// models.
base::TimeDelta PredictionModelFetchStartupDelay();

// Returns the time to wait after a successful fetch of prediction models to
// refresh models.
base::TimeDelta PredictionModelFetchInterval();

// Returns whether to enable fetching the model again when a new optimization
// target observer registration happens, after the initial model fetch is
// completed.
bool IsPredictionModelNewRegistrationFetchEnabled();

// Returns the time to wait for starting a model fetch when a new optimization
// target observer registration happens, after the initial model fetch is
// completed.
base::TimeDelta PredictionModelNewRegistrationFetchDelay();

// Whether to use the model execution watchdog.
bool IsModelExecutionWatchdogEnabled();

// The default timeout for the watchdog to use if none is given by the caller.
base::TimeDelta ModelExecutionWatchdogDefaultTimeout();

// Whether the ability to download models is enabled.
bool IsModelDownloadingEnabled();

// Returns whether unrestricted model downloading is enabled. If true, the
// client should download models using highest priority.
bool IsUnrestrictedModelDownloadingEnabled();

// Returns whether the feature to annotate page content is enabled.
bool IsPageContentAnnotationEnabled();

// Whether we should write content annotations to History Service.
bool ShouldWriteContentAnnotationsToHistoryService();

// Returns the max size of the MRU Cache of content that has been requested
// for annotation.
size_t MaxContentAnnotationRequestsCached();

// Returns whether or not related searches should be extracted from Google SRP
// as part of page content annotations.
bool ShouldExtractRelatedSearches();

// Returns whether the page entities model should be executed on page content
// for a user using |locale| as their browser language.
bool ShouldExecutePageEntitiesModelOnPageContent(const std::string& locale);

// Returns whether the page visibility model should be executed on page content
// for a user using |locale| as their browser language.
bool ShouldExecutePageVisibilityModelOnPageContent(const std::string& locale);

// Returns whether the text embedding model should be executed on page content
// for a user using |locale| as their browser language.
bool ShouldExecuteTextEmbeddingModelOnPageContent(const std::string& locale);

// Returns whether page metadata should be retrieved from the remote
// Optimization Guide service.
bool RemotePageMetadataEnabled();

// Returns the minimum score associated with a category for it to be persisted.
// Will be a value from 0 to 100, inclusive.
int GetMinimumPageCategoryScoreToPersist();

// The time to wait beyond the onload event before sending the hints request for
// link predictions.
base::TimeDelta GetOnloadDelayForHintsFetching();

// The number of bits used for RAPPOR-style metrics reporting on content
// annotation models. Must be at least 1 bit.
int NumBitsForRAPPORMetrics();

// The probability of a bit flip a score with RAPPOR-style metrics reporting.
// Must be between 0 and 1.
double NoiseProbabilityForRAPPORMetrics();

// Returns whether the metadata validation fetch feature is host keyed.
bool ShouldMetadataValidationFetchHostKeyed();

// Returns if Page Visibility Batch Annotations are enabled.
bool PageVisibilityBatchAnnotationsEnabled();

// Returns if Text Embedding Batch Annotations are enabled.
bool TextEmbeddingBatchAnnotationsEnabled();

// The number of visits batch before running the page content annotation
// models. A size of 1 is equivalent to annotating one page load at time
// immediately after requested.
size_t AnnotateVisitBatchSize();

// Whether the page content annotation validation feature or command line flag
// is enabled for the given annotation type.
bool PageContentAnnotationValidationEnabledForType(AnnotationType type);

// The time period between browser start and running a running page content
// annotation validation.
base::TimeDelta PageContentAnnotationValidationStartupDelay();

// The size of batches to run for page content validation.
size_t PageContentAnnotationsValidationBatchSize();

// The maximum size of the visit annotation cache.
size_t MaxVisitAnnotationCacheSize();

// Returns the number of threads to use for model inference on the given
// optimization target.
absl::optional<int> OverrideNumThreadsForOptTarget(
    proto::OptimizationTarget opt_target);

// Whether XNNPACK should be used with TFLite, on platforms where it is
// supported. This is a no-op on unsupported platforms.
bool TFLiteXNNPACKDelegateEnabled();

// Whether to check the pref for whether a previous component version failed.
bool ShouldCheckFailedComponentVersionPref();

// Returns whether the feature for new model store that is tied with Chrome
// installation and shares the models across user profiles, is enabled.
bool IsInstallWideModelStoreEnabled();

// Whether to persist salient image metadata for each visit.
bool ShouldPersistSalientImageMetadata();

// Whether to drop fragments for the URL-keyed hint cache key.
bool ShouldDropFragmentsForURLKeyedHintCacheKey();

// Returns whether to query text embeddings coming from history service.
bool ShouldQueryEmbeddings();

// Returns whether the `model_version` for `opt_target` is part of emergency
// killswitch, and this model should be stopped serving immediately.
std::map<proto::OptimizationTarget, std::set<int64_t>>
GetPredictionModelVersionsInKillSwitch();

}  // namespace features
}  // namespace optimization_guide

#endif  // COMPONENTS_OPTIMIZATION_GUIDE_CORE_OPTIMIZATION_GUIDE_FEATURES_H_
