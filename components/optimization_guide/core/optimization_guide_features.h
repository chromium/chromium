// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OPTIMIZATION_GUIDE_CORE_OPTIMIZATION_GUIDE_FEATURES_H_
#define COMPONENTS_OPTIMIZATION_GUIDE_CORE_OPTIMIZATION_GUIDE_FEATURES_H_

#include <map>
#include <optional>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "base/component_export.h"
#include "base/containers/enum_set.h"
#include "base/containers/flat_set.h"
#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "components/optimization_guide/core/model_execution/feature_keys.h"
#include "components/optimization_guide/core/optimization_guide_enums.h"
#include "components/optimization_guide/proto/hints.pb.h"
#include "components/optimization_guide/proto/model_execution.pb.h"
#include "components/optimization_guide/proto/models.pb.h"
#include "net/nqe/effective_connection_type.h"
#include "url/gurl.h"

namespace optimization_guide {

class MqlsFeatureMetadata;
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
BASE_DECLARE_FEATURE(kOptimizationTargetPrediction);
COMPONENT_EXPORT(OPTIMIZATION_GUIDE_FEATURES)
BASE_DECLARE_FEATURE(kOptimizationGuideModelDownloading);
COMPONENT_EXPORT(OPTIMIZATION_GUIDE_FEATURES)
BASE_DECLARE_FEATURE(kPageTextExtraction);
COMPONENT_EXPORT(OPTIMIZATION_GUIDE_FEATURES)
BASE_DECLARE_FEATURE(kPushNotifications);
COMPONENT_EXPORT(OPTIMIZATION_GUIDE_FEATURES)
BASE_DECLARE_FEATURE(kOptimizationGuideMetadataValidation);
COMPONENT_EXPORT(OPTIMIZATION_GUIDE_FEATURES)
BASE_DECLARE_FEATURE(kPreventLongRunningPredictionModels);
COMPONENT_EXPORT(OPTIMIZATION_GUIDE_FEATURES)
BASE_DECLARE_FEATURE(kOverrideNumThreadsForModelExecution);
COMPONENT_EXPORT(OPTIMIZATION_GUIDE_FEATURES)
BASE_DECLARE_FEATURE(kOptGuideEnableXNNPACKDelegateWithTFLite);
COMPONENT_EXPORT(OPTIMIZATION_GUIDE_FEATURES)
BASE_DECLARE_FEATURE(kOptimizationHintsComponent);
COMPONENT_EXPORT(OPTIMIZATION_GUIDE_FEATURES)
BASE_DECLARE_FEATURE(kOptimizationGuidePersonalizedFetching);
COMPONENT_EXPORT(OPTIMIZATION_GUIDE_FEATURES)
BASE_DECLARE_FEATURE(kOptimizationGuidePredictionModelKillswitch);
COMPONENT_EXPORT(OPTIMIZATION_GUIDE_FEATURES)
BASE_DECLARE_FEATURE(kOptimizationGuideModelExecution);
COMPONENT_EXPORT(OPTIMIZATION_GUIDE_FEATURES)
BASE_DECLARE_FEATURE(kOptimizationGuideOnDeviceModel);
COMPONENT_EXPORT(OPTIMIZATION_GUIDE_FEATURES)
BASE_DECLARE_FEATURE(kOptimizationGuideComposeOnDeviceEval);
COMPONENT_EXPORT(OPTIMIZATION_GUIDE_FEATURES)
BASE_DECLARE_FEATURE(kModelQualityLogging);
COMPONENT_EXPORT(OPTIMIZATION_GUIDE_FEATURES)
BASE_DECLARE_FEATURE(kLogOnDeviceMetricsOnStartup);
COMPONENT_EXPORT(OPTIMIZATION_GUIDE_FEATURES)
BASE_DECLARE_FEATURE(kTextSafetyClassifier);
COMPONENT_EXPORT(OPTIMIZATION_GUIDE_FEATURES)
BASE_DECLARE_FEATURE(kTextSafetyRemoteFallback);
COMPONENT_EXPORT(OPTIMIZATION_GUIDE_FEATURES)
BASE_DECLARE_FEATURE(kOnDeviceModelValidation);

#if !BUILDFLAG(IS_ANDROID)
COMPONENT_EXPORT(OPTIMIZATION_GUIDE_FEATURES)
BASE_DECLARE_FEATURE(kAiSettingsPageRefresh);
#endif

typedef base::EnumSet<proto::RequestContext,
                      proto::RequestContext_MIN,
                      proto::RequestContext_MAX>
    RequestContextSet;

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

// Returns requests contexts for which personalized metadata should be enabled.
COMPONENT_EXPORT(OPTIMIZATION_GUIDE_FEATURES)
RequestContextSet GetAllowedContextsForPersonalizedMetadata();

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

// Returns the random delay time to wait for starting a model fetch when a new
// optimization target observer registration happens, after the initial model
// fetch is completed.
COMPONENT_EXPORT(OPTIMIZATION_GUIDE_FEATURES)
base::TimeDelta PredictionModelNewRegistrationFetchRandomDelay();

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

// Returns whether the page entities model should be executed on page content
// for a user using |locale| as their browser language.
COMPONENT_EXPORT(OPTIMIZATION_GUIDE_FEATURES)
bool ShouldExecutePageEntitiesModelOnPageContent(const std::string& locale);

// The time to wait beyond the onload event before sending the hints request for
// link predictions.
COMPONENT_EXPORT(OPTIMIZATION_GUIDE_FEATURES)
base::TimeDelta GetOnloadDelayForHintsFetching();

// Returns whether the metadata validation fetch feature is host keyed.
COMPONENT_EXPORT(OPTIMIZATION_GUIDE_FEATURES)
bool ShouldMetadataValidationFetchHostKeyed();

// Returns the number of threads to use for model inference on the given
// optimization target.
COMPONENT_EXPORT(OPTIMIZATION_GUIDE_FEATURES)
std::optional<int> OverrideNumThreadsForOptTarget(
    proto::OptimizationTarget opt_target);

// Whether XNNPACK should be used with TFLite, on platforms where it is
// supported. This is a no-op on unsupported platforms.
COMPONENT_EXPORT(OPTIMIZATION_GUIDE_FEATURES)
bool TFLiteXNNPACKDelegateEnabled();

// Whether to check the pref for whether a previous component version failed.
COMPONENT_EXPORT(OPTIMIZATION_GUIDE_FEATURES)
bool ShouldCheckFailedComponentVersionPref();

// Whether logging of model quality is enabled.
COMPONENT_EXPORT(OPTIMIZATION_GUIDE_FEATURES)
bool IsModelQualityLoggingEnabled();

// Whether model quality logging is enabled for a feature.
COMPONENT_EXPORT(OPTIMIZATION_GUIDE_FEATURES)
bool IsModelQualityLoggingEnabledForFeature(const MqlsFeatureMetadata*);

// Returns whether the `model_version` for `opt_target` is part of emergency
// killswitch, and this model should be stopped serving immediately.
COMPONENT_EXPORT(OPTIMIZATION_GUIDE_FEATURES)
std::map<proto::OptimizationTarget, std::set<int64_t>>
GetPredictionModelVersionsInKillSwitch();

// Returns whether the on-device config should be loaded with higher priority.
// If true, all tasks for the on-device model execution config interpreter
// will be run with user visible priority.
COMPONENT_EXPORT(OPTIMIZATION_GUIDE_FEATURES)
bool ShouldLoadOnDeviceModelExecutionConfigWithHigherPriority();

// Returns the idle timeout before the on device model service shuts down.
COMPONENT_EXPORT(OPTIMIZATION_GUIDE_FEATURES)
base::TimeDelta GetOnDeviceModelIdleTimeout();

// Returns the delay before starting the on device model inference when
// running validation.
COMPONENT_EXPORT(OPTIMIZATION_GUIDE_FEATURES)
base::TimeDelta GetOnDeviceModelExecutionValidationStartupDelay();

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

// Returns whether the performance class is compatible with executing the
// on-device model. Used to determine whether or not to fetch the on-device
// model.
COMPONENT_EXPORT(OPTIMIZATION_GUIDE_FEATURES)
bool IsPerformanceClassCompatibleWithOnDeviceModel(
    OnDeviceModelPerformanceClass performance_class);

// Whether any features are enabled that allow launching the on-device
// service.
COMPONENT_EXPORT(OPTIMIZATION_GUIDE_FEATURES)
bool CanLaunchOnDeviceModelService();

// Whether on-device execution is enabled.
COMPONENT_EXPORT(OPTIMIZATION_GUIDE_FEATURES)
bool IsOnDeviceExecutionEnabled();

// The amount of grace period to use from the last time the feature was used to
// consider it as recently used. Recent usage is one of the criteria for the
// base and adaptation on-device models to be downloaded.
COMPONENT_EXPORT(OPTIMIZATION_GUIDE_FEATURES)
base::TimeDelta GetOnDeviceEligibleModelFeatureRecentUsePeriod();

// The on-device model is fetched when the device is considered eligible for
// on-device execution. When the device stops being eligible, the model is
// retained for this amount of time. This protects the user from repeatedly
// downloading the model in the event eligibility fluctuates. for on-device
// evaluation
// See on_device_model_component.cc for how eligibility is computed.
COMPONENT_EXPORT(OPTIMIZATION_GUIDE_FEATURES)
base::TimeDelta GetOnDeviceModelRetentionTime();

// Whether there is enough free disk space to allow on-device model
// installation.
COMPONENT_EXPORT(OPTIMIZATION_GUIDE_FEATURES)
bool IsFreeDiskSpaceSufficientForOnDeviceModelInstall(
    int64_t free_disk_space_bytes);

// Whether there is too little disk space to retain the on-device model
// installation.
COMPONENT_EXPORT(OPTIMIZATION_GUIDE_FEATURES)
bool IsFreeDiskSpaceTooLowForOnDeviceModelInstall(
    int64_t free_disk_space_bytes);

// Returns true if unsafe content should be removed.
COMPONENT_EXPORT(OPTIMIZATION_GUIDE_FEATURES)
bool GetOnDeviceModelRetractUnsafeContent();

// Whether we should initiate download of the text safety classifier model.
COMPONENT_EXPORT(OPTIMIZATION_GUIDE_FEATURES)
bool ShouldUseTextSafetyClassifierModel();

// Number of tokens between each text safety update.
COMPONENT_EXPORT(OPTIMIZATION_GUIDE_FEATURES)
uint32_t GetOnDeviceModelTextSafetyTokenInterval();

// This is the minimum required reliability threshold for language detection to
// be considered reliable enough for the text safety classifier. Clamped to the
// range [0, 1].
COMPONENT_EXPORT(OPTIMIZATION_GUIDE_FEATURES)
double GetOnDeviceModelLanguageDetectionMinimumReliability();

// Whether to use text safety remote fallback for on-device text safety
// evaluation.
COMPONENT_EXPORT(OPTIMIZATION_GUIDE_FEATURES)
bool ShouldUseTextSafetyRemoteFallbackForEligibleFeatures();

// These params configure the repetition checker. See HasRepeatingSuffix() in
// repetition_checker.h for explanation. A value of 2 for num repeats and 16 for
// min repeat chars would mean we will halt a response once it repeats at least
// 16 chars 2 times at the end of the response.
COMPONENT_EXPORT(OPTIMIZATION_GUIDE_FEATURES)
int GetOnDeviceModelNumRepeats();
COMPONENT_EXPORT(OPTIMIZATION_GUIDE_FEATURES)
int GetOnDeviceModelMinRepeatChars();

// Whether the response should be retracted if repeats are detected.
COMPONENT_EXPORT(OPTIMIZATION_GUIDE_FEATURES)
bool GetOnDeviceModelRetractRepeats();

// Settings to control output sampling.
COMPONENT_EXPORT(OPTIMIZATION_GUIDE_FEATURES)
int GetOnDeviceModelDefaultTopK();
COMPONENT_EXPORT(OPTIMIZATION_GUIDE_FEATURES)
int GetOnDeviceModelMaxTopK();
COMPONENT_EXPORT(OPTIMIZATION_GUIDE_FEATURES)
double GetOnDeviceModelDefaultTemperature();

COMPONENT_EXPORT(OPTIMIZATION_GUIDE_FEATURES)
std::vector<uint32_t> GetOnDeviceModelAllowedAdaptationRanks();

// Whether the on-device model will be validated when updated using a set of
// prompts with expected output.
COMPONENT_EXPORT(OPTIMIZATION_GUIDE_FEATURES)
bool IsOnDeviceModelValidationEnabled();

// Whether on-device sessions should be blocked on validation failures.
COMPONENT_EXPORT(OPTIMIZATION_GUIDE_FEATURES)
bool ShouldOnDeviceModelBlockOnValidationFailure();

// Whether the validation result for a model should be cleared if Chrome's
// version changes.
COMPONENT_EXPORT(OPTIMIZATION_GUIDE_FEATURES)
bool ShouldOnDeviceModelClearValidationOnVersionChange();

// The delay from when a new model is received (or startup if validation has not
// completed) until the validation is run.
COMPONENT_EXPORT(OPTIMIZATION_GUIDE_FEATURES)
base::TimeDelta GetOnDeviceModelValidationDelay();

// The maximum number of attempts model validation will be retried.
COMPONENT_EXPORT(OPTIMIZATION_GUIDE_FEATURES)
int GetOnDeviceModelValidationAttemptCount();

}  // namespace features
}  // namespace optimization_guide

#endif  // COMPONENTS_OPTIMIZATION_GUIDE_CORE_OPTIMIZATION_GUIDE_FEATURES_H_
