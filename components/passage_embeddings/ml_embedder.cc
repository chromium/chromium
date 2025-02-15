// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/passage_embeddings/ml_embedder.h"

#include "base/task/sequenced_task_runner.h"
#include "components/passage_embeddings/passage_embeddings_service_controller.h"
#include "services/passage_embeddings/public/mojom/passage_embeddings.mojom.h"

namespace passage_embeddings {

MlEmbedder::MlEmbedder(PassageEmbeddingsServiceController* service_controller)
    : service_controller_(service_controller) {}

MlEmbedder::~MlEmbedder() = default;

Embedder::TaskId MlEmbedder::ComputePassagesEmbeddings(
    PassagePriority priority,
    std::vector<std::string> passages,
    ComputePassagesEmbeddingsCallback callback) {
  service_controller_->GetEmbeddings(
      std::move(passages), priority,
      base::BindOnce(
          [](ComputePassagesEmbeddingsCallback callback,
             std::vector<mojom::PassageEmbeddingsResultPtr> results,
             ComputeEmbeddingsStatus status) {
            std::vector<std::string> result_passages;
            std::vector<Embedding> result_embeddings;
            for (auto& result : results) {
              result_passages.push_back(result->passage);
              result_embeddings.emplace_back(result->embeddings);
              result_embeddings.back().Normalize();
            }
            std::move(callback).Run(std::move(result_passages),
                                    std::move(result_embeddings),
                                    kInvalidTaskId, status);
          },
          std::move(callback)));
  return kInvalidTaskId;
}

bool MlEmbedder::TryCancel(TaskId task_id) {
  return false;
}

}  // namespace passage_embeddings
