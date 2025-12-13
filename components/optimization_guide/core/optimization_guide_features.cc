// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/optimization_guide/core/optimization_guide_features.h"

#include <cstring>
#include <optional>

#include "base/byte_count.h"
#include "base/command_line.h"
#include "base/containers/contains.h"
#include "base/containers/enum_set.h"
#include "base/feature_list.h"
#include "base/logging.h"
#include "base/metrics/field_trial.h"
#include "base/metrics/field_trial_params.h"
#include "base/rand_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/to_string.h"
#include "base/system/sys_info.h"
#include "base/time/time.h"
#include "components/optimization_guide/core/feature_registry/mqls_feature_registry.h"
#include "components/optimization_guide/core/model_execution/feature_keys.h"
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
BASE_FEATURE(kOptimizationHints, base::FEATURE_ENABLED_BY_DEFAULT);

// Enables the prediction of optimization targets.
BASE_FEATURE(kOptimizationTargetPrediction, base::FEATURE_ENABLED_BY_DEFAULT);

// This feature flag does not turn off any behavior, it is only used for
// experiment parameters.
BASE_FEATURE(kPageTextExtraction,
             "OptimizationGuidePageContentExtraction",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Enables the validation of optimization guide metadata.
BASE_FEATURE(kOptimizationGuideMetadataValidation,
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kPreventLongRunningPredictionModels,
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kOverrideNumThreadsForModelExecution,
             base::FEATURE_DISABLED_BY_DEFAULT);

// Killswitch for fetching on search results from a remote Optimization Guide
// Service.
BASE_FEATURE(kOptimizationGuideFetchingForSRP,
             "OptimizationHintsFetchingSRP",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Kill switch for disabling model quality logging.
BASE_FEATURE(kModelQualityLogging, base::FEATURE_ENABLED_BY_DEFAULT);

// An emergency kill switch feature to stop serving certain model versions per
// optimization target. This is useful in exceptional situations when a bad
// model version got served that lead to crashes or critical failures, and an
// immediate remedy is needed to stop serving those versions.
BASE_FEATURE(kOptimizationGuidePredictionModelKillswitch,
             base::FEATURE_DISABLED_BY_DEFAULT);

// Whether to enable model execution.
BASE_FEATURE(kOptimizationGuideModelExecution,
             base::FEATURE_ENABLED_BY_DEFAULT);

// Whether to use the on device model service in optimization guide.
BASE_FEATURE(kOptimizationGuideOnDeviceModel,
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX) || \
    BUILDFLAG(IS_CHROMEOS)
             base::FEATURE_ENABLED_BY_DEFAULT);
#else
             base::FEATURE_DISABLED_BY_DEFAULT);
#endif

// Whether the on device service is launched after a delay on startup to log
// metrics.
BASE_FEATURE(kLogOnDeviceMetricsOnStartup, base::FEATURE_DISABLED_BY_DEFAULT);

// Whether to download the text safety classifier model.
BASE_FEATURE(kTextSafetyClassifier, base::FEATURE_ENABLED_BY_DEFAULT);

// Whether to scan the full text when running the language detection in the text
// safety classifier.
BASE_FEATURE(kTextSafetyScanLanguageDetection,
             base::FEATURE_DISABLED_BY_DEFAULT);

// Whether performance class should be fetched each startup or just after a
// version update.
BASE_FEATURE(kOnDeviceModelFetchPerformanceClassEveryStartup,
             base::FEATURE_DISABLED_BY_DEFAULT);

// Force show the AI page and all AI feature sub-pages in settings, even if they
// would be unavailable otherwise. This is meant for development and test
// purposes only.
BASE_FEATURE(kAiSettingsPageForceAvailable, base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kOnDeviceModelPerformanceParams, base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kAnnotatedPageContentWithActionableElements,
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kAnnotatedPageContentWithMediaData,
             base::FEATURE_DISABLED_BY_DEFAULT);

const base::FeatureParam<std::string> kPerformanceClassListForOnDeviceModel{
    &kOnDeviceModelPerformanceParams,
    "compatible_on_device_performance_classes", "3,4,5,6"};

const base::FeatureParam<std::string>
    kLowTierPerformanceClassListForOnDeviceModel{
        &kOnDeviceModelPerformanceParams,
        "compatible_low_tier_on_device_performance_classes", "3,4"};

const base::FeatureParam<std::string> kPerformanceClassListForImageInput{
    &kOnDeviceModelPerformanceParams,
    "compatible_on_device_performance_classes_image_input", "3,4,5,6"};

const base::FeatureParam<std::string> kPerformanceClassListForAudioInput{
    &kOnDeviceModelPerformanceParams,
    "compatible_on_device_performance_classes_audio_input", "5,6"};

