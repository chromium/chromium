// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/accessibility_annotator/content/content_annotator/content_classifier.h"

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

  return std::make_unique<ContentClassifier>(
      PassKey(), std::move(title_keyword_classifier),
      std::move(url_match_classifier));
}

ContentClassifier::ContentClassifier(
    PassKey pass_key,
    std::unique_ptr<ContentAnnotatorRuleBasedClassifier>
        title_keyword_classifier,
    std::unique_ptr<ContentAnnotatorUrlMatcherClassifier> url_match_classifier)
    : title_keyword_classifier_(std::move(title_keyword_classifier)),
      url_match_classifier_(std::move(url_match_classifier)) {}

ContentClassifier::~ContentClassifier() = default;
ContentClassifier::ContentClassifier(ContentClassifier&&) = default;
ContentClassifier& ContentClassifier::operator=(ContentClassifier&&) = default;

// For testing.
ContentClassifier::ContentClassifier() = default;

ContentClassificationResult ContentClassifier::Classify(
    const ContentClassificationInput& input) const {
  ContentClassificationResult result;

  if (title_keyword_classifier_ && input.page_title &&
      !input.page_title->empty()) {
    ContentClassificationResult::Result title_result;
    std::optional<std::string_view> category =
        title_keyword_classifier_->Classify(*input.page_title);
    if (category) {
      title_result.category = std::string(*category);
    }
    result.title_keyword_result = title_result;
  }

  if (url_match_classifier_ && input.url.is_valid()) {
    ContentClassificationResult::Result url_result;
    std::optional<std::string_view> category =
        url_match_classifier_->Classify(input.url);
    if (category) {
      url_result.category = std::string(*category);
    }
    result.url_match_result = url_result;
  }

  return result;
}

}  // namespace accessibility_annotator
