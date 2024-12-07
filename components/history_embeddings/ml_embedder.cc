// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/history_embeddings/ml_embedder.h"

#include "base/task/sequenced_task_runner.h"
#include "components/history_embeddings/vector_database.h"
#include "components/optimization_guide/core/optimization_guide_model_provider.h"
#include "components/passage_embeddings/passage_embeddings_service_controller.h"
#include "services/passage_embeddings/public/mojom/passage_embeddings.mojom.h"

namespace history_embeddings {

MlEmbedder::MlEmbedder(
    optimization_guide::OptimizationGuideModelProvider* model_provider,
    passage_embeddings::PassageEmbeddingsServiceController* service_controller)
    : model_provider_(model_provider), service_controller_(service_controller) {
  if (model_provider_) {
    model_provider_->AddObserverForOptimizationTargetModel(
        optimization_guide::proto::OPTIMIZATION_TARGET_PASSAGE_EMBEDDER,
        /*model_metadata=*/std::nullopt, this);
  }
}

MlEmbedder::~MlEmbedder() {
  if (model_provider_) {
    model_provider_->RemoveObserverForOptimizationTargetModel(
        optimization_guide::proto::OPTIMIZATION_TARGET_PASSAGE_EMBEDDER, this);
  }
}

void MlEmbedder::ComputePassagesEmbeddings(
    PassageKind kind,
    std::vector<std::string> passages,
    ComputePassagesEmbeddingsCallback callback) {
  passage_embeddings::mojom::PassagePriority priority =
      kind == PassageKind::QUERY
          ? passage_embeddings::mojom::PassagePriority::kUserInitiated
          : passage_embeddings::mojom::PassagePriority::kPassive;

  service_controller_->GetEmbeddings(
      std::move(passages), priority,
      base::BindOnce(
          [](ComputePassagesEmbeddingsCallback callback,
             std::vector<passage_embeddings::mojom::PassageEmbeddingsResultPtr>
                 results,
             passage_embeddings::ComputeEmbeddingsStatus status) {
            std::vector<std::string> result_passages;
            std::vector<Embedding> result_embeddings;
            for (auto& result : results) {
              result_passages.push_back(result->passage);
              result_embeddings.emplace_back(result->embeddings);
              result_embeddings.back().Normalize();
            }
            std::move(callback).Run(std::move(result_passages),
                                    std::move(result_embeddings), status);
          },
          std::move(callback)));
}

void MlEmbedder::OnModelUpdated(
    optimization_guide::proto::OptimizationTarget optimization_target,
    base::optional_ref<const optimization_guide::ModelInfo> model_info) {
  if (optimization_target !=
      optimization_guide::proto::OPTIMIZATION_TARGET_PASSAGE_EMBEDDER) {
    return;
  }

  if (!service_controller_->MaybeUpdateModelInfo(model_info)) {
    return;
  }

  if (on_embedder_ready_) {
    std::move(on_embedder_ready_)
        .Run(service_controller_->GetEmbedderMetadata());
  }
}

void MlEmbedder::SetOnEmbedderReady(OnEmbedderReadyCallback callback) {
  if (service_controller_->EmbedderReady()) {
    std::move(callback).Run(service_controller_->GetEmbedderMetadata());
  } else {
    on_embedder_ready_ = std::move(callback);
  }
}

}  // namespace history_embeddings
