// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/optimization_guide/core/optimization_guide_features.h"

#include <cstring>

#include "base/command_line.h"
#include "base/containers/contains.h"
#include "base/containers/enum_set.h"
#include "base/feature_list.h"
#include "base/logging.h"
#include "base/metrics/field_trial.h"
#include "base/metrics/field_trial_params.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/to_string.h"
#include "base/system/sys_info.h"
#include "build/build_config.h"
#include "components/optimization_guide/core/insertion_ordered_set.h"
#include "components/optimization_guide/core/optimization_guide_constants.h"
#include "components/optimization_guide/core/optimization_guide_enums.h"
#include "components/optimization_guide/core/optimization_guide_switches.h"
#include "components/optimization_guide/machine_learning_tflite_buildflags.h"
#include "components/optimization_guide/proto/common_types.pb.h"
#include "components/variations/hashing.h"
#include "google_apis/gaia/gaia_constants.h"
#include "google_apis/google_api_keys.h"
#include "net/base/url_util.h"
#include "ui/base/l10n/l10n_util.h"

namespace optimization_guide {
namespace features {

namespace {

constexpr auto enabled_by_default_mobile_only =
#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_IOS)
    base::FEATURE_ENABLED_BY_DEFAULT;
#else
    base::FEATURE_DISABLED_BY_DEFAULT;
#endif

}  // namespace

// Enables the syncing of the Optimization Hints component, which provides
// hints for what optimizations can be applied on a page load.
BASE_FEATURE(kOptimizationHints,
             "OptimizationHints",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Enables fetching from a remote Optimization Guide Service.
BASE_FEATURE(kRemoteOptimizationGuideFetching,
             "OptimizationHintsFetching",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kRemoteOptimizationGuideFetchingAnonymousDataConsent,
             "OptimizationHintsFetchingAnonymousDataConsent",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Enables performance info in the context menu and fetching from a remote
// Optimization Guide Service.
BASE_FEATURE(kContextMenuPerformanceInfoAndRemoteHintFetching,
             "ContextMenuPerformanceInfoAndRemoteHintFetching",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enables the prediction of optimization targets.
BASE_FEATURE(kOptimizationTargetPrediction,
             "OptimizationTargetPrediction",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Enables the downloading of models.
BASE_FEATURE(kOptimizationGuideModelDownloading,
             "OptimizationGuideModelDownloading",
#if BUILDFLAG(BUILD_WITH_TFLITE_LIB)
             base::FEATURE_ENABLED_BY_DEFAULT
#else   // BUILD_WITH_TFLITE_LIB
             base::FEATURE_DISABLED_BY_DEFAULT
#endif  // !BUILD_WITH_TFLITE_LIB
);

// Enables push notification of hints.
BASE_FEATURE(kPushNotifications,
             "OptimizationGuidePushNotifications",
             enabled_by_default_mobile_only);

// This feature flag does not turn off any behavior, it is only used for
// experiment parameters.
BASE_FEATURE(kPageTextExtraction,
             "OptimizationGuidePageContentExtraction",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Enables the validation of optimization guide metadata.
BASE_FEATURE(kOptimizationGuideMetadataValidation,
             "OptimizationGuideMetadataValidation",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kPreventLongRunningPredictionModels,
             "PreventLongRunningPredictionModels",
             base::FEATURE_ENABLED_BY_DEFAULT);


BASE_FEATURE(kOverrideNumThreadsForModelExecution,
             "OverrideNumThreadsForModelExecution",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kOptGuideEnableXNNPACKDelegateWithTFLite,
             "OptGuideEnableXNNPACKDelegateWithTFLite",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kOptimizationHintsComponent,
             "OptimizationHintsComponent",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Killswitch for fetching on search results from a remote Optimization Guide
// Service.
BASE_FEATURE(kOptimizationGuideFetchingForSRP,
             "OptimizationHintsFetchingSRP",
             base::FEATURE_ENABLED_BY_DEFAULT);


// Kill switch for disabling model quality logging.
BASE_FEATURE(kModelQualityLogging,
             "ModelQualityLogging",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Enables fetching personalized metadata from Optimization Guide Service.
BASE_FEATURE(kOptimizationGuidePersonalizedFetching,
             "OptimizationPersonalizedHintsFetching",
             base::FEATURE_ENABLED_BY_DEFAULT);

// An emergency kill switch feature to stop serving certain model versions per
// optimization target. This is useful in exceptional situations when a bad
// model version got served that lead to crashes or critical failures, and an
// immediate remedy is needed to stop serving those versions.
BASE_FEATURE(kOptimizationGuidePredictionModelKillswitch,
             "OptimizationGuidePredictionModelKillswitch",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Whether to enable model execution.
BASE_FEATURE(kOptimizationGuideModelExecution,
             "OptimizationGuideModelExecution",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Whether to use the on device model service in optimization guide.
BASE_FEATURE(kOptimizationGuideOnDeviceModel,
             "OptimizationGuideOnDeviceModel",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Whether to allow on device model evaluation for Compose. This has no effect
// if OptimizationGuideOnDeviceModel is off.
BASE_FEATURE(kOptimizationGuideComposeOnDeviceEval,
             "OptimizationGuideComposeOnDeviceEval",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Whether the on device service is launched after a delay on startup to log
// metrics.
BASE_FEATURE(kLogOnDeviceMetricsOnStartup,
             "LogOnDeviceMetricsOnStartup",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Whether to download the text safety classifier model.
BASE_FEATURE(kTextSafetyClassifier,
             "TextSafetyClassifier",
             base::FEATURE_DISABLED_BY_DEFAULT);

// The default value here is a bit of a guess.
// TODO(crbug/1163244): This should be tuned once metrics are available.
base::TimeDelta PageTextExtractionOutstandingRequestsGracePeriod() {
  return base::Milliseconds(GetFieldTrialParamByFeatureAsInt(
      kPageTextExtraction, "outstanding_requests_grace_period_ms", 1000));
}

bool ShouldBatchUpdateHintsForActiveTabsAndTopHosts() {
  if (base::FeatureList::IsEnabled(kRemoteOptimizationGuideFetching)) {
    // Batch update active tabs should only apply to non-desktop platforms.
    return GetFieldTrialParamByFeatureAsBool(kRemoteOptimizationGuideFetching,
                                             "batch_update_hints_for_top_hosts",
                                             enabled_by_default_mobile_only);
  }
  return false;
}

size_t MaxResultsForSRPFetch() {
  static int max_urls = GetFieldTrialParamByFeatureAsInt(
      kOptimizationGuideFetchingForSRP, "max_urls_for_srp_fetch",
      // Default to match overall max.
      MaxUrlsForOptimizationGuideServiceHintsFetch());
  return max_urls;
}

size_t MaxHostsForOptimizationGuideServiceHintsFetch() {
  return GetFieldTrialParamByFeatureAsInt(
      kRemoteOptimizationGuideFetching,
      "max_hosts_for_optimization_guide_service_hints_fetch", 30);
}

size_t MaxUrlsForOptimizationGuideServiceHintsFetch() {
  return GetFieldTrialParamByFeatureAsInt(
      kRemoteOptimizationGuideFetching,
      "max_urls_for_optimization_guide_service_hints_fetch", 30);
}

size_t MaxHostsForRecordingSuccessfullyCovered() {
  return GetFieldTrialParamByFeatureAsInt(
      kRemoteOptimizationGuideFetching,
      "max_hosts_for_recording_successfully_covered", 200);
}

base::TimeDelta StoredFetchedHintsFreshnessDuration() {
  return base::Days(GetFieldTrialParamByFeatureAsInt(
      kRemoteOptimizationGuideFetching,
      "max_store_duration_for_featured_hints_in_days", 1));
}

std::string GetOptimizationGuideServiceAPIKey() {
  // Command line override takes priority.
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  if (command_line->HasSwitch(switches::kOptimizationGuideServiceAPIKey)) {
    return command_line->GetSwitchValueASCII(
        switches::kOptimizationGuideServiceAPIKey);
  }

  return google_apis::GetAPIKey();
}

GURL GetOptimizationGuideServiceGetHintsURL() {
  // Command line override takes priority.
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  if (command_line->HasSwitch(switches::kOptimizationGuideServiceGetHintsURL)) {
    // Assume the command line switch is correct and return it.
    return GURL(command_line->GetSwitchValueASCII(
        switches::kOptimizationGuideServiceGetHintsURL));
  }

  std::string url = base::GetFieldTrialParamValueByFeature(
      kRemoteOptimizationGuideFetching, "optimization_guide_service_url");
  if (url.empty() || !GURL(url).SchemeIs(url::kHttpsScheme)) {
    if (!url.empty())
      LOG(WARNING)
          << "Empty or invalid optimization_guide_service_url provided: "
          << url;
    return GURL(kOptimizationGuideServiceGetHintsDefaultURL);
  }

  return GURL(url);
}

GURL GetOptimizationGuideServiceGetModelsURL() {
  // Command line override takes priority.
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  if (command_line->HasSwitch(
          switches::kOptimizationGuideServiceGetModelsURL)) {
    // Assume the command line switch is correct and return it.
    return GURL(command_line->GetSwitchValueASCII(
        switches::kOptimizationGuideServiceGetModelsURL));
  }

  GURL get_models_url(kOptimizationGuideServiceGetModelsDefaultURL);
  CHECK(get_models_url.SchemeIs(url::kHttpsScheme));
  return get_models_url;
}

bool IsOptimizationTargetPredictionEnabled() {
  return base::FeatureList::IsEnabled(kOptimizationTargetPrediction);
}

bool IsOptimizationHintsEnabled() {
  return base::FeatureList::IsEnabled(kOptimizationHints);
}

bool IsModelQualityLoggingEnabled() {
  return base::FeatureList::IsEnabled(kModelQualityLogging);
}

bool IsModelQualityLoggingEnabledForFeature(
    proto::ModelExecutionFeature feature) {
  if (!IsModelQualityLoggingEnabled()) {
    return false;
  }

  // Always disable logging for test features.
  if (feature ==
          proto::ModelExecutionFeature::MODEL_EXECUTION_FEATURE_UNSPECIFIED ||
      feature == proto::ModelExecutionFeature::MODEL_EXECUTION_FEATURE_TEST) {
    return false;
  }

  bool default_logging_enabled = false;
  switch (feature) {
    case proto::ModelExecutionFeature::MODEL_EXECUTION_FEATURE_COMPOSE:
    case proto::ModelExecutionFeature::MODEL_EXECUTION_FEATURE_TAB_ORGANIZATION:
    case proto::ModelExecutionFeature::MODEL_EXECUTION_FEATURE_WALLPAPER_SEARCH:
      // Enable logging when you have approvals. For new features please
      // consult with components/optimization_guide/core/model_quality/OWNERS to
      // discuss if you need logging or not for your feature.
      default_logging_enabled = true;
      break;
    case proto::ModelExecutionFeature::MODEL_EXECUTION_FEATURE_UNSPECIFIED:
    case proto::ModelExecutionFeature::MODEL_EXECUTION_FEATURE_TEST:
      // Logging disabled.
      NOTREACHED();
      break;
  }

  std::string param_name =
      base::ToLowerASCII(proto::ModelExecutionFeature_Name(feature));
  return GetFieldTrialParamByFeatureAsBool(kModelQualityLogging, param_name,
                                           default_logging_enabled);
}

bool IsRemoteFetchingEnabled() {
  return base::FeatureList::IsEnabled(kRemoteOptimizationGuideFetching);
}

bool IsSRPFetchingEnabled() {
  return base::FeatureList::IsEnabled(kOptimizationGuideFetchingForSRP);
}

bool IsPushNotificationsEnabled() {
  return base::FeatureList::IsEnabled(kPushNotifications);
}

bool IsRemoteFetchingForAnonymousDataConsentEnabled() {
  return base::FeatureList::IsEnabled(
      kRemoteOptimizationGuideFetchingAnonymousDataConsent);
}

bool IsRemoteFetchingExplicitlyAllowedForPerformanceInfo() {
  return base::FeatureList::IsEnabled(
      kContextMenuPerformanceInfoAndRemoteHintFetching);
}

int MaxServerBloomFilterByteSize() {
  return base::GetFieldTrialParamByFeatureAsInt(
      kOptimizationHints, "max_bloom_filter_byte_size", 250 * 1024 /* 250KB */);
}

base::TimeDelta GetHostHintsFetchRefreshDuration() {
  return base::Hours(GetFieldTrialParamByFeatureAsInt(
      kRemoteOptimizationGuideFetching, "hints_fetch_refresh_duration_in_hours",
      1));
}

base::TimeDelta GetActiveTabsFetchRefreshDuration() {
  return base::Hours(GetFieldTrialParamByFeatureAsInt(
      kRemoteOptimizationGuideFetching,
      "active_tabs_fetch_refresh_duration_in_hours", 1));
}

base::TimeDelta GetActiveTabsStalenessTolerance() {
  // 90 days initially chosen since that's how long local history lasts for.
  return base::Days(GetFieldTrialParamByFeatureAsInt(
      kRemoteOptimizationGuideFetching,
      "active_tabs_staleness_tolerance_in_days", 90));
}

size_t MaxConcurrentBatchUpdateFetches() {
  // If overridden, this needs to be large enough where we do not thrash the
  // inflight batch update fetches since if we approach the limit here, we will
  // abort the oldest batch update fetch that is in flight.
  return GetFieldTrialParamByFeatureAsInt(kRemoteOptimizationGuideFetching,
                                          "max_concurrent_batch_update_fetches",
                                          20);
}

size_t MaxConcurrentPageNavigationFetches() {
  // If overridden, this needs to be large enough where we do not thrash the
  // inflight page navigations since if we approach the limit here, we will
  // abort the oldest page navigation fetch that is in flight.
  return GetFieldTrialParamByFeatureAsInt(
      kRemoteOptimizationGuideFetching,
      "max_concurrent_page_navigation_fetches", 20);
}

base::TimeDelta ActiveTabsHintsFetchRandomMinDelay() {
  return base::Seconds(GetFieldTrialParamByFeatureAsInt(
      kRemoteOptimizationGuideFetching, "fetch_random_min_delay_secs", 30));
}

base::TimeDelta ActiveTabsHintsFetchRandomMaxDelay() {
  return base::Seconds(GetFieldTrialParamByFeatureAsInt(
      kRemoteOptimizationGuideFetching, "fetch_random_max_delay_secs", 60));
}

base::TimeDelta StoredHostModelFeaturesFreshnessDuration() {
  return base::Days(GetFieldTrialParamByFeatureAsInt(
      kOptimizationTargetPrediction,
      "max_store_duration_for_host_model_features_in_days", 7));
}

base::TimeDelta StoredModelsValidDuration() {
  // TODO(crbug.com/1234054) This field should not be changed without VERY
  // careful consideration. This is the default duration for models that do not
  // specify retention, so changing this can cause models to be removed and
  // refetch would only apply to newer models. Any feature relying on the model
  // would have a period of time without a valid model, and would need to push a
  // new version.
  return base::Days(GetFieldTrialParamByFeatureAsInt(
      kOptimizationTargetPrediction, "valid_duration_for_models_in_days", 30));
}

base::TimeDelta URLKeyedHintValidCacheDuration() {
  return base::Seconds(GetFieldTrialParamByFeatureAsInt(
      kOptimizationHints, "max_url_keyed_hint_valid_cache_duration_in_seconds",
      60 * 60 /* 1 hour */));
}


size_t MaxHostsForOptimizationGuideServiceModelsFetch() {
  return GetFieldTrialParamByFeatureAsInt(
      kOptimizationTargetPrediction,
      "max_hosts_for_optimization_guide_service_models_fetch", 30);
}

size_t MaxHostModelFeaturesCacheSize() {
  return GetFieldTrialParamByFeatureAsInt(
      kOptimizationTargetPrediction, "max_host_model_features_cache_size", 100);
}

size_t MaxHostKeyedHintCacheSize() {
  size_t max_host_keyed_hint_cache_size = GetFieldTrialParamByFeatureAsInt(
      kOptimizationHints, "max_host_keyed_hint_cache_size", 30);
  return max_host_keyed_hint_cache_size;
}

size_t MaxURLKeyedHintCacheSize() {
  size_t max_url_keyed_hint_cache_size = GetFieldTrialParamByFeatureAsInt(
      kOptimizationHints, "max_url_keyed_hint_cache_size", 30);
  DCHECK_GE(max_url_keyed_hint_cache_size,
            MaxUrlsForOptimizationGuideServiceHintsFetch());
  return max_url_keyed_hint_cache_size;
}

bool ShouldPersistHintsToDisk() {
  return GetFieldTrialParamByFeatureAsBool(kOptimizationHints,
                                           "persist_hints_to_disk", true);
}

RequestContextSet GetAllowedContextsForPersonalizedMetadata() {
  RequestContextSet allowed_contexts;
  if (!base::FeatureList::IsEnabled(kOptimizationGuidePersonalizedFetching)) {
    return allowed_contexts;
  }
  base::FieldTrialParams params;
  if (base::GetFieldTrialParamsByFeature(kOptimizationGuidePersonalizedFetching,
                                         &params) &&
      params.contains("allowed_contexts")) {
    for (const auto& context_str : base::SplitString(
             base::GetFieldTrialParamValueByFeature(
                 kOptimizationGuidePersonalizedFetching, "allowed_contexts"),
             ",", base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY)) {
      proto::RequestContext context;
      if (proto::RequestContext_Parse(context_str, &context)) {
        allowed_contexts.Put(context);
      }
    }
  } else {
    allowed_contexts.Put(proto::RequestContext::CONTEXT_PAGE_INSIGHTS_HUB);
  }
  return allowed_contexts;
}

bool ShouldOverrideOptimizationTargetDecisionForMetricsPurposes(
    proto::OptimizationTarget optimization_target) {
  if (optimization_target != proto::OPTIMIZATION_TARGET_PAINFUL_PAGE_LOAD)
    return false;

  return base::GetFieldTrialParamByFeatureAsBool(
      kOptimizationTargetPrediction, "painful_page_load_metrics_only", false);
}

base::TimeDelta PredictionModelFetchRandomMinDelay() {
  return base::Seconds(GetFieldTrialParamByFeatureAsInt(
      kOptimizationTargetPrediction, "fetch_random_min_delay_secs", 30));
}

base::TimeDelta PredictionModelFetchRandomMaxDelay() {
  return base::Seconds(GetFieldTrialParamByFeatureAsInt(
      kOptimizationTargetPrediction, "fetch_random_max_delay_secs", 60));
}

base::TimeDelta PredictionModelFetchRetryDelay() {
  return base::Minutes(GetFieldTrialParamByFeatureAsInt(
      kOptimizationTargetPrediction, "fetch_retry_minutes", 2));
}

base::TimeDelta PredictionModelFetchStartupDelay() {
  return base::Milliseconds(GetFieldTrialParamByFeatureAsInt(
      kOptimizationTargetPrediction, "fetch_startup_delay_ms", 10000));
}

base::TimeDelta PredictionModelFetchInterval() {
  return base::Hours(GetFieldTrialParamByFeatureAsInt(
      kOptimizationTargetPrediction, "fetch_interval_hours", 24));
}

base::TimeDelta PredictionModelNewRegistrationFetchDelay() {
  return base::Seconds(GetFieldTrialParamByFeatureAsInt(
      kOptimizationTargetPrediction, "new_registration_fetch_delay_secs", 30));
}

bool IsModelExecutionWatchdogEnabled() {
  return base::FeatureList::IsEnabled(kPreventLongRunningPredictionModels);
}

base::TimeDelta ModelExecutionWatchdogDefaultTimeout() {
  return base::Milliseconds(GetFieldTrialParamByFeatureAsInt(
      kPreventLongRunningPredictionModels, "model_execution_timeout_ms",
#if defined(_DEBUG)
      // Debug builds take a much longer time to run.
      60 * 1000
#else
      2000
#endif
      ));
}

bool IsModelDownloadingEnabled() {
  return base::FeatureList::IsEnabled(kOptimizationGuideModelDownloading);
}

bool IsUnrestrictedModelDownloadingEnabled() {
  return base::GetFieldTrialParamByFeatureAsBool(
      kOptimizationGuideModelDownloading, "unrestricted_model_downloading",
      true);
}

base::TimeDelta GetOnloadDelayForHintsFetching() {
  return base::Milliseconds(GetFieldTrialParamByFeatureAsInt(
      kRemoteOptimizationGuideFetching, "onload_delay_for_hints_fetching_ms",
      0));
}

bool ShouldMetadataValidationFetchHostKeyed() {
  DCHECK(base::FeatureList::IsEnabled(kOptimizationGuideMetadataValidation));
  return GetFieldTrialParamByFeatureAsBool(kOptimizationGuideMetadataValidation,
                                           "is_host_keyed", true);
}

bool ShouldDeferStartupActiveTabsHintsFetch() {
  return GetFieldTrialParamByFeatureAsBool(
      kOptimizationHints, "defer_startup_active_tabs_hints_fetch",
#if BUILDFLAG(IS_ANDROID)
      true
#else
      false
#endif
  );
}

std::optional<int> OverrideNumThreadsForOptTarget(
    proto::OptimizationTarget opt_target) {
  if (!base::FeatureList::IsEnabled(kOverrideNumThreadsForModelExecution)) {
    return std::nullopt;
  }

  // 0 is an invalid value to pass to TFLite, so make that nullopt. -1 is valid,
  // but not anything less than that.
  int num_threads = GetFieldTrialParamByFeatureAsInt(
      kOverrideNumThreadsForModelExecution,
      proto::OptimizationTarget_Name(opt_target), 0);
  if (num_threads == 0 || num_threads < -1) {
    return std::nullopt;
  }

  // Cap to the number of CPUs on the device.
  return std::min(num_threads, base::SysInfo::NumberOfProcessors());
}

bool TFLiteXNNPACKDelegateEnabled() {
  return base::FeatureList::IsEnabled(kOptGuideEnableXNNPACKDelegateWithTFLite);
}

bool ShouldCheckFailedComponentVersionPref() {
  return GetFieldTrialParamByFeatureAsBool(
      kOptimizationHintsComponent, "check_failed_component_version_pref",
      false);
}

std::map<proto::OptimizationTarget, std::set<int64_t>>
GetPredictionModelVersionsInKillSwitch() {
  if (!base::FeatureList::IsEnabled(
          kOptimizationGuidePredictionModelKillswitch)) {
    return {};
  }
  base::FieldTrialParams killswitch_params;
  if (!GetFieldTrialParamsByFeature(kOptimizationGuidePredictionModelKillswitch,
                                    &killswitch_params)) {
    return {};
  }
  std::map<proto::OptimizationTarget, std::set<int64_t>>
      killswitch_model_versions;
  for (const auto& killswitch_param : killswitch_params) {
    proto::OptimizationTarget opt_target;
    if (!proto::OptimizationTarget_Parse(killswitch_param.first, &opt_target)) {
      continue;
    }
    for (const std::string& opt_taget_killswitch_version :
         base::SplitString(killswitch_param.second, ",", base::TRIM_WHITESPACE,
                           base::SPLIT_WANT_NONEMPTY)) {
      int64_t opt_taget_killswitch_version_num;
      if (base::StringToInt64(opt_taget_killswitch_version,
                              &opt_taget_killswitch_version_num)) {
        killswitch_model_versions[opt_target].insert(
            opt_taget_killswitch_version_num);
      }
    }
  }
  return killswitch_model_versions;
}

bool ShouldLoadOnDeviceModelExecutionConfigWithHigherPriority() {
  return base::GetFieldTrialParamByFeatureAsBool(
      kOptimizationGuideOnDeviceModel, "ondevice_config_high_priority", true);
}

base::TimeDelta GetOnDeviceModelIdleTimeout() {
  static const base::FeatureParam<base::TimeDelta>
      kOnDeviceModelServiceIdleTimeout{&kOptimizationGuideOnDeviceModel,
                                       "on_device_model_service_idle_timeout",
                                       base::Minutes(1)};
  return kOnDeviceModelServiceIdleTimeout.Get();
}

base::TimeDelta GetOnDeviceModelExecutionValidationStartupDelay() {
  static const base::FeatureParam<base::TimeDelta>
      kOnDeviceModelExecutionValidationStartupDelay{
          &kOptimizationGuideOnDeviceModel,
          "on_device_model_execution_validation_startup_delay",
          base::Seconds(5)};
  return kOnDeviceModelExecutionValidationStartupDelay.Get();
}

int GetOnDeviceModelMinTokensForContext() {
  static const base::FeatureParam<int> kOnDeviceModelMinTokensForContext{
      &kOptimizationGuideOnDeviceModel,
      "on_device_model_min_tokens_for_context", 1024};
  return kOnDeviceModelMinTokensForContext.Get();
}

int GetOnDeviceModelMaxTokensForContext() {
  static const base::FeatureParam<int> kOnDeviceModelMaxTokensForContext{
      &kOptimizationGuideOnDeviceModel,
      "on_device_model_max_tokens_for_context", 4096};
  return kOnDeviceModelMaxTokensForContext.Get();
}

int GetOnDeviceModelContextTokenChunkSize() {
  static const base::FeatureParam<int> kOnDeviceModelContextTokenChunkSize{
      &kOptimizationGuideOnDeviceModel,
      "on_device_model_context_token_chunk_size", 512};
  return kOnDeviceModelContextTokenChunkSize.Get();
}

int GetOnDeviceModelMaxTokensForExecute() {
  static const base::FeatureParam<int> kOnDeviceModelMaxTokensForExecute{
      &kOptimizationGuideOnDeviceModel,
      "on_device_model_max_tokens_for_execute", 1024};
  return kOnDeviceModelMaxTokensForExecute.Get();
}

int GetOnDeviceModelMaxTokensForOutput() {
  static const base::FeatureParam<int> kOnDeviceModelMaxTokensForOutput{
      &kOptimizationGuideOnDeviceModel, "on_device_model_max_tokens_for_output",
      1024};
  return kOnDeviceModelMaxTokensForOutput.Get();
}

int GetOnDeviceModelCrashCountBeforeDisable() {
  static const base::FeatureParam<int> kOnDeviceModelDisableCrashCount{
      &kOptimizationGuideOnDeviceModel, "on_device_model_disable_crash_count",
      3};
  return kOnDeviceModelDisableCrashCount.Get();
}

int GetOnDeviceModelTimeoutCountBeforeDisable() {
  static const base::FeatureParam<int> kOnDeviceModelDisableTimeoutCount{
      &kOptimizationGuideOnDeviceModel, "on_device_model_disable_timeout_count",
      2};
  return kOnDeviceModelDisableTimeoutCount.Get();
}

base::TimeDelta GetOnDeviceStartupMetricDelay() {
  static const base::FeatureParam<base::TimeDelta> kOnDeviceStartupMetricDelay{
      &kLogOnDeviceMetricsOnStartup, "on_device_startup_metric_delay",
      base::Minutes(2)};
  return kOnDeviceStartupMetricDelay.Get();
}

base::TimeDelta GetOnDeviceModelTimeForInitialResponse() {
  static const base::FeatureParam<base::TimeDelta>
      kOnDeviceModelTimeForInitialResponse{
          &kOptimizationGuideOnDeviceModel,
          "on_device_time_for_initial_response", base::Seconds(15)};
  return kOnDeviceModelTimeForInitialResponse.Get();
}

bool GetOnDeviceFallbackToServerOnDisconnect() {
  static const base::FeatureParam<bool>
      kOnDeviceModelFallbackToServerOnDisconnect{
          &kOptimizationGuideOnDeviceModel,
          "on_device_fallback_to_server_on_disconnect", true};
  return kOnDeviceModelFallbackToServerOnDisconnect.Get();
}

bool IsPerformanceClassCompatibleWithOnDeviceModel(
    OnDeviceModelPerformanceClass performance_class) {
  std::string perf_classes_string = base::GetFieldTrialParamValueByFeature(
      kOptimizationGuideOnDeviceModel,
      "compatible_on_device_performance_classes");
  if (perf_classes_string.empty()) {
    perf_classes_string = "3,4,5,6";
  }
  std::vector<std::string_view> perf_classes_list = base::SplitStringPiece(
      perf_classes_string, ",", base::WhitespaceHandling::TRIM_WHITESPACE,
      base::SplitResult::SPLIT_WANT_NONEMPTY);
  return base::Contains(perf_classes_list,
                        base::ToString(static_cast<int>(performance_class)));
}

bool CanLaunchOnDeviceModelService() {
  return base::FeatureList::IsEnabled(kOptimizationGuideOnDeviceModel) ||
         base::FeatureList::IsEnabled(kLogOnDeviceMetricsOnStartup);
}

bool IsOnDeviceExecutionEnabled() {
  return base::FeatureList::IsEnabled(
             features::kOptimizationGuideModelExecution) &&
         base::FeatureList::IsEnabled(kOptimizationGuideOnDeviceModel);
}

base::TimeDelta GetOnDeviceModelRetentionTime() {
  return base::GetFieldTrialParamByFeatureAsTimeDelta(
      kOptimizationGuideOnDeviceModel, "on_device_model_retention_time",
      base::Days(30));
}

bool IsFreeDiskSpaceSufficientForOnDeviceModelInstall(
    int64_t free_disk_space_bytes) {
  return base::GetFieldTrialParamByFeatureAsInt(
             kOptimizationGuideOnDeviceModel,
             "on_device_model_free_space_mb_required_to_install",
             20 * 1024) <= free_disk_space_bytes / (1024 * 1024);
}

bool IsFreeDiskSpaceTooLowForOnDeviceModelInstall(
    int64_t free_disk_space_bytes) {
  return base::GetFieldTrialParamByFeatureAsInt(
             kOptimizationGuideOnDeviceModel,
             "on_device_model_free_space_mb_required_to_retain",
             10 * 1024) >= free_disk_space_bytes / (1024 * 1024);
}

bool GetOnDeviceModelRetractUnsafeContent() {
  static const base::FeatureParam<bool>
      kOnDeviceModelShouldRetractUnsafeContent{
          &kTextSafetyClassifier, "on_device_retract_unsafe_content", false};
  return kOnDeviceModelShouldRetractUnsafeContent.Get();
}

bool GetOnDeviceModelMustUseSafetyModel() {
  static const base::FeatureParam<bool> kOnDeviceModelMustUseSafetyModel{
      &kTextSafetyClassifier, "on_device_must_use_safety_model", false};
  return kOnDeviceModelMustUseSafetyModel.Get();
}

bool ShouldUseTextSafetyClassifierModel() {
  return base::FeatureList::IsEnabled(kTextSafetyClassifier);
}

uint32_t GetOnDeviceModelTextSafetyTokenInterval() {
  static const base::FeatureParam<int32_t>
      kOnDeviceModelTextSafetyTokenInterval{
          &kTextSafetyClassifier, "on_device_text_safety_token_interval", 10};
  return static_cast<uint32_t>(kOnDeviceModelTextSafetyTokenInterval.Get());
}

double GetOnDeviceModelLanguageDetectionMinimumReliability() {
  static const base::FeatureParam<double>
      kOnDeviceModelLanguageDetectionMinimumReliability{
          &kTextSafetyClassifier,
          "on_device_language_detection_minimum_reliability", 0.8};
  return kOnDeviceModelLanguageDetectionMinimumReliability.Get();
}

int GetOnDeviceModelNumRepeats() {
  static const base::FeatureParam<int> kOnDeviceModelNumRepeats{
      &kOptimizationGuideOnDeviceModel, "on_device_model_num_repeats", 2};
  return kOnDeviceModelNumRepeats.Get();
}

int GetOnDeviceModelMinRepeatChars() {
  static const base::FeatureParam<int> kOnDeviceModelMinRepeatChars{
      &kOptimizationGuideOnDeviceModel, "on_device_model_min_repeat_chars", 16};
  return kOnDeviceModelMinRepeatChars.Get();
}

bool GetOnDeviceModelRetractRepeats() {
  static const base::FeatureParam<bool> kOnDeviceModelRetractRepeats{
      &kOptimizationGuideOnDeviceModel, "on_device_model_retract_repeats",
      true};
  return kOnDeviceModelRetractRepeats.Get();
}

int GetOnDeviceModelDefaultTopK() {
  static const base::FeatureParam<int> kTopK{
      &optimization_guide::features::kOptimizationGuideOnDeviceModel,
      "on_device_model_topk", 3};
  return kTopK.Get();
}

double GetOnDeviceModelDefaultTemperature() {
  static const base::FeatureParam<double> kTemperature{
      &kOptimizationGuideOnDeviceModel, "on_device_model_temperature", 0.8};
  return kTemperature.Get();
}

}  // namespace features
}  // namespace optimization_guide
