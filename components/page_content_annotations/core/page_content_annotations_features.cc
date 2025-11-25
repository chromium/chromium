// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/page_content_annotations/core/page_content_annotations_features.h"

#include "base/containers/contains.h"
#include "base/metrics/field_trial.h"
#include "base/metrics/field_trial_params.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "build/build_config.h"
#include "components/page_content_annotations/core/page_content_annotations_switches.h"
#include "ui/base/l10n/l10n_util.h"

namespace page_content_annotations::features {

namespace {

constexpr auto enabled_by_default_desktop_only =
#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_IOS)
    base::FEATURE_DISABLED_BY_DEFAULT;
#else
    base::FEATURE_ENABLED_BY_DEFAULT;
#endif

constexpr auto enabled_by_default_non_ios =
#if BUILDFLAG(IS_IOS)
    base::FEATURE_DISABLED_BY_DEFAULT;
#else
    base::FEATURE_ENABLED_BY_DEFAULT;
#endif

constexpr auto enabled_by_default_non_arm32 =
#if defined(ARCH_CPU_ARMEL)
    base::FEATURE_DISABLED_BY_DEFAULT;
#else
    base::FEATURE_ENABLED_BY_DEFAULT;
#endif

constexpr char enabled_all_mobile_locales_en_us_desktop_only[] =
#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_IOS)
    "*";
#else
    "en-US";
#endif

constexpr char enabled_all_mobile_countries_us_desktop_only[] =
#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_IOS)
    "*";
#else
    "us";
#endif

const base::FeatureParam<base::TimeDelta> kAnnotatedPageContentCaptureDelay{
    &kAnnotatedPageContentExtraction, "capture_delay", base::Seconds(5)};

const base::FeatureParam<bool> kAnnotatedPageContentStudyIncludeInnerText{
    &kAnnotatedPageContentExtraction, "include_inner_text", false};

const base::FeatureParam<bool> kAnnotatedPageContentOnCriticalPath{
    &kAnnotatedPageContentExtraction, "on_critical_path", false};

const base::FeatureParam<std::string> kAnnotatedPageContentMode{
    &kAnnotatedPageContentExtraction, "mode", "default"};

const base::FeatureParam<std::string> kPageContentExtractionTriggeringMode{
    &kAnnotatedPageContentExtraction, "triggering_mode", "on_load"};

bool IsSupportedLocale(const std::string& locale,
                       const std::string& supported_locales) {
  if (supported_locales == "*") {
    return true;
  }

  std::vector<std::string> supported = base::SplitString(
      supported_locales, ",", base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY);
  // An empty admits any locale.
  if (supported.empty()) {
    return true;
  }

  // Otherwise, the locale or the primary language subtag must match an element
  // of the allowlist.
  return base::Contains(supported, locale) ||
         base::Contains(supported, l10n_util::GetLanguage(locale));
}

bool IsSupportedCountry(const std::string& country_code,
                        const std::string& supported_countries) {
  if (supported_countries == "*") {
    return true;
  }

  std::vector<std::string> supported =
      base::SplitString(supported_countries, ",", base::TRIM_WHITESPACE,
                        base::SPLIT_WANT_NONEMPTY);
  // An empty allowlist admits any country.
  if (supported.empty()) {
    return true;
  }

  return std::ranges::any_of(
      supported, [&country_code](const auto& supported_country_code) {
        return base::EqualsCaseInsensitiveASCII(supported_country_code,
                                                country_code);
      });
}

}  // namespace

// Enables page content to be annotated.
BASE_FEATURE(kPageContentAnnotations, base::FEATURE_ENABLED_BY_DEFAULT);

// Enables the page visibility model to be annotated on every page load.
BASE_FEATURE(kPageVisibilityPageContentAnnotations,
             enabled_by_default_non_arm32);

BASE_FEATURE(kPageContentAnnotationsValidation,
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enables fetching page metadata from the remote Optimization Guide service.
BASE_FEATURE(kRemotePageMetadata, base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kOptimizationGuideUseContinueOnShutdownForPageContentAnnotations,
             enabled_by_default_non_ios);

BASE_FEATURE(kExtractRelatedSearchesFromPrefetchedZPSResponse,
             enabled_by_default_desktop_only);

BASE_FEATURE(kAnnotatedPageContentExtraction,
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kOnDeviceCategoryClassifier, base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kPageContentCache, base::FEATURE_DISABLED_BY_DEFAULT);

const base::FeatureParam<int> kPageContentCacheMaxCacheAgeInDays{
    &kPageContentCache, "max_cache_age_in_days", 7};

const base::FeatureParam<int> kPageContentCacheMaxTabs{
    &kPageContentCache, "max_cache_tabs_count", 50};

const base::FeatureParam<bool> kPageContentCacheEnableScreenshot{
    &kPageContentCache, "enable_screenshot", false};

base::TimeDelta PCAServiceWaitForTitleDelayDuration() {
  return base::Milliseconds(GetFieldTrialParamByFeatureAsInt(
      kPageContentAnnotations,
      "pca_service_wait_for_title_delay_in_milliseconds", 5000));
}

bool ShouldEnablePageContentAnnotations() {
  // Allow for the validation experiment or remote page metadata to enable the
  // PCAService without need to enable both features.
  return base::FeatureList::IsEnabled(kPageContentAnnotations) ||
         base::FeatureList::IsEnabled(page_content_annotations::features::
                                          kPageContentAnnotationsValidation) ||
         base::FeatureList::IsEnabled(
             page_content_annotations::features::kRemotePageMetadata) ||
         base::FeatureList::IsEnabled(kOnDeviceCategoryClassifier);
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

bool ShouldExecutePageVisibilityModelOnPageContent(const std::string& locale) {
  return base::FeatureList::IsEnabled(kPageVisibilityPageContentAnnotations) &&
         IsSupportedLocaleForFeature(
             locale, kPageVisibilityPageContentAnnotations,
             /*default_value=*/"ar,en,es,fa,fr,hi,id,pl,pt,tr,vi");
}

bool RemotePageMetadataEnabled(const std::string& locale,
                               const std::string& country_code) {
  return base::FeatureList::IsEnabled(kRemotePageMetadata) &&
         IsSupportedLocaleForFeature(
             locale, kRemotePageMetadata,
             enabled_all_mobile_locales_en_us_desktop_only) &&
         IsSupportedCountryForFeature(
             country_code, kRemotePageMetadata,
             enabled_all_mobile_countries_us_desktop_only);
}

int GetMinimumPageCategoryScoreToPersist() {
  return GetFieldTrialParamByFeatureAsInt(kRemotePageMetadata,
                                          "min_page_category_score", 85);
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

size_t AnnotateVisitBatchSize() {
  // When new visits are synced, the service gets visit notifications in a loop.
  // The service drops new visits during processing a batch. Often only the
  // `kDefaultBatchSize` entries are annotated when new visits are synced. Set
  // the limit to 5 since up to 5 URLs are shown on tab resume module.
  constexpr int kDefaultBatchSize = 5;
  return std::max(1, GetFieldTrialParamByFeatureAsInt(
                         kPageContentAnnotations, "annotate_visit_batch_size",
                         kDefaultBatchSize));
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

base::TimeDelta PageContentAnnotationBatchSizeTimeoutDuration() {
  return base::Seconds(GetFieldTrialParamByFeatureAsInt(
      kPageContentAnnotations, "batch_annotations_timeout_seconds", 30));
}

size_t MaxVisitAnnotationCacheSize() {
  int batch_size = GetFieldTrialParamByFeatureAsInt(
      kPageContentAnnotations, "max_visit_annotation_cache_size", 50);
  return std::max(1, batch_size);
}

bool ShouldPersistSalientImageMetadata(const std::string& locale,
                                       const std::string& country_code) {
  return IsSupportedLocale(locale,
                           enabled_all_mobile_locales_en_us_desktop_only) &&
         IsSupportedCountry(country_code,
                            enabled_all_mobile_countries_us_desktop_only);
}

size_t MaxRelatedSearchesCacheSize() {
  return GetFieldTrialParamByFeatureAsInt(
      kExtractRelatedSearchesFromPrefetchedZPSResponse,
      "max_related_searches_cache_size", 10);
}

bool IsAnnotatedPageContentOnCriticalPath() {
  return kAnnotatedPageContentOnCriticalPath.Get();
}

base::TimeDelta GetAnnotatedPageContentCaptureDelay() {
  return kAnnotatedPageContentCaptureDelay.Get();
}

bool ShouldAnnotatedPageContentStudyIncludeInnerText() {
  return kAnnotatedPageContentStudyIncludeInnerText.Get();
}

std::string AnnotatedPageContentMode() {
  return kAnnotatedPageContentMode.Get();
}

PageContentExtractionTriggeringMode GetPageContentExtractionTriggeringMode() {
  std::string mode_str = kPageContentExtractionTriggeringMode.Get();
  if (mode_str == "on_hidden") {
    return PageContentExtractionTriggeringMode::kOnHidden;
  }
  if (mode_str == "on_load_and_hidden") {
    return PageContentExtractionTriggeringMode::kOnLoadAndHidden;
  }
  return PageContentExtractionTriggeringMode::kOnLoad;
}

bool IsSupportedLocaleForFeature(
    const std::string& locale,
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
  }

  return IsSupportedLocale(locale, value);
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
  }

  return IsSupportedCountry(country_code, value);
}

}  // namespace page_content_annotations::features
