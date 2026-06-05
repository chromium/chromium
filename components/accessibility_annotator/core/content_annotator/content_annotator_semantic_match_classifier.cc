// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/accessibility_annotator/core/content_annotator/content_annotator_semantic_match_classifier.h"

#include <limits>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "base/functional/bind.h"
#include "components/accessibility_annotator/core/content_annotator/content_annotator_classifier_rules_parser.h"
#include "components/passage_embeddings/core/passage_embeddings_types.h"

namespace accessibility_annotator {

namespace {

std::vector<std::string> GetPassagesToEmbed(
    const SemanticMatchRulesMap& rules) {
  std::vector<std::string> passages;
  for (const auto& [category, category_keywords] : rules) {
    for (const std::string& keyword : category_keywords) {
      passages.push_back(keyword);
    }
  }
  return passages;
}

void OnEmbeddingsReady(SemanticMatchRulesMap rules,
                       SemanticMatchEmbeddingsCallback callback,
                       std::vector<std::string> passages,
                       std::vector<passage_embeddings::Embedding> embeddings,
                       uint64_t job_id,
                       passage_embeddings::ComputeEmbeddingsStatus status) {
  std::move(callback).Run(std::move(rules), std::move(embeddings), status);
}

}  // namespace

std::optional<passage_embeddings::Embedder::Job>
ComputeEmbeddingsForSemanticMatchClassifier(
    std::string_view rules_json,
    passage_embeddings::Embedder* embedder,
    SemanticMatchEmbeddingsCallback callback) {
  SemanticMatchRulesMap rules = ParseRulesFromJson(rules_json);
  if (rules.empty() || !embedder) {
    std::move(callback).Run(
        {}, {}, passage_embeddings::ComputeEmbeddingsStatus::kExecutionFailure);
    return std::nullopt;
  }

  std::vector<std::string> passages = GetPassagesToEmbed(rules);
  return embedder->ComputePassagesEmbeddings(
      passage_embeddings::PassagePriority::kUrgent, std::move(passages),
      base::BindOnce(&OnEmbeddingsReady, std::move(rules),
                     std::move(callback)));
}

// static
std::unique_ptr<ContentAnnotatorSemanticMatchClassifier>
ContentAnnotatorSemanticMatchClassifier::Create(
    SemanticMatchRulesMap rules,
    std::vector<passage_embeddings::Embedding> embeddings) {
  std::vector<CategoryEmbedding> category_embeddings;
  size_t embedding_idx = 0;
  for (const auto& [category, category_keywords] : rules) {
    for (size_t i = 0; i < category_keywords.size(); ++i) {
      if (embedding_idx >= embeddings.size()) {
        return nullptr;
      }
      category_embeddings.emplace_back(category,
                                       std::move(embeddings[embedding_idx++]));
    }
  }

  if (embedding_idx != embeddings.size() || category_embeddings.empty()) {
    return nullptr;
  }

  return std::make_unique<ContentAnnotatorSemanticMatchClassifier>(
      std::move(category_embeddings));
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
