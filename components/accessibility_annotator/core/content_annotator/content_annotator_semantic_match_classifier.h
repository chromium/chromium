// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ACCESSIBILITY_ANNOTATOR_CORE_CONTENT_ANNOTATOR_CONTENT_ANNOTATOR_SEMANTIC_MATCH_CLASSIFIER_H_
#define COMPONENTS_ACCESSIBILITY_ANNOTATOR_CORE_CONTENT_ANNOTATOR_CONTENT_ANNOTATOR_SEMANTIC_MATCH_CLASSIFIER_H_

#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "base/containers/flat_map.h"
#include "base/functional/callback_forward.h"
#include "components/passage_embeddings/core/passage_embeddings_types.h"

namespace passage_embeddings {
class Embedder;
}  // namespace passage_embeddings

namespace accessibility_annotator {

class ContentAnnotatorSemanticMatchClassifier;

// A map of categories to their associated keywords/phrases.
using SemanticMatchRulesMap =
    base::flat_map<std::string, std::vector<std::string>>;

using SemanticMatchEmbeddingsCallback = base::OnceCallback<void(
    SemanticMatchRulesMap rules,
    std::vector<passage_embeddings::Embedding> embeddings,
    passage_embeddings::ComputeEmbeddingsStatus status)>;

// Asynchronously computes embeddings for the keywords in `rules_json`.
// Matching is done using cosine similarity scores between the input text's
// embedding and the precomputed embeddings for each category.
// Calls the `callback` with the parsed rules and their corresponding
// embeddings. The embeddings vector corresponds to the passages extracted from
// the rules. Returns a TaskId for cancellation.
passage_embeddings::Embedder::TaskId
ComputeEmbeddingsForSemanticMatchClassifier(
    std::string_view rules_json,
    passage_embeddings::Embedder* embedder,
    SemanticMatchEmbeddingsCallback callback);

class ContentAnnotatorSemanticMatchClassifier {
 public:
  // Synchronously creates a semantic match classifier from precomputed
  // embeddings. The `embeddings` vector must correspond to the passages
  // extracted from the `rules`.
  static std::unique_ptr<ContentAnnotatorSemanticMatchClassifier> Create(
      SemanticMatchRulesMap rules,
      std::vector<passage_embeddings::Embedding> embeddings);

  // TODO(crbug.com/485267512): Move this to ContentClassifierResult.
  struct ClassificationResult {
    std::string category;
    double score;
  };

  struct CategoryEmbedding {
    std::string category;
    passage_embeddings::Embedding embedding;
  };

  explicit ContentAnnotatorSemanticMatchClassifier(
      std::vector<CategoryEmbedding> category_embeddings);
  ContentAnnotatorSemanticMatchClassifier(
      const ContentAnnotatorSemanticMatchClassifier&) = delete;
  ContentAnnotatorSemanticMatchClassifier& operator=(
      const ContentAnnotatorSemanticMatchClassifier&) = delete;
  ~ContentAnnotatorSemanticMatchClassifier();

  // Calculates the cosine similarity score between the input `embedding` and
  // the precomputed embeddings for each category. Returns the category and
  // score for the highest similarity match found. Returns std::nullopt if the
  // input is invalid or no categories are available.
  // TODO(crbug.com/482477208): Add support for multiple categories.
  std::optional<ClassificationResult> Classify(
      const passage_embeddings::Embedding& embedding) const;

 private:
  std::vector<CategoryEmbedding> category_embeddings_;
};

}  // namespace accessibility_annotator

#endif  // COMPONENTS_ACCESSIBILITY_ANNOTATOR_CORE_CONTENT_ANNOTATOR_CONTENT_ANNOTATOR_SEMANTIC_MATCH_CLASSIFIER_H_
