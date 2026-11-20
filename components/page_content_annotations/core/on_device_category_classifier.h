// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PAGE_CONTENT_ANNOTATIONS_CORE_ON_DEVICE_CATEGORY_CLASSIFIER_H_
#define COMPONENTS_PAGE_CONTENT_ANNOTATIONS_CORE_ON_DEVICE_CATEGORY_CLASSIFIER_H_

#include <cstdint>
#include <memory>
#include <optional>
#include <vector>

#include "base/containers/flat_map.h"
#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "components/page_content_annotations/core/page_content_annotation_type.h"
#include "components/passage_embeddings/core/passage_embeddings_types.h"

namespace optimization_guide {
class OptimizationGuideModelProvider;
}  // namespace optimization_guide

namespace page_content_annotations {

enum class CategoryType;
class CategoryClassifierModelHandler;

// Manages the loading and execution of models used to classify the category
// represented by the text.
class OnDeviceCategoryClassifier
    : public passage_embeddings::EmbedderMetadataObserver {
 public:
  OnDeviceCategoryClassifier(
      optimization_guide::OptimizationGuideModelProvider*
          optimization_guide_model_provider,
      passage_embeddings::EmbedderMetadataProvider* embedder_metadata_provider);
  ~OnDeviceCategoryClassifier() override;
  OnDeviceCategoryClassifier(const OnDeviceCategoryClassifier&) = delete;
  OnDeviceCategoryClassifier& operator=(const OnDeviceCategoryClassifier&) =
      delete;

  // Classifies the categories represented by `title_url_embedding` and
  // `passage_embeddings`, invoking `callback` with the resulting category
  // scores.
  void ClassifyTextEmbeddings(
      std::optional<passage_embeddings::Embedding> title_url_embedding,
      std::vector<passage_embeddings::Embedding> passage_embeddings,
      base::OnceCallback<void(std::vector<Category>)> callback);

  // passage_embeddings::EmbedderMetadataObserver:
  void EmbedderMetadataUpdated(
      passage_embeddings::EmbedderMetadata metadata) override;

 private:
  void OnCategoryClassifiersCompleted(
      base::OnceCallback<void(std::vector<Category>)> callback,
      const std::vector<std::pair<CategoryType, std::optional<float>>>&
          classifier_outputs);

  // The model handlers for the category classifiers to run.
  base::flat_map<CategoryType, std::unique_ptr<CategoryClassifierModelHandler>>
      category_classifier_model_handlers_;

  // The model provider for category regression layers. Not owned.
  raw_ptr<optimization_guide::OptimizationGuideModelProvider>
      optimization_guide_model_provider_;

  // The current version of the embedder model.
  std::optional<int64_t> embedder_version_;

  base::ScopedObservation<passage_embeddings::EmbedderMetadataProvider,
                          passage_embeddings::EmbedderMetadataObserver>
      embedder_metadata_observation_{this};

  base::WeakPtrFactory<OnDeviceCategoryClassifier> weak_ptr_factory_{this};
};

}  // namespace page_content_annotations

#endif  // COMPONENTS_PAGE_CONTENT_ANNOTATIONS_CORE_ON_DEVICE_CATEGORY_CLASSIFIER_H_
