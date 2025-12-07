// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSAGE_EMBEDDINGS_PASSAGE_EMBEDDER_MODEL_OBSERVER_H_
#define COMPONENTS_PASSAGE_EMBEDDINGS_PASSAGE_EMBEDDER_MODEL_OBSERVER_H_

#include "base/memory/raw_ptr.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/optimization_guide/core/delivery/optimization_target_model_observer.h"

namespace optimization_guide {
class OptimizationGuideModelProvider;
}  // namespace optimization_guide

namespace passage_embeddings {

class PassageEmbeddingsServiceController;

// Observes updates to the embedder models and notifies
// `PassageEmbeddingsServiceController` to update the models.
class PassageEmbedderModelObserver
    : public KeyedService,
      public optimization_guide::OptimizationTargetModelObserver {
 public:
  // `model_provider` may be nullptr. If provided, it is guaranteed to
  // outlive `this` since EmbedderServiceFactory depends on
  // OptimizationGuideKeyedServiceFactory. `service_controller` is a singleton
  // and never nullptr.
  PassageEmbedderModelObserver(
      optimization_guide::OptimizationGuideModelProvider* model_provider,
      PassageEmbeddingsServiceController* service_controller);
  ~PassageEmbedderModelObserver() override;

 private:
  // OptimizationTargetModelObserver:
  void OnModelUpdated(
      optimization_guide::proto::OptimizationTarget optimization_target,
      base::optional_ref<const optimization_guide::ModelInfo> model_info)
      override;

  // The provider of the embeddings model.
  // May be nullptr. Otherwise it is guaranteed to outlive `this`.
  raw_ptr<optimization_guide::OptimizationGuideModelProvider> model_provider_;

  // The controller used to interact with the PassageEmbeddingsService.
  // It is a singleton and guaranteed not to be nullptr and to outlive `this`.
  raw_ptr<PassageEmbeddingsServiceController> service_controller_;

  // The model target being observed; may be experimental.
  optimization_guide::proto::OptimizationTarget target_;
};

}  // namespace passage_embeddings

#endif  // COMPONENTS_PASSAGE_EMBEDDINGS_PASSAGE_EMBEDDER_MODEL_OBSERVER_H_
