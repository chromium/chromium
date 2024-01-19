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
#include "base/system/sys_info.h"
#include "build/build_config.h"
#include "components/optimization_guide/core/insertion_ordered_set.h"
#include "components/optimization_guide/core/optimization_guide_constants.h"
#include "components/optimization_guide/core/optimization_guide_switches.h"
#include "components/optimization_guide/machine_learning_tflite_buildflags.h"
#include "components/optimization_guide/proto/common_types.pb.h"
#include "components/variations/hashing.h"
#include "google_apis/gaia/gaia_constants.h"
#include "google_apis/google_api_keys.h"
#include "net/base/url_util.h"
#include "optimization_guide_features.h"
#include "ui/base/l10n/l10n_util.h"

namespace optimization_guide {
namespace features {

namespace {

constexpr auto enabled_by_default_desktop_only =
#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_IOS)
    base::FEATURE_DISABLED_BY_DEFAULT;
#else
    base::FEATURE_ENABLED_BY_DEFAULT;
#endif

constexpr auto enabled_by_default_mobile_only =
#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_IOS)
    true;
#else
    false;
#endif

constexpr auto enabled_by_default_ios_only =
#if BUILDFLAG(IS_IOS)
    base::FEATURE_ENABLED_BY_DEFAULT;
#else
    base::FEATURE_DISABLED_BY_DEFAULT;
#endif

// Returns whether |locale| is a supported locale for |feature|.
//
// This matches |locale| with the "supported_locales" feature param value in
// |feature|, which is expected to be a comma-separated list of locales. A
// feature param containing "en,es-ES,zh-TW" restricts the feature to English
// language users from any locale and Spanish language users from the Spain
// es-ES locale. A feature param containing "" is unrestricted by locale and any
// user may load it.
bool IsSupportedLocaleForFeature(
    const std::string locale,
    const base::Feature& feature,
    const std::string& default_value = "de,en,es,fr,it,nl,pt,tr") {
  if (!base::FeatureList::IsEnabled(feature)) {
    return false;
  }

  std::string value =
      base::GetFieldTrialParamValueByFeature(feature, "supported_locales");
  if (value.empty()) {
    // The default list of supported locales for optimization guide features.
    value = default_value;
  } else if (value == "*") {
    // Still provide a way to enable all locales remotely via the '*' character.
    return true;
  }

  std::vector<std::string> supported_locales = base::SplitString(
      value, ",", base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY);
  // An empty allowlist admits any locale.
  if (supported_locales.empty()) {
    return true;
  }

  // Otherwise, the locale or the
  // primary language subtag must match an element of the allowlist.
  std::string locale_language = l10n_util::GetLanguage(locale);
  return base::Contains(supported_locales, locale) ||
         base::Contains(supported_locales, locale_language);
}

bool IsSupportedCountryForFeature(const std::string& country_code,
                                  const base::Feature& feature,
                                  const std::string& default_value) {
  if (!base::FeatureList::IsEnabled(feature)) {
    return false;
  }

  std::string value =
      base::GetFieldTrialParamValueByFeature(feature, "supported_countries");
  if (value.empty()) {
    // The default list of supported countries for optimization guide features.
    value = default_value;
  } else if (value == "*") {
    // Still provide a way to enable all countries remotely via the '*'
    // character.
    return true;
  }

  std::vector<std::string> supported_countries = base::SplitString(
      value, ",", base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY);
  // An empty allowlist admits any country.
  if (supported_countries.empty()) {
    return true;
  }

  return base::ranges::any_of(
      supported_countries, [&country_code](const auto& supported_country_code) {
        return base::EqualsCaseInsensitiveASCII(supported_country_code,
                                                country_code);
      });
}

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

// Enables page content to be annotated.
BASE_FEATURE(kPageContentAnnotations,
             "PageContentAnnotations",
             enabled_by_default_desktop_only);

// Enables fetching page metadata from the remote Optimization Guide service.
BASE_FEATURE(kRemotePageMetadata,
             "RemotePageMetadata",
             enabled_by_default_desktop_only);

// Enables the page entities model to be annotated on every page load.
BASE_FEATURE(kPageEntitiesPageContentAnnotations,
             "PageEntitiesPageContentAnnotations",
             enabled_by_default_desktop_only);
// Enables the page visibility model to be annotated on every page load.
BASE_FEATURE(kPageVisibilityPageContentAnnotations,
             "PageVisibilityPageContentAnnotations",
             base::FEATURE_ENABLED_BY_DEFAULT);
// Enables the text embedding model to be annotated on every page load.
BASE_FEATURE(kTextEmbeddingPageContentAnnotations,
             "TextEmbeddingPageContentAnnotations",
             base::FEATURE_DISABLED_BY_DEFAULT);

// This feature flag does not allow for the entities model to load the name and
// prefix filters.
BASE_FEATURE(kPageEntitiesModelBypassFilters,
             "PageEntitiesModelBypassFilters",
             base::FEATURE_ENABLED_BY_DEFAULT);

// This feature flag enables resetting the entities model on shutdown.
BASE_FEATURE(kPageEntitiesModelResetOnShutdown,
             "PageEntitiesModelResetOnShutdown",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enables push notification of hints.
BASE_FEATURE(kPushNotifications,
             "OptimizationGuidePushNotifications",
             enabled_by_default_ios_only);

// This feature flag does not turn off any behavior, it is only used for
// experiment parameters.
BASE_FEATURE(kPageTextExtraction,
             "OptimizationGuidePageContentExtraction",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Enables the validation of optimization guide metadata.
BASE_FEATURE(kOptimizationGuideMetadataValidation,
             "OptimizationGuideMetadataValidation",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kPageVisibilityBatchAnnotations,
             "PageVisibilityBatchAnnotations",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kTextEmbeddingBatchAnnotations,
             "TextEmbeddingBatchAnnotations",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kPageContentAnnotationsValidation,
             "PageContentAnnotationsValidation",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kPreventLongRunningPredictionModels,
             "PreventLongRunningPredictionModels",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kOptimizationGuideUseContinueOnShutdownForPageContentAnnotations,
             "OptimizationGuideUseContinueOnShutdownForPageContentAnnotations",
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

// Enables the new model store that is tied with Chrome installation and shares
// the models across user profiles.
BASE_FEATURE(kOptimizationGuideInstallWideModelStore,
             "OptimizationGuideInstallWideModelStore",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kExtractRelatedSearchesFromPrefetchedZPSResponse,
             "ExtractRelatedSearchesFromPrefetchedZPSResponse",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kPageContentAnnotationsPersistSalientImageMetadata,
             "PageContentAnnotationsPersistSalientImageMetadata",
             enabled_by_default_desktop_only);

// Killswitch for fetching on search results from a remote Optimization Guide
// Service.
BASE_FEATURE(kOptimizationGuideFetchingForSRP,
             "OptimizationHintsFetchingSRP",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Enables the model store to save relative paths computed from the base model
// store dir. Storing as relative path in the model store is needed for IOS,
// since the directories could change after Chrome upgrade. This feature is
// expected to be enabled only for IOS.
BASE_FEATURE(kModelStoreUseRelativePath,
             "ModelStoreUseRelativePath",
#if BUILDFLAG(IS_IOS)
             base::FEATURE_ENABLED_BY_DEFAULT
#else
             base::FEATURE_DISABLED_BY_DEFAULT
#endif
);

// Kill switch for disabling model quality logging.
BASE_FEATURE(kModelQualityLogging,
             "ModelQualityLogging",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Enables fetching personalized metadata from Optimization Guide Service.
BASE_FEATURE(kOptimizationGuidePersonalizedFetching,
             "OptimizationPersonalizedHintsFetching",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enables text embeddings to annotated on every page visit and later queried.
BASE_FEATURE(kQueryInMemoryTextEmbeddings,
             "QueryInMemoryTextEmbeddings",
             base::FEATURE_DISABLED_BY_DEFAULT);

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

// Whether the on device service is launched after a delay on startup to log
// metrics.
BASE_FEATURE(kLogOnDeviceMetricsOnStartup,
             "LogOnDeviceMetricsOnStartup",
             base::FEATURE_DISABLED_BY_DEFAULT);

size_t MaxRelatedSearchesCacheSize() {
  return GetFieldTrialParamByFeatureAsInt(
      kExtractRelatedSearchesFromPrefetchedZPSResponse,
      "max_related_searches_cache_size", 10);
}

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
    proto::ModelExecutionFeature feature_name) {
  if (!IsModelQualityLoggingEnabled()) {
    return false;
  }

  if (feature_name ==
      proto::ModelExecutionFeature::MODEL_EXECUTION_FEATURE_TEST) {
    return false;
  }

  std::string param_name =
      base::ToLowerASCII(proto::ModelExecutionFeature_Name(feature_name));
  bool default_value = true;

  // Disable compose feature by default.
  if (feature_name ==
      proto::ModelExecutionFeature::MODEL_EXECUTION_FEATURE_COMPOSE) {
    default_value = false;
  }
  return GetFieldTrialParamByFeatureAsBool(kModelQualityLogging, param_name,
                                           default_value);
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

base::TimeDelta PCAServiceWaitForTitleDelayDuration() {
  return base::Milliseconds(GetFieldTrialParamByFeatureAsInt(
      kPageContentAnnotations,
      "pca_service_wait_for_title_delay_in_milliseconds", 5000));
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

bool ShouldEnablePersonalizedMetadata(proto::RequestContext request_context) {
  if (!base::FeatureList::IsEnabled(kOptimizationGuidePersonalizedFetching)) {
    return false;
  }
  using RequestContextSet =
      base::EnumSet<proto::RequestContext, proto::RequestContext_MIN,
                    proto::RequestContext_MAX>;

  static const RequestContextSet allowed_contexts = []() -> RequestContextSet {
    DCHECK(
        base::FeatureList::IsEnabled(kOptimizationGuidePersonalizedFetching));
    std::string param = base::GetFieldTrialParamValueByFeature(
        kOptimizationGuidePersonalizedFetching, "allowed_contexts");
    RequestContextSet allowed_contexts;
    for (const auto& context_str : base::SplitString(
             param, ",", base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY)) {
      proto::RequestContext context;
      if (proto::RequestContext_Parse(context_str, &context)) {
        allowed_contexts.Put(context);
      }
    }
    return allowed_contexts;
  }();

  return allowed_contexts.Has(request_context);
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

bool IsPredictionModelNewRegistrationFetchEnabled() {
  return GetFieldTrialParamByFeatureAsBool(
      kOptimizationGuideInstallWideModelStore, "new_registration_fetch_enabled",
      true);
}

base::TimeDelta PredictionModelNewRegistrationFetchDelay() {
  return base::Seconds(GetFieldTrialParamByFeatureAsInt(
      kOptimizationGuideInstallWideModelStore,
      "new_registration_fetch_delay_secs", 30));
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

bool IsPageContentAnnotationEnabled() {
  return base::FeatureList::IsEnabled(kPageContentAnnotations);
}

bool ShouldWriteContentAnnotationsToHistoryService() {
  return base::GetFieldTrialParamByFeatureAsBool(
      kPageContentAnnotations, "write_to_history_service", true);
}

size_t MaxContentAnnotationRequestsCached() {
  return GetFieldTrialParamByFeatureAsInt(
      kPageContentAnnotations, "max_content_annotation_requests_cached", 50);
}

const base::FeatureParam<bool> kContentAnnotationsExtractRelatedSearchesParam{
    &kPageContentAnnotations, "extract_related_searches", true};

bool ShouldExtractRelatedSearches() {
  return kContentAnnotationsExtractRelatedSearchesParam.Get();
}

bool ShouldExecutePageEntitiesModelOnPageContent(const std::string& locale) {
  return base::FeatureList::IsEnabled(kPageEntitiesPageContentAnnotations) &&
         IsSupportedLocaleForFeature(locale,
                                     kPageEntitiesPageContentAnnotations);
}

bool ShouldExecutePageVisibilityModelOnPageContent(const std::string& locale) {
  return base::FeatureList::IsEnabled(kPageVisibilityPageContentAnnotations) &&
         IsSupportedLocaleForFeature(locale,
                                     kPageVisibilityPageContentAnnotations,
                                     /*default_value=*/"en");
}

bool ShouldExecuteTextEmbeddingModelOnPageContent(const std::string& locale) {
  return (base::FeatureList::IsEnabled(kTextEmbeddingPageContentAnnotations) ||
          TextEmbeddingBatchAnnotationsEnabled()) &&
         IsSupportedLocaleForFeature(locale,
                                     kTextEmbeddingPageContentAnnotations);
}

bool RemotePageMetadataEnabled(const std::string& locale,
                               const std::string& country_code) {
  return base::FeatureList::IsEnabled(kRemotePageMetadata) &&
         IsSupportedLocaleForFeature(locale, kRemotePageMetadata, "en-US") &&
         IsSupportedCountryForFeature(country_code, kRemotePageMetadata, "us");
}

int GetMinimumPageCategoryScoreToPersist() {
  return GetFieldTrialParamByFeatureAsInt(kRemotePageMetadata,
                                          "min_page_category_score", 85);
}

base::TimeDelta GetOnloadDelayForHintsFetching() {
  return base::Milliseconds(GetFieldTrialParamByFeatureAsInt(
      kRemoteOptimizationGuideFetching, "onload_delay_for_hints_fetching_ms",
      0));
}

int NumBitsForRAPPORMetrics() {
  // The number of bits must be at least 1.
  return std::max(
      1, GetFieldTrialParamByFeatureAsInt(kPageContentAnnotations,
                                          "num_bits_for_rappor_metrics", 4));
}

double NoiseProbabilityForRAPPORMetrics() {
  // The noise probability must be between 0 and 1.
  return std::max(0.0, std::min(1.0, GetFieldTrialParamByFeatureAsDouble(
                                         kPageContentAnnotations,
                                         "noise_prob_for_rappor_metrics", .5)));
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

bool PageVisibilityBatchAnnotationsEnabled() {
  return base::FeatureList::IsEnabled(kPageVisibilityBatchAnnotations);
}

bool TextEmbeddingBatchAnnotationsEnabled() {
  return base::FeatureList::IsEnabled(kTextEmbeddingBatchAnnotations);
}

size_t AnnotateVisitBatchSize() {
  return std::max(
      1, GetFieldTrialParamByFeatureAsInt(kPageContentAnnotations,
                                          "annotate_visit_batch_size", 1));
}

bool PageContentAnnotationValidationEnabledForType(AnnotationType type) {
  if (base::FeatureList::IsEnabled(kPageContentAnnotationsValidation)) {
    if (GetFieldTrialParamByFeatureAsBool(kPageContentAnnotationsValidation,
                                          AnnotationTypeToString(type),
                                          false)) {
      return true;
    }
  }

  base::CommandLine* cmd = base::CommandLine::ForCurrentProcess();
  switch (type) {
    case AnnotationType::kPageEntities:
      return cmd->HasSwitch(
          switches::kPageContentAnnotationsValidationPageEntities);
    case AnnotationType::kContentVisibility:
      return cmd->HasSwitch(
          switches::kPageContentAnnotationsValidationContentVisibility);
    case AnnotationType::kTextEmbedding:
      return cmd->HasSwitch(
          switches::kPageContentAnnotationsValidationTextEmbedding);
    default:
      NOTREACHED();
      break;
  }

  return false;
}

base::TimeDelta PageContentAnnotationValidationStartupDelay() {
  return switches::PageContentAnnotationsValidationStartupDelay().value_or(
      base::Seconds(std::max(
          1, GetFieldTrialParamByFeatureAsInt(kPageContentAnnotationsValidation,
                                              "startup_delay", 30))));
}

size_t PageContentAnnotationsValidationBatchSize() {
  return switches::PageContentAnnotationsValidationBatchSize().value_or(
      std::max(1, GetFieldTrialParamByFeatureAsInt(
                      kPageContentAnnotationsValidation, "batch_size", 25)));
}

size_t MaxVisitAnnotationCacheSize() {
  int batch_size = GetFieldTrialParamByFeatureAsInt(
      kPageContentAnnotations, "max_visit_annotation_cache_size", 50);
  return std::max(1, batch_size);
}

absl::optional<int> OverrideNumThreadsForOptTarget(
    proto::OptimizationTarget opt_target) {
  if (!base::FeatureList::IsEnabled(kOverrideNumThreadsForModelExecution)) {
    return absl::nullopt;
  }

  // 0 is an invalid value to pass to TFLite, so make that nullopt. -1 is valid,
  // but not anything less than that.
  int num_threads = GetFieldTrialParamByFeatureAsInt(
      kOverrideNumThreadsForModelExecution,
      proto::OptimizationTarget_Name(opt_target), 0);
  if (num_threads == 0 || num_threads < -1) {
    return absl::nullopt;
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

bool IsInstallWideModelStoreEnabled() {
  return base::FeatureList::IsEnabled(kOptimizationGuideInstallWideModelStore);
}

bool ShouldPersistSalientImageMetadata(const std::string& locale,
                                       const std::string& country_code) {
  return base::FeatureList::IsEnabled(
             kPageContentAnnotationsPersistSalientImageMetadata) &&
         IsSupportedLocaleForFeature(
             locale, kPageContentAnnotationsPersistSalientImageMetadata,
             "en-US") &&
         IsSupportedCountryForFeature(
             country_code, kPageContentAnnotationsPersistSalientImageMetadata,
             "us");
}

bool ShouldQueryEmbeddings() {
  return (base::FeatureList::IsEnabled(kQueryInMemoryTextEmbeddings));
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

base::TimeDelta GetOnDeviceModelIdleTimeout() {
  static const base::FeatureParam<base::TimeDelta>
      kOnDeviceModelServiceIdleTimeout{&kOptimizationGuideOnDeviceModel,
                                       "on_device_model_service_idle_timeout",
                                       base::Minutes(1)};
  return kOnDeviceModelServiceIdleTimeout.Get();
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

bool CanLaunchOnDeviceModelService() {
  return base::FeatureList::IsEnabled(kOptimizationGuideOnDeviceModel) ||
         base::FeatureList::IsEnabled(kLogOnDeviceMetricsOnStartup);
}

}  // namespace features
}  // namespace optimization_guide
