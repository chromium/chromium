// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/accessibility_annotator/content/content_annotator/content_classifier.h"

#include <string_view>

#include "base/containers/map_util.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/string_split.h"
#include "base/timer/elapsed_timer.h"
#include "components/accessibility_annotator/content/content_annotator/content_annotator_classifier_rules_parser.h"
#include "components/accessibility_annotator/content/content_annotator/content_annotator_rule_based_classifier.h"
#include "components/accessibility_annotator/content/content_annotator/content_annotator_url_matcher_classifier.h"
#include "components/accessibility_annotator/content/content_annotator/content_classifier_types.h"
#include "components/accessibility_annotator/core/accessibility_annotator_features.h"
#include "components/variations/hashing.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "services/metrics/public/cpp/ukm_recorder.h"

namespace accessibility_annotator {

std::unique_ptr<ContentClassifier> ContentClassifier::Create() {
  std::unique_ptr<ContentAnnotatorRuleBasedClassifier> title_keyword_classifier;
  const std::string& title_keyword_rules =
      kContentAnnotatorClassifierTitleKeywordRules.Get();
  if (!title_keyword_rules.empty()) {
    title_keyword_classifier =
        ContentAnnotatorRuleBasedClassifier::Create(title_keyword_rules);
    if (!title_keyword_classifier) {
      return nullptr;
    }
  }

  std::unique_ptr<ContentAnnotatorUrlMatcherClassifier> url_match_classifier;
  const std::string& url_match_rules =
      kContentAnnotatorClassifierUrlMatchRules.Get();
  if (!url_match_rules.empty()) {
    url_match_classifier =
        ContentAnnotatorUrlMatcherClassifier::Create(url_match_rules);
    if (!url_match_classifier) {
      return nullptr;
    }
  }

  // Parse the category to relevance value mappings.
  base::flat_map<std::string, ContentClassifierRelevance>
      classifier_relevance_values;
  const std::string& relevance_values_json =
      kContentAnnotatorClassifierRelevanceValues.Get();
  if (!relevance_values_json.empty()) {
    classifier_relevance_values =
        ParseRelevanceValuesFromJson(relevance_values_json);
    if (classifier_relevance_values.empty()) {
      return nullptr;
    }
  }

  // Parse the supported languages.
  base::flat_set<std::string> supported_languages(
      base::SplitString(kContentAnnotatorSupportedLanguages.Get(), ",",
                        base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY));

  return std::make_unique<ContentClassifier>(
      PassKey(), std::move(title_keyword_classifier),
      std::move(url_match_classifier), std::move(classifier_relevance_values),
      std::move(supported_languages));
}

ContentClassifier::ContentClassifier(
    PassKey pass_key,
    std::unique_ptr<ContentAnnotatorRuleBasedClassifier>
        title_keyword_classifier,
    std::unique_ptr<ContentAnnotatorUrlMatcherClassifier> url_match_classifier,
    base::flat_map<std::string, ContentClassifierRelevance>
        classifier_relevance_values,
    base::flat_set<std::string> supported_languages)
    : title_keyword_classifier_(std::move(title_keyword_classifier)),
      url_match_classifier_(std::move(url_match_classifier)),
      classifier_relevance_values_(std::move(classifier_relevance_values)),
      supported_languages_(std::move(supported_languages)) {}

ContentClassifier::~ContentClassifier() = default;
ContentClassifier::ContentClassifier(ContentClassifier&&) = default;
ContentClassifier& ContentClassifier::operator=(ContentClassifier&&) = default;

// For testing.
ContentClassifier::ContentClassifier() = default;

// static
std::string_view ContentClassifier::ClassifierResultStatusToString(
    ClassifierResultStatus status) {
  switch (status) {
    case ClassifierResultStatus::kInconclusiveNoMatch:
      return "InconclusiveNoMatch";
    case ClassifierResultStatus::kDidNotRunMissingClassifier:
      return "DidNotRun.MissingClassifier";
    case ClassifierResultStatus::kDidNotRunEmptyPageTitle:
      return "DidNotRun.EmptyPageTitle";
    case ClassifierResultStatus::kDidNotRunInvalidUrl:
      return "DidNotRun.InvalidUrl";
    case ClassifierResultStatus::kDidNotRunMissingClassifierEmptyPageTitle:
      return "DidNotRun.MissingClassifierEmptyPageTitle";
    case ClassifierResultStatus::kDidNotRunMissingClassifierInvalidUrl:
      return "DidNotRun.MissingClassifierInvalidUrl";
  }
}

int64_t ContentClassifier::GetRelevanceAsInt(
    std::optional<std::string_view> category) const {
  ContentClassifierRelevance relevance =
      ContentClassifierRelevance::kLowContentRelevance;
  if (category) {
    if (auto* found_relevance =
            base::FindOrNull(classifier_relevance_values_, *category)) {
      relevance = *found_relevance;
    }
  }
  return static_cast<int64_t>(relevance);
}

ContentClassificationResult ContentClassifier::Classify(
    const ContentClassificationInput& input) const {
  CHECK(input.adopted_language.has_value());
  CHECK(input.sensitivity_score.has_value());
  CHECK(input.page_title.has_value());

  ContentClassificationResult result;
  ukm::builders::AccessibilityAnnotator_ContentAnnotator_ClassifierResults
      ukm_builder(input.ukm_source_id);

  // 1. Check whether the page is in one of the target language(s).
  result.is_in_target_language =
      supported_languages_.contains(*input.adopted_language);
  base::UmaHistogramBoolean("AccessibilityAnnotator.LanguageCheck",
                            result.is_in_target_language.value());
  ukm_builder.SetIsTargetLanguage(result.is_in_target_language.value());

  // 2. Check whether the page is within the sensitivity threshold.
  result.is_sensitive =
      *input.sensitivity_score < kContentAnnotatorSensitivityThreshold.Get();
  base::UmaHistogramBoolean("AccessibilityAnnotator.SensitivityCheck",
                            result.is_sensitive.value());

  // 3. Run value classifiers.
  std::string title_classifier_result(ClassifierResultStatusToString(
      ClassifierResultStatus::kDidNotRunMissingClassifierEmptyPageTitle));
  if (title_keyword_classifier_ && !input.page_title->empty()) {
    base::ElapsedTimer timer;
    ContentClassificationResult::Result title_result;
    std::optional<std::string_view> category =
        title_keyword_classifier_->Classify(*input.page_title);
    base::UmaHistogramTimes(
        "AccessibilityAnnotator.TitleKeywordClassifierDuration",
        timer.Elapsed());

    if (category) {
      title_result.category = std::string(*category);
    }
    ukm_builder.SetTitleKeywordResult(GetRelevanceAsInt(title_result.category));
    result.title_keyword_result = title_result;
    title_classifier_result =
        result.title_keyword_result->category
            ? *result.title_keyword_result->category
            : std::string(ClassifierResultStatusToString(
                  ClassifierResultStatus::kInconclusiveNoMatch));
  } else if (!input.page_title->empty()) {
    // Classifier missing, but page title is valid.
    title_classifier_result = std::string(ClassifierResultStatusToString(
        ClassifierResultStatus::kDidNotRunMissingClassifier));
  } else if (title_keyword_classifier_) {
    // Classifier present, but page title is empty.
    title_classifier_result = std::string(ClassifierResultStatusToString(
        ClassifierResultStatus::kDidNotRunEmptyPageTitle));
  }
  base::UmaHistogramSparse(
      "AccessibilityAnnotator.TitleKeywordClassifierResult",
      variations::HashName(title_classifier_result));

  std::string url_classifier_result(ClassifierResultStatusToString(
      ClassifierResultStatus::kDidNotRunMissingClassifierInvalidUrl));
  if (url_match_classifier_ && input.url.is_valid()) {
    base::ElapsedTimer timer;
    ContentClassificationResult::Result url_result;
    std::optional<std::string_view> category =
        url_match_classifier_->Classify(input.url);
    base::UmaHistogramTimes("AccessibilityAnnotator.UrlClassifierDuration",
                            timer.Elapsed());

    if (category) {
      url_result.category = std::string(*category);
    }
    ukm_builder.SetUrlMatchResult(GetRelevanceAsInt(url_result.category));
    result.url_match_result = url_result;
    url_classifier_result =
        result.url_match_result->category
            ? *result.url_match_result->category
            : std::string(ClassifierResultStatusToString(
                  ClassifierResultStatus::kInconclusiveNoMatch));
  } else if (input.url.is_valid()) {
    // Classifier missing, but URL is valid.
    url_classifier_result = std::string(ClassifierResultStatusToString(
        ClassifierResultStatus::kDidNotRunMissingClassifier));
  } else if (url_match_classifier_) {
    // Classifier present, but URL is invalid.
    url_classifier_result = std::string(ClassifierResultStatusToString(
        ClassifierResultStatus::kDidNotRunInvalidUrl));
  }
  base::UmaHistogramSparse("AccessibilityAnnotator.UrlClassifierResult",
                           variations::HashName(url_classifier_result));

  ukm_builder.Record(ukm::UkmRecorder::Get());
  return result;
}

}  // namespace accessibility_annotator
