// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ACCESSIBILITY_ANNOTATOR_CORE_CONTENT_ANNOTATOR_CONTENT_CLASSIFIER_H_
#define COMPONENTS_ACCESSIBILITY_ANNOTATOR_CORE_CONTENT_ANNOTATOR_CONTENT_CLASSIFIER_H_

#include <memory>
#include <optional>
#include <string>
#include <string_view>

#include "base/containers/flat_map.h"
#include "base/containers/flat_set.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "base/types/pass_key.h"
#include "components/accessibility_annotator/core/content_annotator/content_annotator_rule_based_classifier.h"
#include "components/accessibility_annotator/core/content_annotator/content_annotator_semantic_match_classifier.h"
#include "components/accessibility_annotator/core/content_annotator/content_annotator_url_matcher_classifier.h"
#include "components/accessibility_annotator/core/content_annotator/content_classifier_types.h"
#include "url/gurl.h"

namespace ukm::builders {
class AccessibilityAnnotator_ContentAnnotator_ClassifierResults;
}  // namespace ukm::builders

namespace passage_embeddings {
class Embedder;
}  // namespace passage_embeddings

namespace accessibility_annotator {

enum class ContentClassifierRelevance;

// This class encapsulates the logic for content classification. It is designed
// to be extensible with multiple individual classifiers.
class ContentClassifier {
 public:
  // LINT.IfChange
  enum class ClassifierResultStatus {
    kInconclusiveNoMatch,
    kDidNotRunMissingClassifier,
    kDidNotRunEmptyPageTitle,
    kDidNotRunInvalidUrl,
    kDidNotRunMissingClassifierEmptyPageTitle,
    kDidNotRunMissingClassifierInvalidUrl,
    kDidNotRunMissingPageTitleEmbedding,
    kDidNotRunMissingClassifierMissingPageTitleEmbedding,
  };
  // LINT.ThenChange(//tools/metrics/histograms/metadata/accessibility_annotator/enums.xml:AccessibilityAnnotatorClassifierResult)

  static std::string_view ClassifierResultStatusToString(
      ClassifierResultStatus status);

  using PassKey = base::PassKey<ContentClassifier>;

  // Creates a ContentClassifier. Sub-classifiers are initialized synchronously,
  // except for the semantic classifier which may be initialized asynchronously
  // if an embedder is available.
  // Returns nullptr if unparsable rules are provided for all sub-classifiers.
  static std::unique_ptr<ContentClassifier> Create(
      passage_embeddings::Embedder* embedder);

  explicit ContentClassifier(
      PassKey pass_key,
      passage_embeddings::Embedder* embedder,
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

  // Runs all classifiers on the given input and returns the result.
  // Classifiers are run only if the input is complete and the classifier
  // supports the input data.
  virtual ContentClassificationResult Classify(
      const ContentClassificationInput& input) const;

  // Triggers the initialization of the semantic classifier if the
  // embedder model has changed.
  void OnEmbedderModelChanged();

  // Returns true if the semantic classifier has been initialized and is ready
  // to classify.
  bool IsSemanticClassifierReadyForTesting() const;

 protected:
  // For testing.
  ContentClassifier();

 private:
  // Resolves the relevance classifier value for UKM logging.
  int64_t GetRelevanceAsInt(std::optional<std::string_view> category) const;

  // Called when the semantic classifier is created.
  void OnSemanticClassifierCreated(
      std::unique_ptr<ContentAnnotatorSemanticMatchClassifier> classifier);

  // Runs the title keyword classifier.
  void RunTitleKeywordClassifier(
      const ContentClassificationInput& input,
      ContentClassificationResult& result,
      ukm::builders::AccessibilityAnnotator_ContentAnnotator_ClassifierResults&
          ukm_builder) const;

  // Runs the URL match classifier.
  void RunUrlMatchClassifier(
      const ContentClassificationInput& input,
      ContentClassificationResult& result,
      ukm::builders::AccessibilityAnnotator_ContentAnnotator_ClassifierResults&
          ukm_builder) const;

  // Runs the semantic match classifier.
  void RunSemanticMatchClassifier(
      const ContentClassificationInput& input,
      ContentClassificationResult& result,
      ukm::builders::AccessibilityAnnotator_ContentAnnotator_ClassifierResults&
          ukm_builder) const;

  // The embedder for computing semantic embeddings.
  raw_ptr<passage_embeddings::Embedder> embedder_;
  // The classifier for matching keywords in the page title.
  std::unique_ptr<ContentAnnotatorRuleBasedClassifier>
      title_keyword_classifier_;
  // The classifier for matching URLs.
  std::unique_ptr<ContentAnnotatorUrlMatcherClassifier> url_match_classifier_;
  // The classifier for semantic matching.
  std::unique_ptr<ContentAnnotatorSemanticMatchClassifier> semantic_classifier_;
  // Map from classifier category to its relevance value.
  base::flat_map<std::string, ContentClassifierRelevance>
      classifier_relevance_values_;
  // Set of supported language codes (e.g. "en", "en-US").
  base::flat_set<std::string> supported_languages_;

  base::WeakPtrFactory<ContentClassifier> weak_ptr_factory_{this};
};

}  // namespace accessibility_annotator

#endif  // COMPONENTS_ACCESSIBILITY_ANNOTATOR_CORE_CONTENT_ANNOTATOR_CONTENT_CLASSIFIER_H_
