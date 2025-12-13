// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PAGE_CONTENT_ANNOTATIONS_CORE_PAGE_CONTENT_ANNOTATIONS_FEATURES_H_
#define COMPONENTS_PAGE_CONTENT_ANNOTATIONS_CORE_PAGE_CONTENT_ANNOTATIONS_FEATURES_H_

#include "base/component_export.h"
#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"
#include "base/time/time.h"

namespace page_content_annotations::features {

COMPONENT_EXPORT(PAGE_CONTENT_ANNOTATIONS_FEATURES)
BASE_DECLARE_FEATURE(kPageContentAnnotations);
COMPONENT_EXPORT(PAGE_CONTENT_ANNOTATIONS_FEATURES)
BASE_DECLARE_FEATURE(kPageVisibilityPageContentAnnotations);
COMPONENT_EXPORT(PAGE_CONTENT_ANNOTATIONS_FEATURES)
BASE_DECLARE_FEATURE(kPageContentAnnotationsValidation);
COMPONENT_EXPORT(PAGE_CONTENT_ANNOTATIONS_FEATURES)
BASE_DECLARE_FEATURE(kRemotePageMetadata);
COMPONENT_EXPORT(PAGE_CONTENT_ANNOTATIONS_FEATURES)
BASE_DECLARE_FEATURE(kExtractRelatedSearchesFromPrefetchedZPSResponse);

// Enables extraction of AnnotatedPageContent for every page load.
COMPONENT_EXPORT(PAGE_CONTENT_ANNOTATIONS_FEATURES)
BASE_DECLARE_FEATURE(kAnnotatedPageContentExtraction);

// Enables the PageContentCache to store AnnotatedPageContent.
COMPONENT_EXPORT(PAGE_CONTENT_ANNOTATIONS_FEATURES)
BASE_DECLARE_FEATURE(kPageContentCache);

// The maximum number of days to keep page content in the cache.
COMPONENT_EXPORT(PAGE_CONTENT_ANNOTATIONS_FEATURES)
extern const base::FeatureParam<int> kPageContentCacheMaxCacheAgeInDays;

// The maximum number of tabs to keep page content in the cache.
COMPONENT_EXPORT(PAGE_CONTENT_ANNOTATIONS_FEATURES)
extern const base::FeatureParam<int> kPageContentCacheMaxTabs;

// Whether to enable taking screenshots for page content cache.
COMPONENT_EXPORT(PAGE_CONTENT_ANNOTATIONS_FEATURES)
extern const base::FeatureParam<bool> kPageContentCacheEnableScreenshot;

// The maximum number of "related searches" entries allowed to be maintained in
// a least-recently-used cache for "related searches" data obtained via ZPS
// prefetch logic.
COMPONENT_EXPORT(PAGE_CONTENT_ANNOTATIONS_FEATURES)
size_t MaxRelatedSearchesCacheSize();

// Enables use of task runner with trait CONTINUE_ON_SHUTDOWN for page content
// annotations on-device models.
COMPONENT_EXPORT(PAGE_CONTENT_ANNOTATIONS_FEATURES)
BASE_DECLARE_FEATURE(
    kOptimizationGuideUseContinueOnShutdownForPageContentAnnotations);

COMPONENT_EXPORT(PAGE_CONTENT_ANNOTATIONS_FEATURES)
BASE_DECLARE_FEATURE(kOnDeviceCategoryClassifier);

// Returns whether page content annotations should be enabled.
COMPONENT_EXPORT(PAGE_CONTENT_ANNOTATIONS_FEATURES)
bool ShouldEnablePageContentAnnotations();

// Whether we should write content annotations to History Service.
COMPONENT_EXPORT(PAGE_CONTENT_ANNOTATIONS_FEATURES)
bool ShouldWriteContentAnnotationsToHistoryService();

// Returns the max size of the MRU Cache of content that has been requested
// for annotation.
COMPONENT_EXPORT(PAGE_CONTENT_ANNOTATIONS_FEATURES)
size_t MaxContentAnnotationRequestsCached();

// Returns whether or not related searches should be extracted from Google SRP
// as part of page content annotations.
COMPONENT_EXPORT(PAGE_CONTENT_ANNOTATIONS_FEATURES)
bool ShouldExtractRelatedSearches();

// The number of bits used for RAPPOR-style metrics reporting on content
// annotation models. Must be at least 1 bit.
COMPONENT_EXPORT(PAGE_CONTENT_ANNOTATIONS_FEATURES)
int NumBitsForRAPPORMetrics();

// The probability of a bit flip a score with RAPPOR-style metrics reporting.
// Must be between 0 and 1.
COMPONENT_EXPORT(PAGE_CONTENT_ANNOTATIONS_FEATURES)
double NoiseProbabilityForRAPPORMetrics();

// The number of visits batch before running the page content annotation
// models. A size of 1 is equivalent to annotating one page load at time
// immediately after requested.
COMPONENT_EXPORT(PAGE_CONTENT_ANNOTATIONS_FEATURES)
size_t AnnotateVisitBatchSize();

// The time period between browser start and running a running page content
// annotation validation.
COMPONENT_EXPORT(PAGE_CONTENT_ANNOTATIONS_FEATURES)
base::TimeDelta PageContentAnnotationValidationStartupDelay();

// The size of batches to run for page content validation.
COMPONENT_EXPORT(PAGE_CONTENT_ANNOTATIONS_FEATURES)
size_t PageContentAnnotationsValidationBatchSize();

// The timeout duration to run batch processing even when the batch size is not
// full.
COMPONENT_EXPORT(PAGE_CONTENT_ANNOTATIONS_FEATURES)
base::TimeDelta PageContentAnnotationBatchSizeTimeoutDuration();

// The amount of time the PCAService will wait for the title of a page to be
// modified.
COMPONENT_EXPORT(PAGE_CONTENT_ANNOTATIONS_FEATURES)
base::TimeDelta PCAServiceWaitForTitleDelayDuration();

// Returns whether page metadata should be retrieved from the remote
// Optimization Guide service.
COMPONENT_EXPORT(PAGE_CONTENT_ANNOTATIONS_FEATURES)
bool RemotePageMetadataEnabled(const std::string& locale,
                               const std::string& country_code);

// Returns the minimum score associated with a category for it to be persisted.
// Will be a value from 0 to 100, inclusive.
COMPONENT_EXPORT(PAGE_CONTENT_ANNOTATIONS_FEATURES)
int GetMinimumPageCategoryScoreToPersist();

// Whether to persist salient image metadata for each visit.
COMPONENT_EXPORT(PAGE_CONTENT_ANNOTATIONS_FEATURES)
bool ShouldPersistSalientImageMetadata(const std::string& locale,
                                       const std::string& country_code);

// Returns whether the page visibility model should be executed on page content
// for a user using |locale| as their browser language.
COMPONENT_EXPORT(PAGE_CONTENT_ANNOTATIONS_FEATURES)
bool ShouldExecutePageVisibilityModelOnPageContent(const std::string& locale);

// The maximum size of the visit annotation cache.
COMPONENT_EXPORT(PAGE_CONTENT_ANNOTATIONS_FEATURES)
size_t MaxVisitAnnotationCacheSize();

// Returns true if AnnotatedPageContent extraction should be aggressively
// prioritized by scheduling.
COMPONENT_EXPORT(PAGE_CONTENT_ANNOTATIONS_FEATURES)
bool IsAnnotatedPageContentOnCriticalPath();

// Allows heuristically delaying the extraction for AnnotatedPageContent once
// the page has loaded so it reaches a steady state.
COMPONENT_EXPORT(PAGE_CONTENT_ANNOTATIONS_FEATURES)
base::TimeDelta GetAnnotatedPageContentCaptureDelay();

// Whether the AnnotatedPageContent study should also capture inner text for a
// comparison.
COMPONENT_EXPORT(PAGE_CONTENT_ANNOTATIONS_FEATURES)
bool ShouldAnnotatedPageContentStudyIncludeInnerText();

// The mode for extracting AnnotatedPageContent in the extraction service.
COMPONENT_EXPORT(PAGE_CONTENT_ANNOTATIONS_FEATURES)
std::string AnnotatedPageContentMode();

// The triggering mode for page content extraction.
enum class PageContentExtractionTriggeringMode {
  kOnLoad,
  kOnHidden,
  kOnLoadAndHidden,
};

// Returns the triggering mode for page content extraction.
COMPONENT_EXPORT(PAGE_CONTENT_ANNOTATIONS_FEATURES)
PageContentExtractionTriggeringMode GetPageContentExtractionTriggeringMode();

// Returns whether |locale| is a supported locale for |feature|.
//
// This matches |locale| with the "supported_locales" feature param value in
// |feature|, which is expected to be a comma-separated list of locales. A
// feature param containing "en,es-ES,zh-TW" restricts the feature to English
// language users from any locale and Spanish language users from the Spain
// es-ES locale. A feature param containing "*" is unrestricted by locale and
// any user may load it, while "" uses the |default_value| allowlist.
// Exposed for test coverage.
COMPONENT_EXPORT(PAGE_CONTENT_ANNOTATIONS_FEATURES)
extern bool IsSupportedLocaleForFeature(const std::string& locale,
                                        const base::Feature& feature,
                                        const std::string& default_value);

// Returns whether |country_code| is supported for |feature|, checking
// against |default_value| allowlist if the feature param "supported_countries"
// is unspecified. A feature param containing "*" is unrestricted.
// Exposed for test coverage.
COMPONENT_EXPORT(PAGE_CONTENT_ANNOTATIONS_FEATURES)
extern bool IsSupportedCountryForFeature(const std::string& country_code,
                                         const base::Feature& feature,
                                         const std::string& default_value);
}  // namespace page_content_annotations::features

#endif  // COMPONENTS_PAGE_CONTENT_ANNOTATIONS_CORE_PAGE_CONTENT_ANNOTATIONS_FEATURES_H_
