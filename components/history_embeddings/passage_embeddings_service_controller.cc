// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/history_embeddings/passage_embeddings_service_controller.h"

#include "base/task/thread_pool.h"
#include "components/history_embeddings/vector_database.h"
#include "components/optimization_guide/core/optimization_guide_util.h"

namespace {

// Time it takes before the remote idles.
constexpr int kRemoteTimeoutSeconds = 60;

passage_embeddings::mojom::PassageEmbeddingsLoadModelsParamsPtr MakeModelParams(
    const base::FilePath& embeddings_path,
    const base::FilePath& sp_path,
    uint32_t input_window_size) {
  auto params =
      passage_embeddings::mojom::PassageEmbeddingsLoadModelsParams::New();
  params->embeddings_model = base::File(
      embeddings_path, base::File::FLAG_OPEN | base::File::FLAG_READ);
  params->sp_model =
      base::File(sp_path, base::File::FLAG_OPEN | base::File::FLAG_READ);
  params->input_window_size = input_window_size;
  return params;
}

}  // namespace

namespace history_embeddings {

PassageEmbeddingsServiceController::PassageEmbeddingsServiceController() =
    default;
PassageEmbeddingsServiceController::~PassageEmbeddingsServiceController() =
    default;

// TODO(b/338650221): Add histograms for bad model info.
bool PassageEmbeddingsServiceController::MaybeUpdateModelPaths(
    base::optional_ref<const optimization_guide::ModelInfo> model_info) {
  // Reset everything, so if the model info is invalid, the service controller
  // would stop accepting requests.
  embeddings_model_path_.clear();
  sp_model_path_.clear();
  model_metadata_ = std::nullopt;
  ResetRemotes();

  if (!model_info.has_value() || !model_info->GetModelMetadata()) {
    return false;
  }

  // The only additional file should be the sentencepiece model.
  base::flat_set<base::FilePath> additional_files =
      model_info->GetAdditionalFiles();
  if (additional_files.size() != 1u) {
    return false;
  }

  // Check validity of model metadata.
  const std::optional<optimization_guide::proto::Any>& metadata =
      model_info->GetModelMetadata();
  std::optional<history_embeddings::proto::PassageEmbeddingsModelMetadata>
      embeddings_metadata = optimization_guide::ParsedAnyMetadata<
          history_embeddings::proto::PassageEmbeddingsModelMetadata>(*metadata);
  if (!embeddings_metadata) {
    return false;
  }

  model_version_ = model_info->GetVersion();
  model_metadata_ = embeddings_metadata;
  embeddings_model_path_ = model_info->GetModelFilePath();
  sp_model_path_ = *(additional_files.begin());

  CHECK(!embeddings_model_path_.empty());
  CHECK(!sp_model_path_.empty());
  return true;
}

void PassageEmbeddingsServiceController::LoadModelsToService(
    mojo::PendingReceiver<passage_embeddings::mojom::PassageEmbedder> model,
    passage_embeddings::mojom::PassageEmbeddingsLoadModelsParamsPtr params) {
  if (!service_remote_) {
    // Close the model files in a background thread.
    base::ThreadPool::PostTask(FROM_HERE, {base::MayBlock()},
                               base::DoNothingWithBoundArgs(std::move(params)));
    return;
  }

  service_remote_->LoadModels(
      std::move(params), std::move(model),
      base::BindOnce(&PassageEmbeddingsServiceController::OnLoadModelsResult,
                     weak_ptr_factory_.GetWeakPtr()));
}

void PassageEmbeddingsServiceController::OnLoadModelsResult(bool success) {
  if (!success) {
    ResetRemotes();
  }
}

EmbedderMetadata PassageEmbeddingsServiceController::GetEmbedderMetadata() {
  return EmbedderMetadata(model_version_, model_metadata_->output_size());
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
        base::BindOnce(&MakeModelParams, embeddings_model_path_, sp_model_path_,
                       model_metadata_->input_window_size()),
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
