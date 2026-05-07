// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/accessibility_annotator/core/accessibility_annotator_features.h"

namespace accessibility_annotator::features {

BASE_FEATURE(kContentAnnotator, base::FEATURE_DISABLED_BY_DEFAULT);
BASE_FEATURE_PARAM(int,
                   kContentAnnotatorMaxPendingUrls,
                   &kContentAnnotator,
                   "content_annotator_max_pending_urls",
                   10);
BASE_FEATURE_PARAM(std::string,
                   kContentAnnotatorClassifierTitleKeywordRules,
                   &kContentAnnotator,
                   "content_annotator_classifier_title_keyword_rules",
                   "");
BASE_FEATURE_PARAM(std::string,
                   kContentAnnotatorClassifierUrlMatchRules,
                   &kContentAnnotator,
                   "content_annotator_classifier_url_match_rules",
                   "");
BASE_FEATURE_PARAM(std::string,
                   kContentAnnotatorClassifierRelevanceValues,
                   &kContentAnnotator,
                   "content_annotator_classifier_relevance_values",
                   "");
BASE_FEATURE_PARAM(double,
                   kContentAnnotatorSensitivityThreshold,
                   &kContentAnnotator,
                   "content_annotator_sensitivity_threshold",
                   0.5);
BASE_FEATURE_PARAM(std::string,
                   kContentAnnotatorSupportedLanguages,
                   &kContentAnnotator,
                   "content_annotator_supported_languages",
                   "en,en-US");
BASE_FEATURE_PARAM(base::TimeDelta,
                   kContentAnnotatorAnnotationTimeout,
                   &kContentAnnotator,
                   "content_annotator_annotation_timeout",
                   base::Seconds(10));
BASE_FEATURE_PARAM(bool,
                   kContentAnnotatorEnableFullAnnotation,
                   &kContentAnnotator,
                   "content_annotator_enable_full_annotation",
                   false);
BASE_FEATURE_PARAM(bool,
                   kContentAnnotatorLanguageCheckEnabled,
                   &kContentAnnotator,
                   "content_annotator_language_check_enabled",
                   true);
BASE_FEATURE_PARAM(int,
                   kContentAnnotatorMaxCacheAnnotations,
                   &kContentAnnotator,
                   "content_annotator_max_cache_annotations",
                   10);
BASE_FEATURE_PARAM(base::TimeDelta,
                   kContentAnnotatorConfirmedStatusLookbackWindow,
                   &kContentAnnotator,
                   "content_annotator_confirmed_status_lookback_window",
                   base::Minutes(20));
BASE_FEATURE_PARAM(std::string,
                   kContentAnnotatorClassifierSemanticMatchRules,
                   &kContentAnnotator,
                   "content_annotator_classifier_semantic_match_rules",
                   "");
BASE_FEATURE_PARAM(double,
                   kContentAnnotatorSemanticMatchThreshold,
                   &kContentAnnotator,
                   "content_annotator_semantic_match_threshold",
                   0.8);
BASE_FEATURE_PARAM(bool,
                   kContentAnnotatorEnableMultiTabAnnotations,
                   &kContentAnnotator,
                   "content_annotator_enable_multi_tab_annotations",
                   false);

BASE_FEATURE(kAccessibilityAnnotator, base::FEATURE_DISABLED_BY_DEFAULT);
BASE_FEATURE_PARAM(base::TimeDelta,
                   kAccessibilityAnnotationTTL,
                   &kAccessibilityAnnotator,
                   "accessibility_annotation_ttl",
                   base::Days(7));
BASE_FEATURE_PARAM(std::string,
                   kAccessibilityAnnotatorEligibleTiers,
                   &kAccessibilityAnnotator,
                   "accessibility_annotator_eligible_tiers",
                   "1,2");

BASE_FEATURE(kAccessibilityAnnotatorFirstRun,
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kAccessibilityAnnotatorFirstRunPhase2,
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kAccessibilityAnnotatorGetEntities,
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kAccessibilityAnnotatorLiveTabContext,
             base::FEATURE_DISABLED_BY_DEFAULT);
BASE_FEATURE_PARAM(int,
                   kAccessibilityAnnotatorLiveTabContextMaxSearchResults,
                   &kAccessibilityAnnotatorLiveTabContext,
                   "live_tab_context_max_search_results",
                   3);
// TODO(b/496946516): Evaluate passage count latency vs recall tradeoffs.
BASE_FEATURE_PARAM(int,
                   kAccessibilityAnnotatorLiveTabContextPassagesPerPage,
                   &kAccessibilityAnnotatorLiveTabContext,
                   "live_tab_context_passages_per_page",
                   10);
BASE_FEATURE_PARAM(base::TimeDelta,
                   kAccessibilityAnnotatorLiveTabContextRequestTimeout,
                   &kAccessibilityAnnotatorLiveTabContext,
                   "live_tab_context_request_timeout",
                   base::Seconds(3));

BASE_FEATURE(kAccessibilityAnnotationReducerOnePResolver,
             base::FEATURE_DISABLED_BY_DEFAULT);
BASE_FEATURE_PARAM(std::string,
                   kAccessibilityAnnotatorOnePServiceUrl,
                   &kAccessibilityAnnotationReducerOnePResolver,
                   "one_p_service_url",
                   "");

// TODO(crbug.com/484049558): Remove this feature once the SQLite database
// storage is ready with the initial schema as default storage.
// Enables the accessibility annotator database storage. This will allow the
// accessibility annotator backend to create the SQLite database.
BASE_FEATURE(kAccessibilityAnnotatorDatabaseStorage,
             base::FEATURE_DISABLED_BY_DEFAULT);

bool IsAccessibilityAnnotatorFirstRunEnabled() {
  return base::FeatureList::IsEnabled(kAccessibilityAnnotatorFirstRun) ||
         base::FeatureList::IsEnabled(kAccessibilityAnnotatorFirstRunPhase2);
}

}  // namespace accessibility_annotator::features
