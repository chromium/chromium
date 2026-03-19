// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/accessibility_annotator/core/accessibility_annotator_features.h"

namespace accessibility_annotator {

BASE_FEATURE(kContentAnnotator, base::FEATURE_DISABLED_BY_DEFAULT);
BASE_FEATURE(kAccessibilityAnnotator, base::FEATURE_DISABLED_BY_DEFAULT);

const base::FeatureParam<int> kContentAnnotatorMaxPendingUrls{
    &kContentAnnotator, "content_annotator_max_pending_urls", 10};
const base::FeatureParam<std::string>
    kContentAnnotatorClassifierTitleKeywordRules{
        &kContentAnnotator, "content_annotator_classifier_title_keyword_rules",
        ""};
const base::FeatureParam<std::string> kContentAnnotatorClassifierUrlMatchRules{
    &kContentAnnotator, "content_annotator_classifier_url_match_rules", ""};
const base::FeatureParam<std::string>
    kContentAnnotatorClassifierRelevanceValues{
        &kContentAnnotator, "content_annotator_classifier_relevance_values",
        ""};
const base::FeatureParam<double> kContentAnnotatorSensitivityThreshold{
    &kContentAnnotator, "content_annotator_sensitivity_threshold", 0.5};
const base::FeatureParam<std::string> kContentAnnotatorSupportedLanguages{
    &kContentAnnotator, "content_annotator_supported_languages", "en,en-US"};
const base::FeatureParam<base::TimeDelta> kContentAnnotatorAnnotationTimeout{
    &kContentAnnotator, "content_annotator_annotation_timeout",
    base::Seconds(10)};
const base::FeatureParam<bool> kContentAnnotatorEnableFullAnnotation{
    &kContentAnnotator, "content_annotator_enable_full_annotation", false};
const base::FeatureParam<bool> kContentAnnotatorLanguageCheckEnabled{
    &kContentAnnotator, "content_annotator_language_check_enabled", true};
const base::FeatureParam<int> kContentAnnotatorMaxCacheAnnotations{
    &kContentAnnotator, "content_annotator_max_cache_annotations", 10};
const base::FeatureParam<std::string>
    kContentAnnotatorClassifierSemanticMatchRules{
        &kContentAnnotator, "content_annotator_classifier_semantic_match_rules",
        ""};
const base::FeatureParam<double> kContentAnnotatorSemanticMatchThreshold{
    &kContentAnnotator, "content_annotator_semantic_match_threshold", 0.8};

}  // namespace accessibility_annotator
