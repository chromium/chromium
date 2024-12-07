// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/passage_embeddings/passage_embeddings_service_controller.h"

#include "base/metrics/histogram_functions.h"
#include "base/task/thread_pool.h"
#include "components/optimization_guide/core/optimization_guide_util.h"
#include "components/passage_embeddings/passage_embeddings_features.h"

namespace passage_embeddings {

namespace {

mojom::PassageEmbeddingsLoadModelsParamsPtr MakeModelParams(
    const base::FilePath& embeddings_path,
    const base::FilePath& sp_path,
    uint32_t input_window_size) {
  auto params = mojom::PassageEmbeddingsLoadModelsParams::New();
  params->embeddings_model = base::File(
      embeddings_path, base::File::FLAG_OPEN | base::File::FLAG_READ);
  params->sp_model =
      base::File(sp_path, base::File::FLAG_OPEN | base::File::FLAG_READ);
  params->input_window_size = input_window_size;
  return params;
}

// Makes the parameters used to run the passage embedder.
passage_embeddings::mojom::PassageEmbedderParamsPtr MakeEmbedderParams() {
  auto params = passage_embeddings::mojom::PassageEmbedderParams::New();
  params->user_initiated_priority_num_threads =
      kUserInitiatedPriorityNumThreads.Get();
  params->passive_priority_num_threads = kPassivePriorityNumThreads.Get();
  params->embedder_cache_size = kEmbedderCacheSize.Get();
  return params;
}

class ScopedEmbeddingsModelInfoStatusLogger {
 public:
  ScopedEmbeddingsModelInfoStatusLogger() = default;
  ~ScopedEmbeddingsModelInfoStatusLogger() {
    CHECK_NE(EmbeddingsModelInfoStatus::kUnknown, status_);
    base::UmaHistogramEnumeration(kModelInfoMetricName, status_);
  }

  void set_status(EmbeddingsModelInfoStatus status) { status_ = status; }

 private:
  EmbeddingsModelInfoStatus status_ = EmbeddingsModelInfoStatus::kUnknown;
};

}  // namespace

PassageEmbeddingsServiceController::PassageEmbeddingsServiceController() =
    default;
PassageEmbeddingsServiceController::~PassageEmbeddingsServiceController() =
    default;

bool PassageEmbeddingsServiceController::MaybeUpdateModelInfo(
    base::optional_ref<const optimization_guide::ModelInfo> model_info) {
  // Reset everything, so if the model info is invalid, the service controller
  // would stop accepting requests.
  embeddings_model_path_.clear();
  sp_model_path_.clear();
  model_metadata_ = std::nullopt;
  ResetRemotes();

  ScopedEmbeddingsModelInfoStatusLogger logger;
  if (!model_info.has_value()) {
    logger.set_status(EmbeddingsModelInfoStatus::kEmpty);
    return false;
  }

  // The only additional file should be the sentencepiece model.
  base::flat_set<base::FilePath> additional_files =
      model_info->GetAdditionalFiles();
  if (additional_files.size() != 1u) {
    logger.set_status(EmbeddingsModelInfoStatus::kInvalidAdditionalFiles);
    return false;
  }

  // Check validity of model metadata.
  const std::optional<optimization_guide::proto::Any>& metadata =
      model_info->GetModelMetadata();
  if (!metadata) {
    logger.set_status(EmbeddingsModelInfoStatus::kNoMetadata);
    return false;
  }
  std::optional<optimization_guide::proto::PassageEmbeddingsModelMetadata>
      embeddings_metadata = optimization_guide::ParsedAnyMetadata<
          optimization_guide::proto::PassageEmbeddingsModelMetadata>(*metadata);
  if (!embeddings_metadata) {
    logger.set_status(EmbeddingsModelInfoStatus::kInvalidMetadata);
    return false;
  }

  model_version_ = model_info->GetVersion();
  model_metadata_ = embeddings_metadata;
  embeddings_model_path_ = model_info->GetModelFilePath();
  sp_model_path_ = *(additional_files.begin());

  CHECK(EmbedderReady());
  logger.set_status(EmbeddingsModelInfoStatus::kValid);
  return true;
}

void PassageEmbeddingsServiceController::LoadModelsToService(
    mojo::PendingReceiver<mojom::PassageEmbedder> receiver,
    mojom::PassageEmbeddingsLoadModelsParamsPtr params) {
  if (!service_remote_) {
    // Close the model files in a background thread.
    base::ThreadPool::PostTask(FROM_HERE, {base::MayBlock()},
                               base::DoNothingWithBoundArgs(std::move(params)));
    return;
  }

  service_remote_->LoadModels(
      std::move(params), MakeEmbedderParams(), std::move(receiver),
      base::BindOnce(&PassageEmbeddingsServiceController::OnLoadModelsResult,
                     weak_ptr_factory_.GetWeakPtr()));
}

void PassageEmbeddingsServiceController::OnLoadModelsResult(bool success) {
  if (!success) {
    ResetRemotes();
  }
}

EmbedderMetadata PassageEmbeddingsServiceController::GetEmbedderMetadata() {
  if (model_metadata_->score_threshold() > 0.0) {
    return EmbedderMetadata(model_version_, model_metadata_->output_size(),
                            model_metadata_->score_threshold());
  }

  return EmbedderMetadata(model_version_, model_metadata_->output_size());
}

void PassageEmbeddingsServiceController::GetEmbeddings(
    std::vector<std::string> passages,
    mojom::PassagePriority priority,
    GetEmbeddingsCallback callback) {
  if (!EmbedderReady()) {
    VLOG(1) << "Missing model path: embeddings='" << embeddings_model_path_
            << "'; sp='" << sp_model_path_ << "'";
    std::move(callback).Run({}, ComputeEmbeddingsStatus::KModelUnavailable);
    return;
  }

  if (!service_remote_) {
    LaunchService();
    auto receiver = embedder_remote_.BindNewPipeAndPassReceiver();
    embedder_remote_.set_disconnect_handler(
        base::BindOnce(&PassageEmbeddingsServiceController::OnDisconnected,
                       weak_ptr_factory_.GetWeakPtr()));
    embedder_remote_.set_idle_handler(
        kEmbedderTimeout.Get(),
        base::BindRepeating(&PassageEmbeddingsServiceController::ResetRemotes,
                            weak_ptr_factory_.GetWeakPtr()));
    base::ThreadPool::PostTaskAndReplyWithResult(
        FROM_HERE, {base::MayBlock()},
        base::BindOnce(&MakeModelParams, embeddings_model_path_, sp_model_path_,
                       model_metadata_->input_window_size()),
        base::BindOnce(&PassageEmbeddingsServiceController::LoadModelsToService,
                       weak_ptr_factory_.GetWeakPtr(), std::move(receiver)));
  }

  embedder_remote_->GenerateEmbeddings(
      std::move(passages), priority,
      base::BindOnce(
          [](GetEmbeddingsCallback callback,
             std::vector<mojom::PassageEmbeddingsResultPtr> results) {
            auto status = results.empty()
                              ? ComputeEmbeddingsStatus::kExecutionFailure
                              : ComputeEmbeddingsStatus::KSuccess;
            std::move(callback).Run(std::move(results), status);
          },
          std::move(callback)));
}

bool PassageEmbeddingsServiceController::EmbedderReady() {
  return !sp_model_path_.empty() && !embeddings_model_path_.empty();
}

void PassageEmbeddingsServiceController::ResetRemotes() {
  service_remote_.reset();
  embedder_remote_.reset();
}

void PassageEmbeddingsServiceController::OnDisconnected() {
  embedder_remote_.reset();
}

}  // namespace passage_embeddings
