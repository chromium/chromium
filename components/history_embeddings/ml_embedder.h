// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_HISTORY_EMBEDDINGS_ML_EMBEDDER_H_
#define COMPONENTS_HISTORY_EMBEDDINGS_ML_EMBEDDER_H_

#include "base/memory/raw_ptr.h"
#include "components/history_embeddings/embedder.h"
#include "components/optimization_guide/core/optimization_target_model_observer.h"

namespace optimization_guide {
class OptimizationGuideModelProvider;
}  // namespace optimization_guide

namespace passage_embeddings {
class PassageEmbeddingsServiceController;
}  // namespace passage_embeddings

namespace history_embeddings {

// An embedder that returns embeddings from a machine learning model.
class MlEmbedder : public Embedder,
                   public optimization_guide::OptimizationTargetModelObserver {
 public:
  MlEmbedder(optimization_guide::OptimizationGuideModelProvider* model_provider,
             passage_embeddings::PassageEmbeddingsServiceController*
                 service_controller);
  ~MlEmbedder() override;

  // Embedder:
  void ComputePassagesEmbeddings(
      PassageKind kind,
      std::vector<std::string> passages,
      ComputePassagesEmbeddingsCallback callback) override;

  void SetOnEmbedderReady(OnEmbedderReadyCallback callback) override;

 private:
  // OptimizationTargetModelObserver:
  void OnModelUpdated(
      optimization_guide::proto::OptimizationTarget optimization_target,
      base::optional_ref<const optimization_guide::ModelInfo> model_info)
      override;

  // The provider of the embeddings model. It may be nullptr if
  // `optimization_guide::kOptimizationHints` feature is disabled. Otherwise, it
  // is guaranteed to outlive `this` since HistoryEmbeddingsServiceFactory
  // depends on OptimizationGuideKeyedServiceFactory.
  raw_ptr<optimization_guide::OptimizationGuideModelProvider> model_provider_;

  // The controller used to interact with the PassageEmbeddingsService.
  // It is a singleton and guaranteed not to be nullptr.
  raw_ptr<passage_embeddings::PassageEmbeddingsServiceController>
      service_controller_;

  // Called once the embedder is ready.
  OnEmbedderReadyCallback on_embedder_ready_;
};

}  // namespace history_embeddings

#endif  // COMPONENTS_HISTORY_EMBEDDINGS_ML_EMBEDDER_H_
