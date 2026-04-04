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
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/scoped_observation.h"
#include "components/page_content_annotations/core/page_content_annotation_type.h"
#include "components/passage_embeddings/core/passage_embeddings_types.h"
#include "services/metrics/public/cpp/ukm_source_id.h"

namespace optimization_guide {
class OptimizationGuideModelProvider;
}  // namespace optimization_guide

class GURL;

namespace page_content_annotations {

enum class CategoryType;
class CategoryClassifierModelHandler;

// Manages the loading and execution of models used to classify the category
// represented by the text.
class OnDeviceCategoryClassifier
    : public passage_embeddings::EmbedderMetadataObserver {
 public:
  class Observer : public base::CheckedObserver {
   public:
    virtual void OnCategoriesClassified(
        const GURL& url,
        ukm::SourceId source_id,
        const std::vector<Category>& categories) = 0;
  };

  OnDeviceCategoryClassifier(
      optimization_guide::OptimizationGuideModelProvider*
          optimization_guide_model_provider,
      passage_embeddings::EmbedderMetadataProvider* embedder_metadata_provider);
  ~OnDeviceCategoryClassifier() override;
  OnDeviceCategoryClassifier(const OnDeviceCategoryClassifier&) = delete;
  OnDeviceCategoryClassifier& operator=(const OnDeviceCategoryClassifier&) =
      delete;

  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

  // Invoked when embeddings have been successfully computed for the page.
  void OnPageEmbeddingAvailable(
      const GURL& url,
      ukm::SourceId source_id,
      passage_embeddings::Embedding title_url_embedding,
      std::vector<passage_embeddings::Embedding> passage_embeddings);

  // passage_embeddings::EmbedderMetadataObserver:
  void EmbedderMetadataUpdated(
      passage_embeddings::EmbedderMetadata metadata) override;

 private:
  void OnCategoryClassifiersCompleted(
      const GURL& url,
      ukm::SourceId source_id,
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

  base::ObserverList<Observer> observers_;

  base::ScopedObservation<passage_embeddings::EmbedderMetadataProvider,
                          passage_embeddings::EmbedderMetadataObserver>
      embedder_metadata_observation_{this};

  base::WeakPtrFactory<OnDeviceCategoryClassifier> weak_ptr_factory_{this};
};

}  // namespace page_content_annotations

#endif  // COMPONENTS_PAGE_CONTENT_ANNOTATIONS_CORE_ON_DEVICE_CATEGORY_CLASSIFIER_H_
