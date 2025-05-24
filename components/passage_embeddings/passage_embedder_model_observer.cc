// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/passage_embeddings/passage_embedder_model_observer.h"

#include "components/optimization_guide/core/optimization_guide_model_provider.h"
#include "components/passage_embeddings/passage_embeddings_service_controller.h"

namespace passage_embeddings {

PassageEmbedderModelObserver::PassageEmbedderModelObserver(
    optimization_guide::OptimizationGuideModelProvider* model_provider,
    PassageEmbeddingsServiceController* service_controller,
    bool experimental)
    : model_provider_(model_provider),
      service_controller_(service_controller),
      target_(experimental ? optimization_guide::proto::
                                 OPTIMIZATION_TARGET_EXPERIMENTAL_EMBEDDER
                           : optimization_guide::proto::
                                 OPTIMIZATION_TARGET_PASSAGE_EMBEDDER) {
  VLOG(3) << "Target: " << target_;
  if (model_provider_) {
    model_provider_->AddObserverForOptimizationTargetModel(
        target_,
        /*model_metadata=*/std::nullopt, this);
  }
}

PassageEmbedderModelObserver::~PassageEmbedderModelObserver() {
  if (model_provider_) {
    model_provider_->RemoveObserverForOptimizationTargetModel(target_, this);
  }
}

void PassageEmbedderModelObserver::OnModelUpdated(
    optimization_guide::proto::OptimizationTarget optimization_target,
    base::optional_ref<const optimization_guide::ModelInfo> model_info) {
  VLOG(3) << "Model updated for target: " << optimization_target;
  if (optimization_target != target_) {
    return;
  }

  service_controller_->MaybeUpdateModelInfo(model_info);
}

}  // namespace passage_embeddings
