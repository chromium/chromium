// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/page_content_annotations/core/on_device_category_classifier.h"

#include <memory>
#include <vector>

#include "base/barrier_callback.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "components/optimization_guide/core/delivery/optimization_guide_model_provider.h"
#include "components/page_content_annotations/core/category_classifier_model_handler.h"
#include "components/page_content_annotations/core/page_content_annotation_type.h"

namespace page_content_annotations {

namespace {

// Callback invoked when a single category classifier has completed.
void OnSingleCategoryClassifierComplete(
    CategoryType category_type,
    base::OnceCallback<void(std::pair<CategoryType, std::optional<float>>)>
        callback,
    const std::optional<float>& maybe_score) {
  std::move(callback).Run(std::make_pair(category_type, maybe_score));
}

}  // namespace

OnDeviceCategoryClassifier::OnDeviceCategoryClassifier(
    optimization_guide::OptimizationGuideModelProvider*
        optimization_guide_model_provider,
    passage_embeddings::EmbedderMetadataProvider* embedder_metadata_provider)
    : optimization_guide_model_provider_(optimization_guide_model_provider) {
  if (embedder_metadata_provider) {
    embedder_metadata_observation_.Observe(embedder_metadata_provider);
  }
  category_classifier_model_handlers_[CategoryType::kEducation] =
      std::make_unique<CategoryClassifierModelHandler>(
          optimization_guide::proto::OPTIMIZATION_TARGET_EDU_CLASSIFIER,
          optimization_guide_model_provider_,
          base::ThreadPool::CreateSequencedTaskRunner(
              {base::MayBlock(), base::TaskPriority::BEST_EFFORT}));
  category_classifier_model_handlers_[CategoryType::kShopping] =
      std::make_unique<CategoryClassifierModelHandler>(
          optimization_guide::proto::OPTIMIZATION_TARGET_SHOPPING_CLASSIFIER,
          optimization_guide_model_provider_,
          base::ThreadPool::CreateSequencedTaskRunner(
              {base::MayBlock(), base::TaskPriority::BEST_EFFORT}));
}

OnDeviceCategoryClassifier::~OnDeviceCategoryClassifier() = default;

void OnDeviceCategoryClassifier::ClassifyTextEmbeddings(
    std::optional<passage_embeddings::Embedding> title_url_embedding,
    std::vector<passage_embeddings::Embedding> passage_embeddings,
    base::OnceCallback<void(std::vector<Category>)> callback) {
  if (!title_url_embedding.has_value()) {
    // Skip if the title-URL embedding is empty.
    std::move(callback).Run({});
    return;
  }

  // Run each of the category classifiers on the returned embedding.
  auto barrier_callback =
      base::BarrierCallback<std::pair<CategoryType, std::optional<float>>>(
          category_classifier_model_handlers_.size(),
          base::BindOnce(
              &OnDeviceCategoryClassifier::OnCategoryClassifiersCompleted,
              weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
  for (const auto& [category_type, model_handler] :
       category_classifier_model_handlers_) {
    // Check if the model is compatible with the current embedder version.
    std::optional<int64_t> required_version =
        model_handler->GetRequiredEmbedderVersion();

    if (!embedder_version_ || !required_version ||
        *required_version != *embedder_version_) {
      // Embedder version is unknown, versions mismatch, or required version is
      // missing. Skip this classifier.
      OnSingleCategoryClassifierComplete(category_type, barrier_callback,
                                         std::nullopt);
      continue;
    }

    // Execute the model with the input vector.
    std::vector<float> input_vector = model_handler->ConstructInputVector(
        *title_url_embedding, passage_embeddings);
    model_handler->ExecuteModelWithInput(
        base::BindOnce(&OnSingleCategoryClassifierComplete, category_type,
                       barrier_callback),
        input_vector);
  }
}

void OnDeviceCategoryClassifier::EmbedderMetadataUpdated(
    passage_embeddings::EmbedderMetadata metadata) {
  embedder_version_ = metadata.model_version;
}

void OnDeviceCategoryClassifier::OnCategoryClassifiersCompleted(
    base::OnceCallback<void(std::vector<Category>)> callback,
    const std::vector<std::pair<CategoryType, std::optional<float>>>&
        classifier_outputs) {
  std::vector<Category> outputs;
  for (const auto& [category_type, maybe_score] : classifier_outputs) {
    if (maybe_score) {
      outputs.push_back(
          {.category_type = category_type, .score = *maybe_score});
    }
  }

  std::move(callback).Run(std::move(outputs));
}

}  // namespace page_content_annotations
