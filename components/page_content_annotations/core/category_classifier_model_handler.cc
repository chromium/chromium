// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/page_content_annotations/core/category_classifier_model_handler.h"

#include <algorithm>
#include <iterator>
#include <optional>

#include "base/containers/extend.h"
#include "base/memory/scoped_refptr.h"
#include "base/task/sequenced_task_runner.h"
#include "components/optimization_guide/core/delivery/optimization_guide_model_provider.h"
#include "components/optimization_guide/proto/category_classifier_metadata.pb.h"
#include "components/optimization_guide/proto/models.pb.h"
#include "components/page_content_annotations/core/category_classifier_model_executor.h"
#include "components/passage_embeddings/core/passage_embeddings_types.h"

namespace page_content_annotations {

namespace {

std::vector<float> GetMeanPooledVector(
    const std::vector<passage_embeddings::Embedding>& entries) {
  CHECK(!entries.empty());

  std::vector<float> pooled_vector(entries[0].GetData().size(), 0.0f);
  for (const auto& entry : entries) {
    DCHECK_EQ(entry.GetData().size(), pooled_vector.size());
    for (size_t i = 0; i < entry.GetData().size(); ++i) {
      pooled_vector[i] += entry.GetData()[i];
    }
  }
  for (float& value : pooled_vector) {
    value /= static_cast<float>(entries.size());
  }
  return pooled_vector;
}

std::vector<float> GetMaxPooledVector(
    const std::vector<passage_embeddings::Embedding>& entries) {
  CHECK(!entries.empty());

  std::vector<float> pooled_vector(entries[0].GetData().size(), 0.0f);
  for (const auto& entry : entries) {
    DCHECK_EQ(entry.GetData().size(), pooled_vector.size());
    for (size_t i = 0; i < entry.GetData().size(); ++i) {
      pooled_vector[i] = std::max(pooled_vector[i], entry.GetData()[i]);
    }
  }
  return pooled_vector;
}

}  // namespace

CategoryClassifierModelHandler::CategoryClassifierModelHandler(
    optimization_guide::proto::OptimizationTarget optimization_target,
    optimization_guide::OptimizationGuideModelProvider* model_provider,
    scoped_refptr<base::SequencedTaskRunner> model_executor_task_runner)
    : CategoryClassifierModelHandler(
          model_provider,
          model_executor_task_runner,
          std::make_unique<CategoryClassifierModelExecutor>(),
          /*model_inference_timeout=*/std::nullopt,
          optimization_target,
          std::nullopt) {}

CategoryClassifierModelHandler::~CategoryClassifierModelHandler() = default;

std::optional<int64_t>
CategoryClassifierModelHandler::GetRequiredEmbedderVersion() const {
  auto metadata = ParsedSupportedFeaturesForLoadedModel<
      optimization_guide::proto::CategoryClassifierMetadata>();
  if (metadata && metadata->has_required_embedder_version()) {
    return metadata->required_embedder_version();
  }
  return std::nullopt;
}

std::vector<float> CategoryClassifierModelHandler::ConstructInputVector(
    passage_embeddings::Embedding title_url_embedding,
    std::vector<passage_embeddings::Embedding> passage_embeddings) const {
  auto metadata = ParsedSupportedFeaturesForLoadedModel<
      optimization_guide::proto::CategoryClassifierMetadata>();
  auto concatenation_strategy =
      metadata && metadata->has_passage_embedding_concatenation_strategy()
          ? std::make_optional(
                metadata->passage_embedding_concatenation_strategy())
          : std::nullopt;
  if (!concatenation_strategy || concatenation_strategy->max_passages() <= 0 ||
      concatenation_strategy->pooling_strategy() ==
          optimization_guide::proto::
              CategoryClassifierPassageEmbeddingConcatenationStrategy::
                  POOLING_STRATEGY_UNKNOWN) {
    return title_url_embedding.GetData();
  }

  // Construct pooled embedding.
  size_t embedding_size = title_url_embedding.GetData().size();
  std::vector<float> result_vector = title_url_embedding.GetData();
  std::vector<passage_embeddings::Embedding> passage_embedding_data;
  for (size_t i = 0;
       i < passage_embeddings.size() &&
       i < static_cast<size_t>(concatenation_strategy->max_passages());
       ++i) {
    DCHECK_EQ(passage_embeddings[i].GetData().size(), embedding_size);
    passage_embedding_data.push_back(std::move(passage_embeddings[i]));
  }
  bool needs_max = false;
  bool needs_mean = false;
  switch (concatenation_strategy->pooling_strategy()) {
    case optimization_guide::proto::
        CategoryClassifierPassageEmbeddingConcatenationStrategy::
            POOLING_STRATEGY_MAX: {
      needs_max = true;
      break;
    }
    case optimization_guide::proto::
        CategoryClassifierPassageEmbeddingConcatenationStrategy::
            POOLING_STRATEGY_MEAN: {
      needs_mean = true;
      break;
    }
    case optimization_guide::proto::
        CategoryClassifierPassageEmbeddingConcatenationStrategy::
            POOLING_STRATEGY_MEAN_MAX: {
      needs_mean = true;
      needs_max = true;
      break;
    }
    default:
      return result_vector;
  }

  // The order of mean and max pooling does matter for the mean_max pooling
  // strategy. Make sure that there is no max mean in the pooling strategy.
  // Otherwise, bad times are to be had.
  if (needs_mean) {
    std::vector<float> mean_pooled_embedding =
        passage_embedding_data.empty()
            ? std::vector<float>(embedding_size, 0.0f)
            : GetMeanPooledVector(passage_embedding_data);
    base::Extend(result_vector, mean_pooled_embedding);
  }
  if (needs_max) {
    std::vector<float> max_pooled_embedding =
        passage_embedding_data.empty()
            ? std::vector<float>(embedding_size, 0.0f)
            : GetMaxPooledVector(passage_embedding_data);
    base::Extend(result_vector, max_pooled_embedding);
  }
  return result_vector;
}

}  // namespace page_content_annotations
