// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OPTIMIZATION_GUIDE_CORE_OPTIMIZATION_GUIDE_FEATURES_H_
#define COMPONENTS_OPTIMIZATION_GUIDE_CORE_OPTIMIZATION_GUIDE_FEATURES_H_

#include <string>
#include <utility>

#include "base/component_export.h"
#include "base/containers/flat_set.h"
#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"
#include "base/time/time.h"
#include "components/optimization_guide/core/page_content_annotation_type.h"
#include "components/optimization_guide/proto/hints.pb.h"
#include "components/optimization_guide/proto/model_execution.pb.h"
#include "components/optimization_guide/proto/models.pb.h"
#include "net/nqe/effective_connection_type.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "url/gurl.h"

namespace optimization_guide {
namespace features {

COMPONENT_EXPORT(OPTIMIZATION_GUIDE_FEATURES)
BASE_DECLARE_FEATURE(kOptimizationHints);
COMPONENT_EXPORT(OPTIMIZATION_GUIDE_FEATURES)
BASE_DECLARE_FEATURE(kRemoteOptimizationGuideFetching);
COMPONENT_EXPORT(OPTIMIZATION_GUIDE_FEATURES)
BASE_DECLARE_FEATURE(kRemoteOptimizationGuideFetchingAnonymousDataConsent);
COMPONENT_EXPORT(OPTIMIZATION_GUIDE_FEATURES)
BASE_DECLARE_FEATURE(kOptimizationGuideFetchingForSRP);
COMPONENT_EXPORT(OPTIMIZATION_GUIDE_FEATURES)
BASE_DECLARE_FEATURE(kContextMenuPerformanceInfoAndRemoteHintFetching);
COMPONENT_EXPORT(OPTIMIZATION_GUIDE_FEATURES)
BASE_DECLARE_FEATURE(kOptimizationTargetPrediction);
COMPONENT_EXPORT(OPTIMIZATION_GUIDE_FEATURES)
BASE_DECLARE_FEATURE(kOptimizationGuideModelDownloading);
COMPONENT_EXPORT(OPTIMIZATION_GUIDE_FEATURES)
BASE_DECLARE_FEATURE(kPageContentAnnotations);
COMPONENT_EXPORT(OPTIMIZATION_GUIDE_FEATURES)
BASE_DECLARE_FEATURE(kPageEntitiesPageContentAnnotations);
COMPONENT_EXPORT(OPTIMIZATION_GUIDE_FEATURES)
BASE_DECLARE_FEATURE(kPageEntitiesModelBypassFilters);
COMPONENT_EXPORT(OPTIMIZATION_GUIDE_FEATURES)
BASE_DECLARE_FEATURE(kPageEntitiesModelResetOnShutdown);
COMPONENT_EXPORT(OPTIMIZATION_GUIDE_FEATURES)
BASE_DECLARE_FEATURE(kPageVisibilityPageContentAnnotations);
COMPONENT_EXPORT(OPTIMIZATION_GUIDE_FEATURES)
BASE_DECLARE_FEATURE(kTextEmbeddingPageContentAnnotations);
COMPONENT_EXPORT(OPTIMIZATION_GUIDE_FEATURES)
BASE_DECLARE_FEATURE(kPageTextExtraction);
COMPONENT_EXPORT(OPTIMIZATION_GUIDE_FEATURES)
BASE_DECLARE_FEATURE(kPushNotifications);
COMPONENT_EXPORT(OPTIMIZATION_GUIDE_FEATURES)
BASE_DECLARE_FEATURE(kOptimizationGuideMetadataValidation);
COMPONENT_EXPORT(OPTIMIZATION_GUIDE_FEATURES)
BASE_DECLARE_FEATURE(kPageVisibilityBatchAnnotations);
COMPONENT_EXPORT(OPTIMIZATION_GUIDE_FEATURES)
BASE_DECLARE_FEATURE(kTextEmbeddingBatchAnnotations);
COMPONENT_EXPORT(OPTIMIZATION_GUIDE_FEATURES)
BASE_DECLARE_FEATURE(kPageContentAnnotationsValidation);
COMPONENT_EXPORT(OPTIMIZATION_GUIDE_FEATURES)
BASE_DECLARE_FEATURE(kPreventLongRunningPredictionModels);
COMPONENT_EXPORT(OPTIMIZATION_GUIDE_FEATURES)
BASE_DECLARE_FEATURE(kOverrideNumThreadsForModelExecution);
COMPONENT_EXPORT(OPTIMIZATION_GUIDE_FEATURES)
BASE_DECLARE_FEATURE(kOptGuideEnableXNNPACKDelegateWithTFLite);
COMPONENT_EXPORT(OPTIMIZATION_GUIDE_FEATURES)
BASE_DECLARE_FEATURE(kRemotePageMetadata);
COMPONENT_EXPORT(OPTIMIZATION_GUIDE_FEATURES)
BASE_DECLARE_FEATURE(kOptimizationHintsComponent);
COMPONENT_EXPORT(OPTIMIZATION_GUIDE_FEATURES)
BASE_DECLARE_FEATURE(kOptimizationGuideInstallWideModelStore);
COMPONENT_EXPORT(OPTIMIZATION_GUIDE_FEATURES)
BASE_DECLARE_FEATURE(kExtractRelatedSearchesFromPrefetchedZPSResponse);
COMPONENT_EXPORT(OPTIMIZATION_GUIDE_FEATURES)
BASE_DECLARE_FEATURE(kPageContentAnnotationsPersistSalientImageMetadata);
COMPONENT_EXPORT(OPTIMIZATION_GUIDE_FEATURES)
BASE_DECLARE_FEATURE(kModelStoreUseRelativePath);
COMPONENT_EXPORT(OPTIMIZATION_GUIDE_FEATURES)
BASE_DECLARE_FEATURE(kOptimizationGuidePersonalizedFetching);
COMPONENT_EXPORT(OPTIMIZATION_GUIDE_FEATURES)
BASE_DECLARE_FEATURE(kQueryInMemoryTextEmbeddings);
COMPONENT_EXPORT(OPTIMIZATION_GUIDE_FEATURES)
BASE_DECLARE_FEATURE(kOptimizationGuidePredictionModelKillswitch);
COMPONENT_EXPORT(OPTIMIZATION_GUIDE_FEATURES)
BASE_DECLARE_FEATURE(kOptimizationGuideModelExecution);
COMPONENT_EXPORT(OPTIMIZATION_GUIDE_FEATURES)
BASE_DECLARE_FEATURE(kOptimizationGuideOnDeviceModel);
COMPONENT_EXPORT(OPTIMIZATION_GUIDE_FEATURES)
BASE_DECLARE_FEATURE(kModelQualityLogging);
COMPONENT_EXPORT(OPTIMIZATION_GUIDE_FEATURES)
BASE_DECLARE_FEATURE(kLogOnDeviceMetricsOnStartup);

// Enables use of task runner with trait CONTINUE_ON_SHUTDOWN for page content
// annotations on-device models.
COMPONENT_EXPORT(OPTIMIZATION_GUIDE_FEATURES)
BASE_DECLARE_FEATURE(
    kOptimizationGuideUseContinueOnShutdownForPageContentAnnotations);

// The maximum number of "related searches" entries allowed to be maintained in
// a least-recently-used cache for "related searches" data obtained via ZPS
// prefetch logic.
COMPONENT_EXPORT(OPTIMIZATION_GUIDE_FEATURES)
size_t MaxRelatedSearchesCacheSize();

// The grace period duration for how long to give outstanding page text dump
// requests to respond after DidFinishLoad.
COMPONENT_EXPORT(OPTIMIZATION_GUIDE_FEATURES)
base::TimeDelta PageTextExtractionOutstandingRequestsGracePeriod();

// Whether hints for active tabs and top hosts should be batch updated.
COMPONENT_EXPORT(OPTIMIZATION_GUIDE_FEATURES)
bool ShouldBatchUpdateHintsForActiveTabsAndTopHosts();

// The maximum number of hosts allowed to be requested by the client to the
// remote Optimization Guide Service.
COMPONENT_EXPORT(OPTIMIZATION_GUIDE_FEATURES)
size_t MaxHostsForOptimizationGuideServiceHintsFetch();

// The maximum number of URLs allowed to be requested by the client to the
// remote Optimization Guide Service.
COMPONENT_EXPORT(OPTIMIZATION_GUIDE_FEATURES)
size_t MaxUrlsForOptimizationGuideServiceHintsFetch();

// Whether hints fetching for search results is enabled.
COMPONENT_EXPORT(OPTIMIZATION_GUIDE_FEATURES)
bool IsSRPFetchingEnabled();
// The maximum number of search results allowed to be requested by the client to
// the remote Optimization Guide Service.
COMPONENT_EXPORT(OPTIMIZATION_GUIDE_FEATURES)
size_t MaxResultsForSRPFetch();

// The maximum number of hosts allowed to be stored as covered by the hints
// fetcher.
COMPONENT_EXPORT(OPTIMIZATION_GUIDE_FEATURES)
size_t MaxHostsForRecordingSuccessfullyCovered();

// The amount of time a fetched hint will be considered fresh enough
// to be used and remain in the OptimizationGuideStore.
COMPONENT_EXPORT(OPTIMIZATION_GUIDE_FEATURES)
base::TimeDelta StoredFetchedHintsFreshnessDuration();

// The API key for the One Platform Optimization Guide Service.
COMPONENT_EXPORT(OPTIMIZATION_GUIDE_FEATURES)
std::string GetOptimizationGuideServiceAPIKey();

// The host for the One Platform Optimization Guide Service for hints.
COMPONENT_EXPORT(OPTIMIZATION_GUIDE_FEATURES)
GURL GetOptimizationGuideServiceGetHintsURL();

// The host for the One Platform Optimization Guide Service for Models and Host
// Model Features.
COMPONENT_EXPORT(OPTIMIZATION_GUIDE_FEATURES)
GURL GetOptimizationGuideServiceGetModelsURL();

// Whether prediction of optimization targets is enabled.
COMPONENT_EXPORT(OPTIMIZATION_GUIDE_FEATURES)
bool IsOptimizationTargetPredictionEnabled();

// Whether server optimization hints are enabled.
COMPONENT_EXPORT(OPTIMIZATION_GUIDE_FEATURES)
bool IsOptimizationHintsEnabled();

// Returns true if the feature to fetch from the remote Optimization Guide
// Service is enabled. This controls the fetching of both hints and models.
COMPONENT_EXPORT(OPTIMIZATION_GUIDE_FEATURES)
bool IsRemoteFetchingEnabled();

// Returns true if the feature to fetch data for users that have consented to
// anonymous data collection is enabled but are not Data Saver users.
COMPONENT_EXPORT(OPTIMIZATION_GUIDE_FEATURES)
bool IsRemoteFetchingForAnonymousDataConsentEnabled();

// Returns true if a feature that explicitly allows remote fetching has been
// enabled.
COMPONENT_EXPORT(OPTIMIZATION_GUIDE_FEATURES)
bool IsRemoteFetchingExplicitlyAllowedForPerformanceInfo();

// Returns true if the feature to use push notifications is enabled.
COMPONENT_EXPORT(OPTIMIZATION_GUIDE_FEATURES)
bool IsPushNotificationsEnabled();

// The maximum data byte size for a server-provided bloom filter. This is
// a client-side safety limit for RAM use in case server sends too large of
// a bloom filter.
COMPONENT_EXPORT(OPTIMIZATION_GUIDE_FEATURES)
int MaxServerBloomFilterByteSize();

// Returns the duration of the time window before hints expiration during which
// the hosts should be refreshed. Example: If the hints for a host expire at
// time T, then they are eligible for refresh at T -
// GetHostHintsFetchRefreshDuration().
COMPONENT_EXPORT(OPTIMIZATION_GUIDE_FEATURES)
base::TimeDelta GetHostHintsFetchRefreshDuration();

// Returns the duration of the time window between fetches for hints for the
// URLs opened in active tabs.
COMPONENT_EXPORT(OPTIMIZATION_GUIDE_FEATURES)
base::TimeDelta GetActiveTabsFetchRefreshDuration();

// Returns the max duration since the time a tab has to be shown to be
// considered active for a hints refresh.
COMPONENT_EXPORT(OPTIMIZATION_GUIDE_FEATURES)
base::TimeDelta GetActiveTabsStalenessTolerance();

// Returns the max number of concurrent fetches to the remote Optimization Guide
// Service that should be allowed for batch updates
COMPONENT_EXPORT(OPTIMIZATION_GUIDE_FEATURES)
size_t MaxConcurrentBatchUpdateFetches();

// Returns the max number of concurrent fetches to the remote Optimization Guide
// Service that should be allowed for navigations.
COMPONENT_EXPORT(OPTIMIZATION_GUIDE_FEATURES)
size_t MaxConcurrentPageNavigationFetches();

// Returns the minimum random delay before starting to fetch for hints for
// active tabs.
COMPONENT_EXPORT(OPTIMIZATION_GUIDE_FEATURES)
base::TimeDelta ActiveTabsHintsFetchRandomMinDelay();

// Returns the maximum random delay before starting to fetch for hints for
// active tabs.
COMPONENT_EXPORT(OPTIMIZATION_GUIDE_FEATURES)
base::TimeDelta ActiveTabsHintsFetchRandomMaxDelay();

// Returns whether fetching hints for active tabs should happen on deferred
// startup. Otherwise active tabs hints will be fetched after a random interval
// between ActiveTabsHintsFetchRandomMinDelay() and
// ActiveTabsHintsFetchRandomMaxDelay().
COMPONENT_EXPORT(OPTIMIZATION_GUIDE_FEATURES)
bool ShouldDeferStartupActiveTabsHintsFetch();

// The amount of time host model features will be considered fresh enough
// to be used and remain in the OptimizationGuideStore.
COMPONENT_EXPORT(OPTIMIZATION_GUIDE_FEATURES)
base::TimeDelta StoredHostModelFeaturesFreshnessDuration();

// The maximum duration for which models can remain in the
// OptimizationGuideStore without being loaded.
COMPONENT_EXPORT(OPTIMIZATION_GUIDE_FEATURES)
base::TimeDelta StoredModelsValidDuration();

// The amount of time URL-keyed hints within the hint cache will be
// allowed to be used and not be purged.
COMPONENT_EXPORT(OPTIMIZATION_GUIDE_FEATURES)
base::TimeDelta URLKeyedHintValidCacheDuration();

// The amount of time the PCAService will wait for the title of a page to be
// modified.
COMPONENT_EXPORT(OPTIMIZATION_GUIDE_FEATURES)
base::TimeDelta PCAServiceWaitForTitleDelayDuration();

// The maximum number of hosts allowed to be requested by the client to the
// remote Optimization Guide Service for use by prediction models.
COMPONENT_EXPORT(OPTIMIZATION_GUIDE_FEATURES)
size_t MaxHostsForOptimizationGuideServiceModelsFetch();

// The maximum number of hosts allowed to be maintained in a least-recently-used
// cache by the prediction manager.
COMPONENT_EXPORT(OPTIMIZATION_GUIDE_FEATURES)
size_t MaxHostModelFeaturesCacheSize();

// The maximum number of hints allowed to be maintained in a least-recently-used
// cache for hosts.
COMPONENT_EXPORT(OPTIMIZATION_GUIDE_FEATURES)
size_t MaxHostKeyedHintCacheSize();

// The maximum number of hints allowed to be maintained in a least-recently-used
// cache for URLs.
COMPONENT_EXPORT(OPTIMIZATION_GUIDE_FEATURES)
size_t MaxURLKeyedHintCacheSize();

// Returns true if hints should be persisted to disk. If this is false, hints
// will just be stored in-memory and evicted if not recently used.
COMPONENT_EXPORT(OPTIMIZATION_GUIDE_FEATURES)
bool ShouldPersistHintsToDisk();

// Returns true if the optimization target decision for |optimization_target|
// should not be propagated to the caller in an effort to fully understand the
// statistics for the served model and not taint the resulting data.
COMPONENT_EXPORT(OPTIMIZATION_GUIDE_FEATURES)
bool ShouldOverrideOptimizationTargetDecisionForMetricsPurposes(
    proto::OptimizationTarget optimization_target);

// Returns whether personalized metadata should be enabled for
// |request_context|.
COMPONENT_EXPORT(OPTIMIZATION_GUIDE_FEATURES)
bool ShouldEnablePersonalizedMetadata(proto::RequestContext request_context);

// Returns the minimum random delay before starting to fetch for prediction
// models and host model features.
COMPONENT_EXPORT(OPTIMIZATION_GUIDE_FEATURES)
base::TimeDelta PredictionModelFetchRandomMinDelay();

// Returns the maximum random delay before starting to fetch for prediction
// models and host model features.
COMPONENT_EXPORT(OPTIMIZATION_GUIDE_FEATURES)
base::TimeDelta PredictionModelFetchRandomMaxDelay();

// Returns the time to wait before retrying a failed fetch for prediction
// models.
COMPONENT_EXPORT(OPTIMIZATION_GUIDE_FEATURES)
base::TimeDelta PredictionModelFetchRetryDelay();

// Returns the time to wait after browser start before fetching prediciton
// models.
COMPONENT_EXPORT(OPTIMIZATION_GUIDE_FEATURES)
base::TimeDelta PredictionModelFetchStartupDelay();

// Returns the time to wait after a successful fetch of prediction models to
// refresh models.
COMPONENT_EXPORT(OPTIMIZATION_GUIDE_FEATURES)
base::TimeDelta PredictionModelFetchInterval();

// Returns whether to enable fetching the model again when a new optimization
// target observer registration happens, after the initial model fetch is
// completed.
COMPONENT_EXPORT(OPTIMIZATION_GUIDE_FEATURES)
bool IsPredictionModelNewRegistrationFetchEnabled();

// Returns the time to wait for starting a model fetch when a new optimization
// target observer registration happens, after the initial model fetch is
// completed.
COMPONENT_EXPORT(OPTIMIZATION_GUIDE_FEATURES)
base::TimeDelta PredictionModelNewRegistrationFetchDelay();

// Whether to use the model execution watchdog.
COMPONENT_EXPORT(OPTIMIZATION_GUIDE_FEATURES)
bool IsModelExecutionWatchdogEnabled();

// The default timeout for the watchdog to use if none is given by the caller.
COMPONENT_EXPORT(OPTIMIZATION_GUIDE_FEATURES)
base::TimeDelta ModelExecutionWatchdogDefaultTimeout();

// Whether the ability to download models is enabled.
COMPONENT_EXPORT(OPTIMIZATION_GUIDE_FEATURES)
bool IsModelDownloadingEnabled();

// Returns whether unrestricted model downloading is enabled. If true, the
// client should download models using highest priority.
COMPONENT_EXPORT(OPTIMIZATION_GUIDE_FEATURES)
bool IsUnrestrictedModelDownloadingEnabled();

// Returns whether the feature to annotate page content is enabled.
COMPONENT_EXPORT(OPTIMIZATION_GUIDE_FEATURES)
bool IsPageContentAnnotationEnabled();

// Whether we should write content annotations to History Service.
COMPONENT_EXPORT(OPTIMIZATION_GUIDE_FEATURES)
bool ShouldWriteContentAnnotationsToHistoryService();

// Returns the max size of the MRU Cache of content that has been requested
// for annotation.
COMPONENT_EXPORT(OPTIMIZATION_GUIDE_FEATURES)
size_t MaxContentAnnotationRequestsCached();

// Returns whether or not related searches should be extracted from Google SRP
// as part of page content annotations.
COMPONENT_EXPORT(OPTIMIZATION_GUIDE_FEATURES)
bool ShouldExtractRelatedSearches();

// Returns whether the page entities model should be executed on page content
// for a user using |locale| as their browser language.
COMPONENT_EXPORT(OPTIMIZATION_GUIDE_FEATURES)
bool ShouldExecutePageEntitiesModelOnPageContent(const std::string& locale);

// Returns whether the page visibility model should be executed on page content
// for a user using |locale| as their browser language.
COMPONENT_EXPORT(OPTIMIZATION_GUIDE_FEATURES)
bool ShouldExecutePageVisibilityModelOnPageContent(const std::string& locale);

// Returns whether the text embedding model should be executed on page content
// for a user using |locale| as their browser language.
COMPONENT_EXPORT(OPTIMIZATION_GUIDE_FEATURES)
bool ShouldExecuteTextEmbeddingModelOnPageContent(const std::string& locale);

// Returns whether page metadata should be retrieved from the remote
// Optimization Guide service.
COMPONENT_EXPORT(OPTIMIZATION_GUIDE_FEATURES)
bool RemotePageMetadataEnabled(const std::string& locale,
                               const std::string& country_code);

// Returns the minimum score associated with a category for it to be persisted.
// Will be a value from 0 to 100, inclusive.
COMPONENT_EXPORT(OPTIMIZATION_GUIDE_FEATURES)
int GetMinimumPageCategoryScoreToPersist();

// The time to wait beyond the onload event before sending the hints request for
// link predictions.
COMPONENT_EXPORT(OPTIMIZATION_GUIDE_FEATURES)
base::TimeDelta GetOnloadDelayForHintsFetching();

// The number of bits used for RAPPOR-style metrics reporting on content
// annotation models. Must be at least 1 bit.
COMPONENT_EXPORT(OPTIMIZATION_GUIDE_FEATURES)
int NumBitsForRAPPORMetrics();

// The probability of a bit flip a score with RAPPOR-style metrics reporting.
// Must be between 0 and 1.
COMPONENT_EXPORT(OPTIMIZATION_GUIDE_FEATURES)
double NoiseProbabilityForRAPPORMetrics();

// Returns whether the metadata validation fetch feature is host keyed.
COMPONENT_EXPORT(OPTIMIZATION_GUIDE_FEATURES)
bool ShouldMetadataValidationFetchHostKeyed();

// Returns if Page Visibility Batch Annotations are enabled.
COMPONENT_EXPORT(OPTIMIZATION_GUIDE_FEATURES)
bool PageVisibilityBatchAnnotationsEnabled();

// Returns if Text Embedding Batch Annotations are enabled.
COMPONENT_EXPORT(OPTIMIZATION_GUIDE_FEATURES)
bool TextEmbeddingBatchAnnotationsEnabled();

// The number of visits batch before running the page content annotation
// models. A size of 1 is equivalent to annotating one page load at time
// immediately after requested.
COMPONENT_EXPORT(OPTIMIZATION_GUIDE_FEATURES)
size_t AnnotateVisitBatchSize();

// Whether the page content annotation validation feature or command line flag
// is enabled for the given annotation type.
COMPONENT_EXPORT(OPTIMIZATION_GUIDE_FEATURES)
bool PageContentAnnotationValidationEnabledForType(AnnotationType type);

// The time period between browser start and running a running page content
// annotation validation.
COMPONENT_EXPORT(OPTIMIZATION_GUIDE_FEATURES)
base::TimeDelta PageContentAnnotationValidationStartupDelay();

// The size of batches to run for page content validation.
COMPONENT_EXPORT(OPTIMIZATION_GUIDE_FEATURES)
size_t PageContentAnnotationsValidationBatchSize();

// The maximum size of the visit annotation cache.
COMPONENT_EXPORT(OPTIMIZATION_GUIDE_FEATURES)
size_t MaxVisitAnnotationCacheSize();

// Returns the number of threads to use for model inference on the given
// optimization target.
COMPONENT_EXPORT(OPTIMIZATION_GUIDE_FEATURES)
absl::optional<int> OverrideNumThreadsForOptTarget(
    proto::OptimizationTarget opt_target);

// Whether XNNPACK should be used with TFLite, on platforms where it is
// supported. This is a no-op on unsupported platforms.
COMPONENT_EXPORT(OPTIMIZATION_GUIDE_FEATURES)
bool TFLiteXNNPACKDelegateEnabled();

// Whether to check the pref for whether a previous component version failed.
COMPONENT_EXPORT(OPTIMIZATION_GUIDE_FEATURES)
bool ShouldCheckFailedComponentVersionPref();

// Returns whether the feature for new model store that is tied with Chrome
// installation and shares the models across user profiles, is enabled.
COMPONENT_EXPORT(OPTIMIZATION_GUIDE_FEATURES)
bool IsInstallWideModelStoreEnabled();

// Whether to persist salient image metadata for each visit.
COMPONENT_EXPORT(OPTIMIZATION_GUIDE_FEATURES)
bool ShouldPersistSalientImageMetadata(const std::string& locale,
                                       const std::string& country_code);

// Returns whether to query text embeddings coming from history service.
COMPONENT_EXPORT(OPTIMIZATION_GUIDE_FEATURES)
bool ShouldQueryEmbeddings();

// Whether logging of model quality is enabled.
COMPONENT_EXPORT(OPTIMIZATION_GUIDE_FEATURES)
bool IsModelQualityLoggingEnabled();

// Whether model quality logging is enabled for a feature.
COMPONENT_EXPORT(OPTIMIZATION_GUIDE_FEATURES)
bool IsModelQualityLoggingEnabledForFeature(
    proto::ModelExecutionFeature feature);

// Returns whether the `model_version` for `opt_target` is part of emergency
// killswitch, and this model should be stopped serving immediately.
COMPONENT_EXPORT(OPTIMIZATION_GUIDE_FEATURES)
std::map<proto::OptimizationTarget, std::set<int64_t>>
GetPredictionModelVersionsInKillSwitch();

// Returns the idle timeout before the on device model service shuts down.
COMPONENT_EXPORT(OPTIMIZATION_GUIDE_FEATURES)
base::TimeDelta GetOnDeviceModelIdleTimeout();

// These params determine how context processing works for the on device model.
// The model will process at least min tokens before responding. While waiting
// for the ExecuteModel() call, up to max tokens will be processed in chunks of
// the given size.
COMPONENT_EXPORT(OPTIMIZATION_GUIDE_FEATURES)
int GetOnDeviceModelMinTokensForContext();
COMPONENT_EXPORT(OPTIMIZATION_GUIDE_FEATURES)
int GetOnDeviceModelMaxTokensForContext();
COMPONENT_EXPORT(OPTIMIZATION_GUIDE_FEATURES)
int GetOnDeviceModelContextTokenChunkSize();

// The maximum tokens for the input when executing the model.
COMPONENT_EXPORT(OPTIMIZATION_GUIDE_FEATURES)
int GetOnDeviceModelMaxTokensForExecute();

// The maximum tokens the model will output if the maximum input is given.
COMPONENT_EXPORT(OPTIMIZATION_GUIDE_FEATURES)
int GetOnDeviceModelMaxTokensForOutput();

// Returns the number of crashes without a successful response before the
// on-device model won't be used.
COMPONENT_EXPORT(OPTIMIZATION_GUIDE_FEATURES)
int GetOnDeviceModelCrashCountBeforeDisable();

// Returns the number of sessions that timed out before the on-device model
// won't be used.
COMPONENT_EXPORT(OPTIMIZATION_GUIDE_FEATURES)
int GetOnDeviceModelTimeoutCountBeforeDisable();

COMPONENT_EXPORT(OPTIMIZATION_GUIDE_FEATURES)
base::TimeDelta GetOnDeviceStartupMetricDelay();

// Returns the amount of time before the initial response needs to be received
// from the on-device model before falling back to the server.
COMPONENT_EXPORT(OPTIMIZATION_GUIDE_FEATURES)
base::TimeDelta GetOnDeviceModelTimeForInitialResponse();

// Returns true if during execution a disconnect is received (which generally
// means a crash) the message should be sent to the server for processing.
COMPONENT_EXPORT(OPTIMIZATION_GUIDE_FEATURES)
bool GetOnDeviceFallbackToServerOnDisconnect();

// Whether any features are enabled that allow launching the on-device service.
COMPONENT_EXPORT(OPTIMIZATION_GUIDE_FEATURES)
bool CanLaunchOnDeviceModelService();

}  // namespace features
}  // namespace optimization_guide

#endif  // COMPONENTS_OPTIMIZATION_GUIDE_CORE_OPTIMIZATION_GUIDE_FEATURES_H_
