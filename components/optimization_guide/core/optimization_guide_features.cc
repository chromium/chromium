// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/optimization_guide/core/optimization_guide_features.h"

#include "base/command_line.h"
#include "base/containers/contains.h"
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
#include "components/variations/hashing.h"
#include "google_apis/google_api_keys.h"
#include "net/base/url_util.h"
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
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enables the page entities model to be annotated on every page load.
BASE_FEATURE(kPageEntitiesPageContentAnnotations,
             "PageEntitiesPageContentAnnotations",
             enabled_by_default_desktop_only);
// Enables the page visibility model to be annotated on every page load.
BASE_FEATURE(kPageVisibilityPageContentAnnotations,
             "PageVisibilityPageContentAnnotations",
             base::FEATURE_ENABLED_BY_DEFAULT);

// This feature flag does not allow for the entities model to load the name and
// prefix filters.
BASE_FEATURE(kPageEntitiesModelBypassFilters,
             "PageEntitiesModelBypassFilters",
             enabled_by_default_desktop_only);

// This feature flag enables resetting the entities model on shutdown.
BASE_FEATURE(kPageEntitiesModelResetOnShutdown,
             "PageEntitiesModelResetOnShutdown",
             base::FEATURE_DISABLED_BY_DEFAULT);

// This feature flag enables batch entities to only be fetched via one thread
// hop.
BASE_FEATURE(kPageEntitiesModelBatchEntityMetadataSimplification,
             "PageEntitiesModelBatchEntityMetadataSimplification",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enables push notification of hints.
BASE_FEATURE(kPushNotifications,
             "OptimizationGuidePushNotifications",
             base::FEATURE_DISABLED_BY_DEFAULT);

// This feature flag does not turn off any behavior, it is only used for
// experiment parameters.
BASE_FEATURE(kPageTextExtraction,
             "OptimizationGuidePageContentExtraction",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Enables the validation of optimization guide metadata.
BASE_FEATURE(kOptimizationGuideMetadataValidation,
             "OptimizationGuideMetadataValidation",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kPageTopicsBatchAnnotations,
             "PageTopicsBatchAnnotations",
             base::FEATURE_ENABLED_BY_DEFAULT);
BASE_FEATURE(kPageVisibilityBatchAnnotations,
             "PageVisibilityBatchAnnotations",
             base::FEATURE_ENABLED_BY_DEFAULT);

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
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kExtractRelatedSearchesFromPrefetchedZPSResponse,
             "ExtractRelatedSearchesFromPrefetchedZPSResponse",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kPageContentAnnotationsPersistSalientImageMetadata,
             "PageContentAnnotationsPersistSalientImageMetadata",
             base::FEATURE_DISABLED_BY_DEFAULT);

// The default value here is a bit of a guess.
// TODO(crbug/1163244): This should be tuned once metrics are available.
base::TimeDelta PageTextExtractionOutstandingRequestsGracePeriod() {
  return base::Milliseconds(GetFieldTrialParamByFeatureAsInt(
      kPageTextExtraction, "outstanding_requests_grace_period_ms", 1000));
}

bool ShouldBatchUpdateHintsForActiveTabsAndTopHosts() {
  if (base::FeatureList::IsEnabled(kRemoteOptimizationGuideFetching)) {
    return GetFieldTrialParamByFeatureAsBool(kRemoteOptimizationGuideFetching,
                                             "batch_update_hints_for_top_hosts",
                                             true);
  }
  return false;
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

bool IsRemoteFetchingEnabled() {
  return base::FeatureList::IsEnabled(kRemoteOptimizationGuideFetching);
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

int ActiveTabsHintsFetchRandomMinDelaySecs() {
  return GetFieldTrialParamByFeatureAsInt(kRemoteOptimizationGuideFetching,
                                          "fetch_random_min_delay_secs", 30);
}

int ActiveTabsHintsFetchRandomMaxDelaySecs() {
  return GetFieldTrialParamByFeatureAsInt(kRemoteOptimizationGuideFetching,
                                          "fetch_random_max_delay_secs", 60);
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

bool ShouldOverrideOptimizationTargetDecisionForMetricsPurposes(
    proto::OptimizationTarget optimization_target) {
  if (optimization_target != proto::OPTIMIZATION_TARGET_PAINFUL_PAGE_LOAD)
    return false;

  return base::GetFieldTrialParamByFeatureAsBool(
      kOptimizationTargetPrediction, "painful_page_load_metrics_only", false);
}

int PredictionModelFetchRandomMinDelaySecs() {
  return GetFieldTrialParamByFeatureAsInt(kOptimizationTargetPrediction,
                                          "fetch_random_min_delay_secs", 30);
}

int PredictionModelFetchRandomMaxDelaySecs() {
  return GetFieldTrialParamByFeatureAsInt(kOptimizationTargetPrediction,
                                          "fetch_random_max_delay_secs", 60);
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

bool ShouldPersistSearchMetadataForNonGoogleSearches() {
  return base::GetFieldTrialParamByFeatureAsBool(
      kPageContentAnnotations,
      "persist_search_metadata_for_non_google_searches", true);
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

bool ShouldUseBatchEntityMetadataSimplication() {
  return base::FeatureList::IsEnabled(
      kPageEntitiesModelBatchEntityMetadataSimplification);
}

bool ShouldExecutePageVisibilityModelOnPageContent(const std::string& locale) {
  return base::FeatureList::IsEnabled(kPageVisibilityPageContentAnnotations) &&
         IsSupportedLocaleForFeature(locale,
                                     kPageVisibilityPageContentAnnotations,
                                     /*default_value=*/"en");
}

bool RemotePageMetadataEnabled() {
  return base::FeatureList::IsEnabled(kRemotePageMetadata);
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

bool PageTopicsBatchAnnotationsEnabled() {
  return base::FeatureList::IsEnabled(kPageTopicsBatchAnnotations);
}

bool PageVisibilityBatchAnnotationsEnabled() {
  return base::FeatureList::IsEnabled(kPageVisibilityBatchAnnotations);
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
    case AnnotationType::kPageTopics:
      return cmd->HasSwitch(
          switches::kPageContentAnnotationsValidationPageTopics);
    case AnnotationType::kPageEntities:
      return cmd->HasSwitch(
          switches::kPageContentAnnotationsValidationPageEntities);
    case AnnotationType::kContentVisibility:
      return cmd->HasSwitch(
          switches::kPageContentAnnotationsValidationContentVisibility);
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

bool ShouldPersistSalientImageMetadata() {
  return base::FeatureList::IsEnabled(
      kPageContentAnnotationsPersistSalientImageMetadata);
}

}  // namespace features
}  // namespace optimization_guide
