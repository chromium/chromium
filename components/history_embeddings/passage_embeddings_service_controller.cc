// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/history_embeddings/passage_embeddings_service_controller.h"

#include "base/task/thread_pool.h"
#include "components/history_embeddings/vector_database.h"

namespace {

// Time it takes before the remote idles.
constexpr int kRemoteTimeoutSeconds = 60;

passage_embeddings::mojom::PassageEmbeddingsModelAssetsPtr MakeModelAssets(
    const base::FilePath& embeddings_path,
    const base::FilePath& sp_path) {
  auto assets = passage_embeddings::mojom::PassageEmbeddingsModelAssets::New();
  assets->embeddings_model = base::File(
      embeddings_path, base::File::FLAG_OPEN | base::File::FLAG_READ);
  assets->sp_model =
      base::File(sp_path, base::File::FLAG_OPEN | base::File::FLAG_READ);
  return assets;
}

}  // namespace

namespace history_embeddings {

PassageEmbeddingsServiceController::PassageEmbeddingsServiceController() =
    default;
PassageEmbeddingsServiceController::~PassageEmbeddingsServiceController() =
    default;

void PassageEmbeddingsServiceController::MaybeUpdateModelPaths(
    base::optional_ref<const optimization_guide::ModelInfo> model_info) {
  embeddings_model_path_.clear();
  sp_model_path_.clear();
  ResetRemotes();

  if (!model_info.has_value()) {
    return;
  }

  // The only additional file should be the sentencepiece model.
  base::flat_set<base::FilePath> additional_files =
      model_info->GetAdditionalFiles();
  if (additional_files.size() != 1u) {
    return;
  }

  embeddings_model_path_ = model_info->GetModelFilePath();
  sp_model_path_ = *(additional_files.begin());

  CHECK(!embeddings_model_path_.empty());
  CHECK(!sp_model_path_.empty());
}

void PassageEmbeddingsServiceController::LoadModelsToService(
    mojo::PendingReceiver<passage_embeddings::mojom::PassageEmbedder> model,
    passage_embeddings::mojom::PassageEmbeddingsModelAssetsPtr assets) {
  if (!service_remote_) {
    // Close the model files in a background thread.
    base::ThreadPool::PostTask(FROM_HERE, {base::MayBlock()},
                               base::DoNothingWithBoundArgs(std::move(assets)));
    return;
  }

  service_remote_->LoadModels(
      std::move(assets), std::move(model),
      base::BindOnce(&PassageEmbeddingsServiceController::OnLoadModelsResult,
                     weak_ptr_factory_.GetWeakPtr()));
}

void PassageEmbeddingsServiceController::OnLoadModelsResult(bool success) {
  if (!success) {
    ResetRemotes();
  }
}

void PassageEmbeddingsServiceController::GetEmbeddings(
    std::vector<std::string> passages,
    GetEmbeddingsCallback callback) {
  if (embeddings_model_path_.empty() || sp_model_path_.empty()) {
    VLOG(1) << "Missing model path: embeddings='" << embeddings_model_path_
            << "'; sp='" << sp_model_path_ << "'";
    std::move(callback).Run({}, {});
    return;
  }

  if (!service_remote_) {
    LaunchService();
    base::ThreadPool::PostTaskAndReplyWithResult(
        FROM_HERE, {base::MayBlock()},
        base::BindOnce(&MakeModelAssets, embeddings_model_path_,
                       sp_model_path_),
        base::BindOnce(&PassageEmbeddingsServiceController::LoadModelsToService,
                       weak_ptr_factory_.GetWeakPtr(),
                       embedder_remote_.BindNewPipeAndPassReceiver()));
    embedder_remote_.set_disconnect_handler(
        base::BindOnce(&PassageEmbeddingsServiceController::OnDisconnected,
                       weak_ptr_factory_.GetWeakPtr()));
    embedder_remote_.set_idle_handler(
        base::Seconds(kRemoteTimeoutSeconds),
        base::BindRepeating(&PassageEmbeddingsServiceController::ResetRemotes,
                            weak_ptr_factory_.GetWeakPtr()));
  }

  embedder_remote_->GenerateEmbeddings(
      std::move(passages),
      base::BindOnce(
          [](GetEmbeddingsCallback callback,
             std::vector<passage_embeddings::mojom::PassageEmbeddingsResultPtr>
                 results) {
            std::vector<std::string> result_passages;
            std::vector<Embedding> result_embeddings;
            for (auto& result : results) {
              result_passages.push_back(result->passage);
              result_embeddings.emplace_back(result->embeddings);
              result_embeddings.back().Normalize();
            }
            std::move(callback).Run(std::move(result_passages),
                                    std::move(result_embeddings));
          },
          std::move(callback)));
}

void PassageEmbeddingsServiceController::ResetRemotes() {
  service_remote_.reset();
  embedder_remote_.reset();
}

void PassageEmbeddingsServiceController::OnDisconnected() {
  embedder_remote_.reset();
}

}  // namespace history_embeddings
