// Copyright 2019 The Chromium Authors. All rights reserved.
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
#include "build/build_config.h"
#include "components/optimization_guide/core/insertion_ordered_set.h"
#include "components/optimization_guide/core/optimization_guide_constants.h"
#include "components/optimization_guide/core/optimization_guide_prefs.h"
#include "components/optimization_guide/core/optimization_guide_switches.h"
#include "components/optimization_guide/machine_learning_tflite_buildflags.h"
#include "components/prefs/pref_service.h"
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
bool IsSupportedLocaleForFeature(const std::string locale,
                                 const base::Feature& feature) {
  if (!base::FeatureList::IsEnabled(feature)) {
    return false;
  }

  std::string value =
      base::GetFieldTrialParamValueByFeature(feature, "supported_locales");
  if (value.empty()) {
    // The default list of supported locales for optimization guide features.
    value = "de,en,es,fr,it,nl,pt,tr";
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
const base::Feature kOptimizationHints{"OptimizationHints",
                                       base::FEATURE_ENABLED_BY_DEFAULT};

// Feature flag that contains a feature param that specifies the field trials
// that are allowed to be sent up to the Optimization Guide Server.
const base::Feature kOptimizationHintsFieldTrials{
    "OptimizationHintsFieldTrials", base::FEATURE_DISABLED_BY_DEFAULT};

// Enables fetching from a remote Optimization Guide Service.
const base::Feature kRemoteOptimizationGuideFetching{
    "OptimizationHintsFetching", base::FEATURE_ENABLED_BY_DEFAULT};

const base::Feature kRemoteOptimizationGuideFetchingAnonymousDataConsent {
  "OptimizationHintsFetchingAnonymousDataConsent",
#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_IOS)
      base::FEATURE_ENABLED_BY_DEFAULT
#else   // !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)
      base::FEATURE_DISABLED_BY_DEFAULT
#endif  // BUILDFLAG(IS_ANDROID)
};

// Enables performance info in the context menu and fetching from a remote
// Optimization Guide Service.
const base::Feature kContextMenuPerformanceInfoAndRemoteHintFetching{
    "ContextMenuPerformanceInfoAndRemoteHintFetching",
    base::FEATURE_DISABLED_BY_DEFAULT};

// Enables the prediction of optimization targets.
const base::Feature kOptimizationTargetPrediction{
    "OptimizationTargetPrediction", base::FEATURE_ENABLED_BY_DEFAULT};

// Enables the downloading of models.
const base::Feature kOptimizationGuideModelDownloading {
  "OptimizationGuideModelDownloading",
#if BUILDFLAG(BUILD_WITH_TFLITE_LIB)
      base::FEATURE_ENABLED_BY_DEFAULT
#else   // BUILD_WITH_TFLITE_LIB
      base::FEATURE_DISABLED_BY_DEFAULT
#endif  // !BUILD_WITH_TFLITE_LIB
};

// Enables page content to be annotated.
const base::Feature kPageContentAnnotations{"PageContentAnnotations",
                                            enabled_by_default_desktop_only};

// Enables the page entities model to be annotated on every page load.
const base::Feature kPageEntitiesPageContentAnnotations{
    "PageEntitiesPageContentAnnotations", enabled_by_default_desktop_only};
// Enables the page visibility model to be annotated on every page load.
const base::Feature kPageVisibilityPageContentAnnotations{
    "PageVisibilityPageContentAnnotations", base::FEATURE_DISABLED_BY_DEFAULT};

// This feature flag does not allow for the entities model to load the name and
// prefix filters.
const base::Feature kPageEntitiesModelBypassFilters{
    "PageEntitiesModelBypassFilters", base::FEATURE_DISABLED_BY_DEFAULT};

// This feature flag enables resetting the entities model on shutdown.
const base::Feature kPageEntitiesModelResetOnShutdown{
    "PageEntitiesModelResetOnShutdown", base::FEATURE_DISABLED_BY_DEFAULT};

// Enables push notification of hints.
const base::Feature kPushNotifications{"OptimizationGuidePushNotifications",
                                       base::FEATURE_DISABLED_BY_DEFAULT};

// This feature flag does not turn off any behavior, it is only used for
// experiment parameters.
const base::Feature kPageTextExtraction{
    "OptimizationGuidePageContentExtraction", base::FEATURE_ENABLED_BY_DEFAULT};

// Enables the validation of optimization guide metadata.
const base::Feature kOptimizationGuideMetadataValidation{
    "OptimizationGuideMetadataValidation", base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kPageTopicsBatchAnnotations{
    "PageTopicsBatchAnnotations", base::FEATURE_ENABLED_BY_DEFAULT};
const base::Feature kPageVisibilityBatchAnnotations{
    "PageVisibilityBatchAnnotations", base::FEATURE_ENABLED_BY_DEFAULT};

const base::Feature kUseLocalPageEntitiesMetadataProvider{
    "UseLocalPageEntitiesMetadataProvider", base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kBatchAnnotationsValidation{
    "BatchAnnotationsValidation", base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kPreventLongRunningPredictionModels{
    "PreventLongRunningPredictionModels", base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature
    kOptimizationGuideUseContinueOnShutdownForPageContentAnnotations{
        "OptimizationGuideUseContinueOnShutdownForPageContentAnnotations",
        base::FEATURE_ENABLED_BY_DEFAULT};

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
      "max_store_duration_for_featured_hints_in_days", 7));
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

bool IsRemoteFetchingEnabled(PrefService* pref_service) {
  return base::FeatureList::IsEnabled(kRemoteOptimizationGuideFetching) &&
         pref_service->GetBoolean(
             optimization_guide::prefs::kOptimizationGuideFetchingEnabled);
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
      72));
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
      kOptimizationTargetPrediction, "fetch_startup_delay_ms", 2000));
}

