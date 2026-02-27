// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ACCESSIBILITY_ANNOTATOR_CONTENT_CONTENT_ANNOTATOR_CONTENT_CLASSIFIER_H_
#define COMPONENTS_ACCESSIBILITY_ANNOTATOR_CONTENT_CONTENT_ANNOTATOR_CONTENT_CLASSIFIER_H_

#include <memory>
#include <optional>
#include <string>
#include <string_view>

#include "base/containers/flat_map.h"
#include "base/containers/flat_set.h"
#include "base/time/time.h"
#include "base/types/pass_key.h"
#include "components/accessibility_annotator/content/content_annotator/content_annotator_rule_based_classifier.h"
#include "components/accessibility_annotator/content/content_annotator/content_annotator_url_matcher_classifier.h"
#include "components/accessibility_annotator/content/content_annotator/content_classifier_types.h"
#include "url/gurl.h"

namespace accessibility_annotator {

enum class ContentClassifierRelevance;

// This class encapsulates the logic for content classification. It is designed
// to be extensible with multiple individual classifiers.
class ContentClassifier {
 public:
  enum class ClassifierResultStatus {
    kInconclusiveNoMatch,
    kDidNotRunMissingClassifier,
    kDidNotRunEmptyPageTitle,
    kDidNotRunInvalidUrl,
    kDidNotRunMissingClassifierEmptyPageTitle,
    kDidNotRunMissingClassifierInvalidUrl,
  };

  static std::string_view ClassifierResultStatusToString(
      ClassifierResultStatus status);

  using PassKey = base::PassKey<ContentClassifier>;

  // Creates a ContentClassifier, fully initialized with all sub-classifiers.
  // Returns nullptr if unparsable rules are provided for all sub-classifiers.
  static std::unique_ptr<ContentClassifier> Create();

  explicit ContentClassifier(
      PassKey pass_key,
      std::unique_ptr<ContentAnnotatorRuleBasedClassifier>
          title_keyword_classifier,
      std::unique_ptr<ContentAnnotatorUrlMatcherClassifier>
          url_match_classifier,
      base::flat_map<std::string, ContentClassifierRelevance>
          classifier_relevance_values,
      base::flat_set<std::string> supported_languages);

  ContentClassifier(const ContentClassifier&) = delete;
  ContentClassifier& operator=(const ContentClassifier&) = delete;
  virtual ~ContentClassifier();
  ContentClassifier(ContentClassifier&&);
  ContentClassifier& operator=(ContentClassifier&&);

  // Runs all classifiers on the given input and returns the result.
  // Classifiers are run only if the input is complete and the classifier
  // supports the input data.
  virtual ContentClassificationResult Classify(
      const ContentClassificationInput& input) const;

 protected:
  // For testing.
  ContentClassifier();

 private:
  // Resolves the relevance classifier value for UKM logging.
  int64_t GetRelevanceAsInt(std::optional<std::string_view> category) const;

  // The classifier for matching keywords in the page title.
  std::unique_ptr<ContentAnnotatorRuleBasedClassifier>
      title_keyword_classifier_;
  // The classifier for matching URLs.
  std::unique_ptr<ContentAnnotatorUrlMatcherClassifier> url_match_classifier_;
  // Map from classifier category to its relevance value.
  base::flat_map<std::string, ContentClassifierRelevance>
      classifier_relevance_values_;
  // Set of supported language codes (e.g. "en", "en-US").
  base::flat_set<std::string> supported_languages_;
};

}  // namespace accessibility_annotator

#endif  // COMPONENTS_ACCESSIBILITY_ANNOTATOR_CONTENT_CONTENT_ANNOTATOR_CONTENT_CLASSIFIER_H_