BASE_FEATURE(kOptimizationGuideIconView, base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kBrokerModelSessionsForUntrustedProcesses,
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enables proactively sending GAIA information to the Optimization Guide
// Service.
BASE_FEATURE(kOptimizationGuideProactivePersonalizedHintsFetching,
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kOptimizationGuideBypassFormsClassificationAuth,
             base::FEATURE_DISABLED_BY_DEFAULT);

// Controls whether to enforce a timeout for subframe page content extraction.
// If enabled, defaults to 1 second. If disabled, wait indefinitely for all
// subframes to respond.
BASE_FEATURE(kGetAIPageContentSubframeTimeoutEnabled,
             base::FEATURE_ENABLED_BY_DEFAULT);
const base::FeatureParam<base::TimeDelta> kGetAIPageContentSubframeTimeoutParam{
    &kGetAIPageContentSubframeTimeoutEnabled, "timeout", base::Seconds(1)};

// Controls whether to enforce a timeout for main frame page content extraction.
// If enabled, defaults to 10 seconds. If disabled, wait indefinitely for the
// main frame to respond.
BASE_FEATURE(kGetAIPageContentMainFrameTimeoutEnabled,
             base::FEATURE_ENABLED_BY_DEFAULT);
const base::FeatureParam<base::TimeDelta>
    kGetAIPageContentMainFrameTimeoutParam{
        &kGetAIPageContentMainFrameTimeoutEnabled, "timeout",
        base::Seconds(10)};

// The default value here is a bit of a guess.
// TODO(crbug.com/40163041): This should be tuned once metrics are available.
base::TimeDelta PageTextExtractionOutstandingRequestsGracePeriod() {
  return base::Milliseconds(GetFieldTrialParamByFeatureAsInt(
      kPageTextExtraction, "outstanding_requests_grace_period_ms", 1000));
}

size_t MaxResultsForSRPFetch() {
  static int max_urls = GetFieldTrialParamByFeatureAsInt(
      kOptimizationGuideFetchingForSRP, "max_urls_for_srp_fetch", 10);
  return max_urls;
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
    const MqlsFeatureMetadata* metadata) {
  if (!IsModelQualityLoggingEnabled()) {
    return false;
  }

  return metadata->LoggingEnabledViaFieldTrial();
}

bool IsSRPFetchingEnabled() {
  return base::FeatureList::IsEnabled(kOptimizationGuideFetchingForSRP);
}

bool IsPushNotificationsEnabled() {
  return enabled_by_default_mobile_only;
}

size_t MaxHostKeyedHintCacheSize() {
  size_t max_host_keyed_hint_cache_size = GetFieldTrialParamByFeatureAsInt(
      kOptimizationHints, "max_host_keyed_hint_cache_size", 30);
  return max_host_keyed_hint_cache_size;
}

bool ShouldPersistHintsToDisk() {
  return GetFieldTrialParamByFeatureAsBool(kOptimizationHints,
                                           "persist_hints_to_disk", true);
}

RequestContextSet GetAllowedContextsForPersonalizedMetadata() {
  RequestContextSet allowed_contexts;
  allowed_contexts.Put(proto::RequestContext::CONTEXT_PAGE_INSIGHTS_HUB);
  return allowed_contexts;
}

OptimizationTypeSet GetAllowedOptimizationTypesForProactivePersonalization() {
  OptimizationTypeSet allowed_optimization_types;
  if (!base::FeatureList::IsEnabled(
          kOptimizationGuideProactivePersonalizedHintsFetching)) {
    return allowed_optimization_types;
  }
  base::FieldTrialParams params;
  if (base::GetFieldTrialParamsByFeature(
          kOptimizationGuideProactivePersonalizedHintsFetching, &params) &&
      params.contains("allowed_optimization_types")) {
    for (const auto& context_str : base::SplitString(
             base::GetFieldTrialParamValueByFeature(
                 kOptimizationGuideProactivePersonalizedHintsFetching,
                 "allowed_optimization_types"),
             ",", base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY)) {
      proto::OptimizationType optimization_type;
      if (proto::OptimizationType_Parse(context_str, &optimization_type)) {
        allowed_optimization_types.Put(optimization_type);
      }
    }
  }
  return allowed_optimization_types;
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

base::TimeDelta PredictionModelNewRegistrationFetchRandomDelay() {
  static const base::FeatureParam<base::TimeDelta> kMinDelay{
      &kOptimizationTargetPrediction, "new_registration_fetch_min_delay",
      base::Seconds(5)};
  static const base::FeatureParam<base::TimeDelta> kMaxDelay{
      &kOptimizationTargetPrediction, "new_registration_fetch_max_delay",
      base::Seconds(10)};
  return base::RandTimeDelta(kMinDelay.Get(), kMaxDelay.Get());
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

int GetOnDeviceModelCrashCountBeforeDisable() {
  static const base::FeatureParam<int> kOnDeviceModelDisableCrashCount{
      &kOptimizationGuideOnDeviceModel, "on_device_model_disable_crash_count",
      3};
  return kOnDeviceModelDisableCrashCount.Get();
}

base::TimeDelta GetOnDeviceModelMaxCrashBackoffTime() {
  static const base::FeatureParam<base::TimeDelta>
      kOnDeviceModelMaxCrashBackoffTime{
          &kOptimizationGuideOnDeviceModel,
          "on_device_model_max_crash_backoff_time", base::Hours(1)};
  return kOnDeviceModelMaxCrashBackoffTime.Get();
}

base::TimeDelta GetOnDeviceModelCrashBackoffBaseTime() {
  static const base::FeatureParam<base::TimeDelta>
      kOnDeviceModelCrashBackoffBaseTime{
          &kOptimizationGuideOnDeviceModel,
          "on_device_model_crash_backoff_base_time", base::Minutes(1)};
  return kOnDeviceModelCrashBackoffBaseTime.Get();
}

base::TimeDelta GetOnDeviceStartupMetricDelay() {
  static const base::FeatureParam<base::TimeDelta> kOnDeviceStartupMetricDelay{
      &kLogOnDeviceMetricsOnStartup, "on_device_startup_metric_delay",
      base::Minutes(3)};
  return kOnDeviceStartupMetricDelay.Get();
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

base::TimeDelta GetOnDeviceEligibleModelFeatureRecentUsePeriod() {
  return base::GetFieldTrialParamByFeatureAsTimeDelta(
      kOptimizationGuideOnDeviceModel,
      "on_device_model_feature_recent_use_period", base::Days(30));
}

base::TimeDelta GetOnDeviceModelRetentionTime() {
  return base::GetFieldTrialParamByFeatureAsTimeDelta(
      kOptimizationGuideOnDeviceModel, "on_device_model_retention_time",
      base::Days(30));
}

base::ByteCount GetDiskSpaceRequiredForOnDeviceModelInstall() {
  return base::MiB(base::GetFieldTrialParamByFeatureAsInt(
      kOptimizationGuideOnDeviceModel,
      "on_device_model_free_space_mb_required_to_install",
      base::GiB(20).InMiB()));
}

bool IsFreeDiskSpaceSufficientForOnDeviceModelInstall(
    base::ByteCount free_disk_space_bytes) {
  return GetDiskSpaceRequiredForOnDeviceModelInstall() <= free_disk_space_bytes;
}

bool IsFreeDiskSpaceTooLowForOnDeviceModelInstall(
    base::ByteCount free_disk_space_bytes) {
  return base::MiB(base::GetFieldTrialParamByFeatureAsInt(
             kOptimizationGuideOnDeviceModel,
             "on_device_model_free_space_mb_required_to_retain",
             base::GiB(5).InMiB())) >= free_disk_space_bytes;
}

bool GetOnDeviceModelRetractUnsafeContent() {
  static const base::FeatureParam<bool>
      kOnDeviceModelShouldRetractUnsafeContent{
          &kTextSafetyClassifier, "on_device_retract_unsafe_content", true};
  return kOnDeviceModelShouldRetractUnsafeContent.Get();
}

bool ShouldUseTextSafetyClassifierModel() {
  return base::FeatureList::IsEnabled(kTextSafetyClassifier);
}

bool ShouldUseGeneralizedSafetyModel() {
  static const base::FeatureParam<bool> kUseGeneralizedSafetyModel{
      &kTextSafetyClassifier, "use_generalized_safety_model", false};
  return kUseGeneralizedSafetyModel.Get();
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

int GetOnDeviceModelMaxTopK() {
  static const base::FeatureParam<int> kMaxTopK{
      &optimization_guide::features::kOptimizationGuideOnDeviceModel,
      "on_device_model_max_topk", 128};
  return kMaxTopK.Get();
}

double GetOnDeviceModelDefaultTemperature() {
  static const base::FeatureParam<double> kTemperature{
      &kOptimizationGuideOnDeviceModel, "on_device_model_temperature", 0.8};
  return kTemperature.Get();
}

std::vector<uint32_t> GetOnDeviceModelAllowedAdaptationRanks() {
  static const base::FeatureParam<std::string>
      kOnDeviceModelAllowedAdaptationRanks{&kOptimizationGuideOnDeviceModel,
                                           "allowed_adaptation_ranks", "32"};
  std::vector<uint32_t> ranks;
  const auto ranks_str = kOnDeviceModelAllowedAdaptationRanks.Get();
  auto rank_strs = base::SplitStringPiece(
      ranks_str, ",", base::WhitespaceHandling::TRIM_WHITESPACE,
      base::SplitResult::SPLIT_WANT_NONEMPTY);
  ranks.reserve(rank_strs.size());
  for (const auto& rank_str : rank_strs) {
    uint32_t rank;
    if (base::StringToUint(rank_str, &rank)) {
      ranks.push_back(rank);
    }
  }
  return ranks;
}

bool ShouldEnableOptimizationGuideIconView() {
  return base::FeatureList::IsEnabled(kOptimizationGuideIconView);
}

std::optional<base::TimeDelta> GetSubframeGetAIPageContentTimeout() {
  if (!base::FeatureList::IsEnabled(kGetAIPageContentSubframeTimeoutEnabled)) {
    return std::nullopt;
  }
  return kGetAIPageContentSubframeTimeoutParam.Get();
}

std::optional<base::TimeDelta> GetMainFrameGetAIPageContentTimeout() {
  if (!base::FeatureList::IsEnabled(kGetAIPageContentMainFrameTimeoutEnabled)) {
    return std::nullopt;
  }
  return kGetAIPageContentMainFrameTimeoutParam.Get();
}

}  // namespace features
}  // namespace optimization_guide