base::TimeDelta PredictionModelFetchInterval() {
  return base::Hours(GetFieldTrialParamByFeatureAsInt(
      kOptimizationTargetPrediction, "fetch_interval_hours", 24));
}

absl::optional<base::TimeDelta> ModelExecutionTimeout() {
  if (!base::FeatureList::IsEnabled(kPreventLongRunningPredictionModels)) {
    return absl::nullopt;
  }
  return base::Milliseconds(GetFieldTrialParamByFeatureAsInt(
      kPreventLongRunningPredictionModels, "model_execution_timeout_ms", 2000));
}

base::flat_set<uint32_t> FieldTrialNameHashesAllowedForFetch() {
  std::string value = base::GetFieldTrialParamValueByFeature(
      kOptimizationHintsFieldTrials, "allowed_field_trial_names");
  if (value.empty())
    return {};

  std::vector<std::string> allowed_field_trial_names = base::SplitString(
      value, ",", base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY);
  base::flat_set<uint32_t> allowed_field_trial_name_hashes;
  for (const auto& allowed_field_trial_name : allowed_field_trial_names) {
    allowed_field_trial_name_hashes.insert(
        variations::HashName(allowed_field_trial_name));
  }
  return allowed_field_trial_name_hashes;
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

uint64_t MaxSizeForPageContentTextDump() {
  return static_cast<uint64_t>(base::GetFieldTrialParamByFeatureAsInt(
      kPageContentAnnotations, "max_size_for_text_dump_in_bytes", 1024));
}

bool ShouldAnnotateTitleInsteadOfPageContent() {
  return base::GetFieldTrialParamByFeatureAsBool(
      kPageContentAnnotations, "annotate_title_instead_of_page_content", true);
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
                                     kPageVisibilityPageContentAnnotations);
}

bool RemotePageEntitiesEnabled() {
  return GetFieldTrialParamByFeatureAsBool(kPageContentAnnotations,
                                           "fetch_remote_page_entities", false);
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

bool UseLocalPageEntitiesMetadataProvider() {
  return base::FeatureList::IsEnabled(kUseLocalPageEntitiesMetadataProvider);
}

size_t AnnotateVisitBatchSize() {
  return std::max(
      1, GetFieldTrialParamByFeatureAsInt(kPageContentAnnotations,
                                          "annotate_visit_batch_size", 1));
}

bool BatchAnnotationsValidationEnabled() {
  return base::FeatureList::IsEnabled(kBatchAnnotationsValidation);
}

base::TimeDelta BatchAnnotationValidationStartupDelay() {
  return base::Seconds(
      std::max(1, GetFieldTrialParamByFeatureAsInt(kBatchAnnotationsValidation,
                                                   "startup_delay", 30)));
}

size_t BatchAnnotationsValidationBatchSize() {
  int batch_size = GetFieldTrialParamByFeatureAsInt(kBatchAnnotationsValidation,
                                                    "batch_size", 25);
  return std::max(1, batch_size);
}

bool BatchAnnotationsValidationUsePageTopics() {
  return GetFieldTrialParamByFeatureAsBool(kBatchAnnotationsValidation,
                                           "use_page_topics", false);
}

size_t MaxVisitAnnotationCacheSize() {
  int batch_size = GetFieldTrialParamByFeatureAsInt(
      kPageContentAnnotations, "max_visit_annotation_cache_size", 50);
  return std::max(1, batch_size);
}

}  // namespace features
}  // namespace optimization_guide
