// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PAGE_CONTENT_ANNOTATIONS_CORE_ON_DEVICE_CATEGORY_CLASSIFIER_H_
#define COMPONENTS_PAGE_CONTENT_ANNOTATIONS_CORE_ON_DEVICE_CATEGORY_CLASSIFIER_H_

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "base/containers/flat_map.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/scoped_observation.h"
#include "components/optimization_guide/core/inference/model_handler.h"
#include "components/passage_embeddings/passage_embeddings_types.h"

namespace optimization_guide {
class OptimizationGuideModelProvider;
}  // namespace optimization_guide

namespace page_content_annotations {

struct Category;
enum class CategoryType;

// Manages the loading and execution of models used to classify the category
// represented by the text.
class OnDeviceCategoryClassifier
    : public passage_embeddings::EmbedderMetadataObserver {
 public:
  OnDeviceCategoryClassifier(
      optimization_guide::OptimizationGuideModelProvider*
          optimization_guide_model_provider,
      passage_embeddings::EmbedderMetadataProvider* embedder_metadata_provider,
      passage_embeddings::Embedder* embedder);
  ~OnDeviceCategoryClassifier() override;
  OnDeviceCategoryClassifier(const OnDeviceCategoryClassifier&) = delete;
  OnDeviceCategoryClassifier& operator=(const OnDeviceCategoryClassifier&) =
      delete;

  // Classifies `text` and invokes `callback` with the classifications of all
  // supported categories when complete.
  //
  // Note that it is possible that this returns empty if the underlying models
  // are not available yet.
  void ClassifyText(const std::string& text,
                    base::OnceCallback<void(std::vector<Category>)> callback);

 private:
  // EmbedderMetadataObserver:
  void EmbedderMetadataUpdated(
      passage_embeddings::EmbedderMetadata metadata) override;

  // Callback invoked when the embedding has been computed.
  void OnEmbeddingComputed(
      base::OnceCallback<void(std::vector<Category>)> callback,
      std::vector<std::string> passages,
      std::vector<passage_embeddings::Embedding> embeddings,
      passage_embeddings::Embedder::TaskId task_id,
      passage_embeddings::ComputeEmbeddingsStatus status);

  // Callback invoked when all category classifiers have completed execution.
  void OnCategoryClassifiersCompleted(
      base::OnceCallback<void(std::vector<Category>)> callback,
      const std::vector<std::pair<CategoryType, std::optional<float>>>&
          classifier_outputs);

  // The model handlers for the category classifiers to run.
  base::flat_map<
      CategoryType,
      std::unique_ptr<
          optimization_guide::ModelHandler<float, const std::vector<float>&>>>
      category_classifier_model_handlers_;

  // Whether the embedder is currently available.
  bool is_embedder_available_ = false;

  // The model provider for category regression layers. Not owned.
  raw_ptr<optimization_guide::OptimizationGuideModelProvider>
      optimization_guide_model_provider_;
  // The provider of embedder metadata to determine whether `embedder_` is
  // currently available to submit jobs to. Not owned.
  raw_ptr<passage_embeddings::EmbedderMetadataProvider>
      embedder_metadata_provider_;
  // The text embedder to use. Not owned.
  raw_ptr<passage_embeddings::Embedder> embedder_;

  base::ScopedObservation<passage_embeddings::EmbedderMetadataProvider,
                          passage_embeddings::EmbedderMetadataObserver>
      scoped_observation_{this};

  base::WeakPtrFactory<OnDeviceCategoryClassifier> weak_ptr_factory_{this};
};

}  // namespace page_content_annotations

#endif  // COMPONENTS_PAGE_CONTENT_ANNOTATIONS_CORE_ON_DEVICE_CATEGORY_CLASSIFIER_H_
