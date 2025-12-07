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

#include "base/byte_count.h"
#include "base/component_export.h"
#include "base/containers/enum_set.h"
#include "base/containers/flat_set.h"
#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"
#include "base/time/time.h"
#include "build/build_config.h"
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
BASE_DECLARE_FEATURE(kOptimizationGuideFetchingForSRP);
COMPONENT_EXPORT(OPTIMIZATION_GUIDE_FEATURES)
BASE_DECLARE_FEATURE(kOptimizationTargetPrediction);
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
BASE_DECLARE_FEATURE(kOptimizationGuidePersonalizedFetching);
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
COMPONENT_EXPORT(OPTIMIZATION_GUIDE_FEATURES)
BASE_DECLARE_FEATURE(kTextSafetyClassifier);
COMPONENT_EXPORT(OPTIMIZATION_GUIDE_FEATURES)
BASE_DECLARE_FEATURE(kTextSafetyScanLanguageDetection);
COMPONENT_EXPORT(OPTIMIZATION_GUIDE_FEATURES)
BASE_DECLARE_FEATURE(kOnDeviceModelFetchPerformanceClassEveryStartup);
COMPONENT_EXPORT(OPTIMIZATION_GUIDE_FEATURES)
BASE_DECLARE_FEATURE(kAiSettingsPageForceAvailable);
COMPONENT_EXPORT(OPTIMIZATION_GUIDE_FEATURES)
BASE_DECLARE_FEATURE(kAnnotatedPageContentWithActionableElements);
COMPONENT_EXPORT(OPTIMIZATION_GUIDE_FEATURES)
BASE_DECLARE_FEATURE(kAnnotatedPageContentWithMediaData);
COMPONENT_EXPORT(OPTIMIZATION_GUIDE_FEATURES)
BASE_DECLARE_FEATURE(kOptimizationGuideProactivePersonalizedHintsFetching);
COMPONENT_EXPORT(OPTIMIZATION_GUIDE_FEATURES)
BASE_DECLARE_FEATURE(kOptimizationGuideBypassFormsClassificationAuth);

// Allows setting feature params for model download configuration, such as
// minimum performance class for download.
COMPONENT_EXPORT(OPTIMIZATION_GUIDE_FEATURES)
BASE_DECLARE_FEATURE(kOnDeviceModelPerformanceParams);

// Comma-separated list of performance classes (e.g. "3,4,5") that should
// download the base model. Use "*" if there is no performance class
// requirement.
COMPONENT_EXPORT(OPTIMIZATION_GUIDE_FEATURES)
extern const base::FeatureParam<std::string>
    kPerformanceClassListForOnDeviceModel;

// Comma-separated list of performance classes that should use a smaller model
// if available. This should be a subset of
// kPerformanceClassListForOnDeviceModel.
COMPONENT_EXPORT(OPTIMIZATION_GUIDE_FEATURES)
extern const base::FeatureParam<std::string>
    kLowTierPerformanceClassListForOnDeviceModel;

// Comma-separated list of performance classes that have image input enabled.
COMPONENT_EXPORT(OPTIMIZATION_GUIDE_FEATURES)
extern const base::FeatureParam<std::string> kPerformanceClassListForImageInput;

// Comma-separated list of performance classes that have audio input enabled.
COMPONENT_EXPORT(OPTIMIZATION_GUIDE_FEATURES)
extern const base::FeatureParam<std::string> kPerformanceClassListForAudioInput;

COMPONENT_EXPORT(OPTIMIZATION_GUIDE_FEATURES)
BASE_DECLARE_FEATURE(kOptimizationGuideIconView);

// Whether model sessions may be brokered to untrusted processes.
COMPONENT_EXPORT(OPTIMIZATION_GUIDE_FEATURES)
BASE_DECLARE_FEATURE(kBrokerModelSessionsForUntrustedProcesses);

COMPONENT_EXPORT(OPTIMIZATION_GUIDE_FEATURES)
BASE_DECLARE_FEATURE(kGetAIPageContentSubframeTimeoutEnabled);
COMPONENT_EXPORT(OPTIMIZATION_GUIDE_FEATURES)
extern const base::FeatureParam<base::TimeDelta>
    kGetAIPageContentSubframeTimeoutParam;

typedef base::EnumSet<proto::RequestContext,
                      proto::RequestContext_MIN,
                      proto::RequestContext_MAX>
    RequestContextSet;

typedef base::EnumSet<proto::OptimizationType,
                      proto::OptimizationType_MIN,
                      proto::OptimizationType_MAX>
    OptimizationTypeSet;

// The grace period duration for how long to give outstanding page text dump
// requests to respond after DidFinishLoad.
COMPONENT_EXPORT(OPTIMIZATION_GUIDE_FEATURES)
base::TimeDelta PageTextExtractionOutstandingRequestsGracePeriod();

// Whether hints for active tabs and top hosts should be batch updated.
COMPONENT_EXPORT(OPTIMIZATION_GUIDE_FEATURES)
bool ShouldBatchUpdateHintsForActiveTabsAndTopHosts();

// Whether hints fetching for search results is enabled.
COMPONENT_EXPORT(OPTIMIZATION_GUIDE_FEATURES)
bool IsSRPFetchingEnabled();
// The maximum number of search results allowed to be requested by the client to
// the remote Optimization Guide Service.
COMPONENT_EXPORT(OPTIMIZATION_GUIDE_FEATURES)
size_t MaxResultsForSRPFetch();

// The API key for the One Platform Optimization Guide Service.
COMPONENT_EXPORT(OPTIMIZATION_GUIDE_FEATURES)
std::string GetOptimizationGuideServiceAPIKey();

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

