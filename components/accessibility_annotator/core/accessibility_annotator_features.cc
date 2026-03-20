// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/accessibility_annotator/core/accessibility_annotator_features.h"

namespace accessibility_annotator {

BASE_FEATURE(kContentAnnotator, base::FEATURE_DISABLED_BY_DEFAULT);
BASE_FEATURE(kAccessibilityAnnotator, base::FEATURE_DISABLED_BY_DEFAULT);
BASE_FEATURE(kAccessibilityAnnotatorGetEntities,
             base::FEATURE_DISABLED_BY_DEFAULT);

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
BASE_FEATURE_PARAM(std::string,
                   kContentAnnotatorExtractedDataValidationSchema,
                   &kContentAnnotator,
                   "content_annotator_extracted_data_validation_schema",
                   "");

BASE_FEATURE(kAccessibilityAnnotationReducerOnePResolver,
             base::FEATURE_DISABLED_BY_DEFAULT);

}  // namespace accessibility_annotator
