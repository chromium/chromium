// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/page_content_annotations/core/on_device_category_classifier.h"

#include "base/barrier_callback.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "components/optimization_guide/core/delivery/optimization_guide_model_provider.h"
#include "components/page_content_annotations/core/edu_classifier_model_handler.h"
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
        optimization_guide_model_provider)
    : optimization_guide_model_provider_(optimization_guide_model_provider) {
  category_classifier_model_handlers_[CategoryType::kEducation] =
      std::make_unique<EduClassifierModelHandler>(
          optimization_guide_model_provider_,
          base::ThreadPool::CreateSequencedTaskRunner(
              {base::MayBlock(), base::TaskPriority::BEST_EFFORT}));
}

OnDeviceCategoryClassifier::~OnDeviceCategoryClassifier() = default;

void OnDeviceCategoryClassifier::OnPageEmbeddingAvailable(
    const GURL& url,
    const passage_embeddings::Embedding& embedding,
    base::OnceCallback<void(const std::vector<Category>&)> callback) {
  // Run each of the category classifiers on the returned embedding.
  auto barrier_callback =
      base::BarrierCallback<std::pair<CategoryType, std::optional<float>>>(
          category_classifier_model_handlers_.size(),
          base::BindOnce(
              &OnDeviceCategoryClassifier::OnCategoryClassifiersCompleted,
              weak_ptr_factory_.GetWeakPtr(), std::move(callback), url));
  for (const auto& [category_type, model_handler] :
       category_classifier_model_handlers_) {
    model_handler->ExecuteModelWithInput(
        base::BindOnce(&OnSingleCategoryClassifierComplete, category_type,
                       barrier_callback),
        embedding.GetData());
  }
}

void OnDeviceCategoryClassifier::OnCategoryClassifiersCompleted(
    base::OnceCallback<void(const std::vector<Category>&)> callback,
    const GURL& url,
    const std::vector<std::pair<CategoryType, std::optional<float>>>&
        classifier_outputs) {
  std::vector<Category> outputs;
  for (const auto& [category_type, maybe_score] : classifier_outputs) {
    if (maybe_score) {
      outputs.push_back(
          {.category_type = category_type, .score = *maybe_score});
    }
  }
  if (!callback.is_null()) {
    std::move(callback).Run(outputs);
  }
}

}  // namespace page_content_annotations