// Returns true if the feature to use push notifications is enabled.
COMPONENT_EXPORT(OPTIMIZATION_GUIDE_FEATURES)
bool IsPushNotificationsEnabled();

// Returns whether fetching hints for active tabs should happen on deferred
// startup. Otherwise active tabs hints will be fetched after a random interval
// between ActiveTabsHintsFetchRandomMinDelay() and
// ActiveTabsHintsFetchRandomMaxDelay().
COMPONENT_EXPORT(OPTIMIZATION_GUIDE_FEATURES)
bool ShouldDeferStartupActiveTabsHintsFetch();

// The maximum number of hints allowed to be maintained in a least-recently-used
// cache for hosts.
COMPONENT_EXPORT(OPTIMIZATION_GUIDE_FEATURES)
size_t MaxHostKeyedHintCacheSize();

// Returns true if hints should be persisted to disk. If this is false, hints
// will just be stored in-memory and evicted if not recently used.
COMPONENT_EXPORT(OPTIMIZATION_GUIDE_FEATURES)
bool ShouldPersistHintsToDisk();

// Returns requests contexts for which personalized metadata should be enabled.
COMPONENT_EXPORT(OPTIMIZATION_GUIDE_FEATURES)
RequestContextSet GetAllowedContextsForPersonalizedMetadata();

// Returns optimization types for which proactive personalization should be
// enabled.
COMPONENT_EXPORT(OPTIMIZATION_GUIDE_FEATURES)
OptimizationTypeSet GetAllowedOptimizationTypesForProactivePersonalization();

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

// Returns whether the page entities model should be executed on page content
// for a user using |locale| as their browser language.
COMPONENT_EXPORT(OPTIMIZATION_GUIDE_FEATURES)
bool ShouldExecutePageEntitiesModelOnPageContent(const std::string& locale);

// Returns whether the metadata validation fetch feature is host keyed.
COMPONENT_EXPORT(OPTIMIZATION_GUIDE_FEATURES)
bool ShouldMetadataValidationFetchHostKeyed();

// Returns the number of threads to use for model inference on the given
// optimization target.
COMPONENT_EXPORT(OPTIMIZATION_GUIDE_FEATURES)
std::optional<int> OverrideNumThreadsForOptTarget(
    proto::OptimizationTarget opt_target);

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

// Returns the number of crashes without a successful response before the
// on-device model won't be used.
COMPONENT_EXPORT(OPTIMIZATION_GUIDE_FEATURES)
int GetOnDeviceModelCrashCountBeforeDisable();

// Feature params for handling exponential backoff after crashes.
COMPONENT_EXPORT(OPTIMIZATION_GUIDE_FEATURES)
base::TimeDelta GetOnDeviceModelMaxCrashBackoffTime();
COMPONENT_EXPORT(OPTIMIZATION_GUIDE_FEATURES)
base::TimeDelta GetOnDeviceModelCrashBackoffBaseTime();

COMPONENT_EXPORT(OPTIMIZATION_GUIDE_FEATURES)
base::TimeDelta GetOnDeviceStartupMetricDelay();

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

// Return the disk space required for on device model install.
COMPONENT_EXPORT(OPTIMIZATION_GUIDE_FEATURES)
base::ByteCount GetDiskSpaceRequiredForOnDeviceModelInstall();

// Whether there is enough free disk space to allow on-device model
// installation.
COMPONENT_EXPORT(OPTIMIZATION_GUIDE_FEATURES)
bool IsFreeDiskSpaceSufficientForOnDeviceModelInstall(
    base::ByteCount free_disk_space_bytes);

// Whether there is too little disk space to retain the on-device model
// installation.
COMPONENT_EXPORT(OPTIMIZATION_GUIDE_FEATURES)
bool IsFreeDiskSpaceTooLowForOnDeviceModelInstall(
    base::ByteCount free_disk_space_bytes);

// Returns true if unsafe content should be removed.
COMPONENT_EXPORT(OPTIMIZATION_GUIDE_FEATURES)
bool GetOnDeviceModelRetractUnsafeContent();

// Whether we should initiate download of the text safety classifier model.
COMPONENT_EXPORT(OPTIMIZATION_GUIDE_FEATURES)
bool ShouldUseTextSafetyClassifierModel();

// This is the minimum required reliability threshold for language detection to
// be considered reliable enough for the text safety classifier. Clamped to the
// range [0, 1].
COMPONENT_EXPORT(OPTIMIZATION_GUIDE_FEATURES)
double GetOnDeviceModelLanguageDetectionMinimumReliability();

// Whether the newer generalized safety model is used instead of the ULM-based
// model as the text safety model. Irrelevant if
// `ShouldUseTextSafetyClassifierModel()` returns false;
COMPONENT_EXPORT(OPTIMIZATION_GUIDE_FEATURES)
bool ShouldUseGeneralizedSafetyModel();

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

// Returns whether the icon view should be enabled.
COMPONENT_EXPORT(OPTIMIZATION_GUIDE_FEATURES)
bool ShouldEnableOptimizationGuideIconView();

// Returns what the timeout for calls to GetAIPageContent should be for
// subframes. An empty return value indicates no timeout should be applied.
COMPONENT_EXPORT(OPTIMIZATION_GUIDE_FEATURES)
std::optional<base::TimeDelta> GetSubframeGetAIPageContentTimeout();

}  // namespace features
}  // namespace optimization_guide

#endif  // COMPONENTS_OPTIMIZATION_GUIDE_CORE_OPTIMIZATION_GUIDE_FEATURES_H_
