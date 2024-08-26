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

// Enables page content to be annotated.
BASE_FEATURE(kPageContentAnnotations,
             "PageContentAnnotations",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Enables the page visibility model to be annotated on every page load.
BASE_FEATURE(kPageVisibilityPageContentAnnotations,
             "PageVisibilityPageContentAnnotations",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kPageVisibilityBatchAnnotations,
             "PageVisibilityBatchAnnotations",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kTextEmbeddingBatchAnnotations,
             "TextEmbeddingBatchAnnotations",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kPageContentAnnotationsValidation,
             "PageContentAnnotationsValidation",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enables fetching page metadata from the remote Optimization Guide service.
BASE_FEATURE(kRemotePageMetadata,
             "RemotePageMetadata",
             enabled_by_default_desktop_only);

BASE_FEATURE(kOptimizationGuideUseContinueOnShutdownForPageContentAnnotations,
             "OptimizationGuideUseContinueOnShutdownForPageContentAnnotations",
             enabled_by_default_non_ios);

BASE_FEATURE(kPageContentAnnotationsPersistSalientImageMetadata,
             "PageContentAnnotationsPersistSalientImageMetadata",
             enabled_by_default_desktop_only);

BASE_FEATURE(kExtractRelatedSearchesFromPrefetchedZPSResponse,
             "ExtractRelatedSearchesFromPrefetchedZPSResponse",
             enabled_by_default_desktop_only);

// Enables text embeddings to annotated on every page visit and later queried.
BASE_FEATURE(kQueryInMemoryTextEmbeddings,
             "QueryInMemoryTextEmbeddings",
             base::FEATURE_DISABLED_BY_DEFAULT);

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
             page_content_annotations::features::kRemotePageMetadata);
}

bool ShouldQueryEmbeddings() {
  return (base::FeatureList::IsEnabled(kQueryInMemoryTextEmbeddings));
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
         IsSupportedLocaleForFeature(locale, kRemotePageMetadata, "en-US") &&
         IsSupportedCountryForFeature(country_code, kRemotePageMetadata, "us");
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

bool PageVisibilityBatchAnnotationsEnabled() {
  return base::FeatureList::IsEnabled(kPageVisibilityBatchAnnotations);
}

bool TextEmbeddingBatchAnnotationsEnabled() {
  return base::FeatureList::IsEnabled(kTextEmbeddingBatchAnnotations);
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
  return base::FeatureList::IsEnabled(
             kPageContentAnnotationsPersistSalientImageMetadata) &&
         IsSupportedLocaleForFeature(
             locale, kPageContentAnnotationsPersistSalientImageMetadata,
             "en-US") &&
         IsSupportedCountryForFeature(
             country_code, kPageContentAnnotationsPersistSalientImageMetadata,
             "us");
}

size_t MaxRelatedSearchesCacheSize() {
  return GetFieldTrialParamByFeatureAsInt(
      kExtractRelatedSearchesFromPrefetchedZPSResponse,
      "max_related_searches_cache_size", 10);
}

}  // namespace page_content_annotations::features
