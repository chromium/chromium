// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/accessibility_annotator/content/content_annotator/content_classifier.h"

#include "base/strings/string_split.h"
#include "components/accessibility_annotator/content/content_annotator/content_annotator_classifier_rules_parser.h"
#include "components/accessibility_annotator/content/content_annotator/content_annotator_rule_based_classifier.h"
#include "components/accessibility_annotator/content/content_annotator/content_annotator_url_matcher_classifier.h"
#include "components/accessibility_annotator/content/content_annotator/content_classifier_types.h"
#include "components/accessibility_annotator/core/accessibility_annotator_features.h"

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

ContentClassificationResult ContentClassifier::Classify(
    const ContentClassificationInput& input) const {
  CHECK(input.adopted_language.has_value());
  CHECK(input.sensitivity_score.has_value());
  CHECK(input.page_title.has_value());

  ContentClassificationResult result;

  // 1. Check whether the page is in one of the target language(s).
  if (!supported_languages_.contains(*input.adopted_language)) {
    // If the page isn't in a supported language, do not proceed
    // with further classification.
    return result;
  }

  // 2. Check whether the page is within the sensitivity threshold.
  if (!(*input.sensitivity_score <
        kContentAnnotatorSensitivityThreshold.Get())) {
    // If the page is considered too sensitive, do not proceed with further
    // classification.
    return result;
  }

  // 3. Run value classifiers.
  if (title_keyword_classifier_ && !input.page_title->empty()) {
    ContentClassificationResult::Result title_result;
    std::optional<std::string_view> category =
        title_keyword_classifier_->Classify(*input.page_title);
    if (category) {
      title_result.category = std::string(*category);
      // TODO(crbug.com/478246547): Log the category and value.
    }
    result.title_keyword_result = title_result;
  }

  if (url_match_classifier_ && input.url.is_valid()) {
    ContentClassificationResult::Result url_result;
    std::optional<std::string_view> category =
        url_match_classifier_->Classify(input.url);
    if (category) {
      url_result.category = std::string(*category);
      // TODO(crbug.com/478246547): Log the category and value.
    }
    result.url_match_result = url_result;
  }

  return result;
}

}  // namespace accessibility_annotator
