// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ACCESSIBILITY_ANNOTATOR_CONTENT_CONTENT_ANNOTATOR_CONTENT_CLASSIFIER_H_
#define COMPONENTS_ACCESSIBILITY_ANNOTATOR_CONTENT_CONTENT_ANNOTATOR_CONTENT_CLASSIFIER_H_

#include <memory>
#include <optional>
#include <string>

#include "base/time/time.h"
#include "base/types/pass_key.h"
#include "components/accessibility_annotator/content/content_annotator/content_annotator_rule_based_classifier.h"
#include "components/accessibility_annotator/content/content_annotator/content_annotator_url_matcher_classifier.h"
#include "components/accessibility_annotator/content/content_annotator/content_classifier_types.h"
#include "url/gurl.h"

namespace accessibility_annotator {

// This class encapsulates the logic for content classification. It is designed
// to be extensible with multiple individual classifiers.
class ContentClassifier {
 public:
  using PassKey = base::PassKey<ContentClassifier>;

  // Creates a ContentClassifier, fully initialized with all sub-classifiers.
  // Returns nullptr if unparsable rules are provided for all sub-classifiers.
  static std::unique_ptr<ContentClassifier> Create();

  explicit ContentClassifier(
      PassKey pass_key,
      std::unique_ptr<ContentAnnotatorRuleBasedClassifier>
          title_keyword_classifier,
      std::unique_ptr<ContentAnnotatorUrlMatcherClassifier>
          url_match_classifier);

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
  // The classifier for matching keywords in the page title.
  std::unique_ptr<ContentAnnotatorRuleBasedClassifier>
      title_keyword_classifier_;
  // The classifier for matching URLs.
  std::unique_ptr<ContentAnnotatorUrlMatcherClassifier> url_match_classifier_;
};

}  // namespace accessibility_annotator

#endif  // COMPONENTS_ACCESSIBILITY_ANNOTATOR_CONTENT_CONTENT_ANNOTATOR_CONTENT_CLASSIFIER_H_
