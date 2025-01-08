// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/passage_embeddings/passage_embedder_model_observer.h"

#include "components/optimization_guide/core/optimization_guide_model_provider.h"
#include "components/passage_embeddings/passage_embeddings_service_controller.h"

namespace passage_embeddings {

PassageEmbedderModelObserver::PassageEmbedderModelObserver(
    optimization_guide::OptimizationGuideModelProvider* model_provider,
    PassageEmbeddingsServiceController* service_controller)
    : model_provider_(model_provider), service_controller_(service_controller) {
  if (model_provider_) {
    model_provider_->AddObserverForOptimizationTargetModel(
        optimization_guide::proto::OPTIMIZATION_TARGET_EXPERIMENTAL_EMBEDDER,
        /*model_metadata=*/std::nullopt, this);
  }
}

PassageEmbedderModelObserver::~PassageEmbedderModelObserver() {
  if (model_provider_) {
    model_provider_->RemoveObserverForOptimizationTargetModel(
        optimization_guide::proto::OPTIMIZATION_TARGET_EXPERIMENTAL_EMBEDDER,
        this);
  }
}

void PassageEmbedderModelObserver::OnModelUpdated(
    optimization_guide::proto::OptimizationTarget optimization_target,
    base::optional_ref<const optimization_guide::ModelInfo> model_info) {
  if (optimization_target !=
      optimization_guide::proto::OPTIMIZATION_TARGET_EXPERIMENTAL_EMBEDDER) {
    return;
  }

  service_controller_->MaybeUpdateModelInfo(model_info);
}

}  // namespace passage_embeddings
