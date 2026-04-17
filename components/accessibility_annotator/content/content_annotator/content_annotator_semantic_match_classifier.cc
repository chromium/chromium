// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/accessibility_annotator/content/content_annotator/content_annotator_semantic_match_classifier.h"

#include <limits>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "base/functional/bind.h"
#include "components/accessibility_annotator/content/content_annotator/content_annotator_classifier_rules_parser.h"
#include "components/passage_embeddings/core/passage_embeddings_types.h"

namespace accessibility_annotator {

namespace {

void OnEmbeddingsReady(std::vector<std::string> categories,
                       ClassifierCallback callback,
                       std::vector<std::string> passages,
                       std::vector<passage_embeddings::Embedding> embeddings,
                       passage_embeddings::Embedder::TaskId task_id,
                       passage_embeddings::ComputeEmbeddingsStatus status) {
  if (status != passage_embeddings::ComputeEmbeddingsStatus::kSuccess ||
      embeddings.size() != categories.size()) {
    std::move(callback).Run(nullptr);
    return;
  }

  std::vector<ContentAnnotatorSemanticMatchClassifier::CategoryEmbedding>
      category_embeddings;
  category_embeddings.reserve(categories.size());
  for (size_t i = 0; i < categories.size(); ++i) {
    category_embeddings.emplace_back(std::move(categories[i]),
                                     std::move(embeddings[i]));
  }

  if (category_embeddings.empty()) {
    std::move(callback).Run(nullptr);
    return;
  }

  std::move(callback).Run(
      std::make_unique<ContentAnnotatorSemanticMatchClassifier>(
          std::move(category_embeddings)));
}

}  // namespace

void CreateSemanticMatchClassifier(std::string_view rules_json,
                                   passage_embeddings::Embedder* embedder,
                                   ClassifierCallback callback) {
  // 1. Parse rules.
  base::flat_map<std::string, std::vector<std::string>> rules =
      ParseRulesFromJson(rules_json);
  if (rules.empty() || !embedder) {
    std::move(callback).Run(nullptr);
    return;
  }

  // 2. Prepare passages for embedding.
  std::vector<std::string> categories;
  std::vector<std::string> keywords;
  categories.reserve(rules.size());
  keywords.reserve(rules.size());
  for (const auto& [category, category_keywords] : rules) {
    for (const std::string& keyword : category_keywords) {
      categories.push_back(category);
      keywords.push_back(keyword);
    }
  }

  // 3. Compute embeddings asynchronously.
  embedder->ComputePassagesEmbeddings(
      passage_embeddings::PassagePriority::kUrgent, std::move(keywords),
      base::BindOnce(&OnEmbeddingsReady, std::move(categories),
                     std::move(callback)));
}

ContentAnnotatorSemanticMatchClassifier::
    ContentAnnotatorSemanticMatchClassifier(
        std::vector<CategoryEmbedding> category_embeddings)
    : category_embeddings_(std::move(category_embeddings)) {}

ContentAnnotatorSemanticMatchClassifier::
    ~ContentAnnotatorSemanticMatchClassifier() = default;

std::optional<ContentAnnotatorSemanticMatchClassifier::ClassificationResult>
ContentAnnotatorSemanticMatchClassifier::Classify(
    const passage_embeddings::Embedding& embedding) const {
  if (category_embeddings_.empty()) {
    return std::nullopt;
  }

  // Ensure the input embedding has a non-zero magnitude.
  if (embedding.Magnitude() < std::numeric_limits<float>::epsilon()) {
    return std::nullopt;
  }

  std::string_view best_category;
  double best_similarity = -1.0;

  // TODO(crbug.com/491892322): Consider other algorithms for finding the best
  // match.
  for (const CategoryEmbedding& entry : category_embeddings_) {
    double similarity = embedding.ScoreWith(entry.embedding);
    if (similarity > best_similarity) {
      best_similarity = similarity;
      best_category = entry.category;
    }
  }

  if (!best_category.empty() && best_similarity > 0.5) {
    return ClassificationResult{std::string(best_category), best_similarity};
  }

  return std::nullopt;
}

}  // namespace accessibility_annotator
