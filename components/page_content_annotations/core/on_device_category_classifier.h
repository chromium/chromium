// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PAGE_CONTENT_ANNOTATIONS_CORE_ON_DEVICE_CATEGORY_CLASSIFIER_H_
#define COMPONENTS_PAGE_CONTENT_ANNOTATIONS_CORE_ON_DEVICE_CATEGORY_CLASSIFIER_H_

#include <memory>
#include <optional>
#include <vector>

#include "base/containers/flat_map.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "components/optimization_guide/core/inference/model_handler.h"
#include "components/page_content_annotations/core/page_content_annotation_type.h"
#include "components/passage_embeddings/core/passage_embeddings_types.h"

namespace optimization_guide {
class OptimizationGuideModelProvider;
}  // namespace optimization_guide

class GURL;

namespace page_content_annotations {

enum class CategoryType;

// Manages the loading and execution of models used to classify the category
// represented by the text.
class OnDeviceCategoryClassifier {
 public:
  explicit OnDeviceCategoryClassifier(
      optimization_guide::OptimizationGuideModelProvider*
          optimization_guide_model_provider);
  ~OnDeviceCategoryClassifier();
  OnDeviceCategoryClassifier(const OnDeviceCategoryClassifier&) = delete;
  OnDeviceCategoryClassifier& operator=(const OnDeviceCategoryClassifier&) =
      delete;

  // Invoked when an embedding has been successfully computed for the page.
  void OnPageEmbeddingAvailable(
      const GURL& url,
      const passage_embeddings::Embedding& embedding,
      base::OnceCallback<void(const std::vector<Category>&)> callback);

 private:
  // Callback invoked when all category classifiers have completed execution.
  void OnCategoryClassifiersCompleted(
      base::OnceCallback<void(const std::vector<Category>&)> callback,
      const GURL& url,
      const std::vector<std::pair<CategoryType, std::optional<float>>>&
          classifier_outputs);

  // The model handlers for the category classifiers to run.
  base::flat_map<
      CategoryType,
      std::unique_ptr<
          optimization_guide::ModelHandler<float, const std::vector<float>&>>>
      category_classifier_model_handlers_;

  // The model provider for category regression layers. Not owned.
  raw_ptr<optimization_guide::OptimizationGuideModelProvider>
      optimization_guide_model_provider_;

  base::WeakPtrFactory<OnDeviceCategoryClassifier> weak_ptr_factory_{this};
};

}  // namespace page_content_annotations

#endif  // COMPONENTS_PAGE_CONTENT_ANNOTATIONS_CORE_ON_DEVICE_CATEGORY_CLASSIFIER_H_
