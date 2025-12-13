// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/passage_embeddings/passage_embeddings_service_controller.h"

#include <ranges>

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/metrics/histogram_functions.h"
#include "base/not_fatal_until.h"
#include "base/notreached.h"
#include "base/task/thread_pool.h"
#include "components/optimization_guide/core/optimization_guide_util.h"
#include "components/passage_embeddings/internal/scheduling_embedder.h"
#include "components/passage_embeddings/passage_embeddings_features.h"
#include "components/passage_embeddings/passage_embeddings_types.h"
#include "mojo/public/cpp/bindings/callback_helpers.h"
#include "services/passage_embeddings/public/mojom/passage_embeddings.mojom.h"

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
mojom::PassageEmbedderParamsPtr MakeEmbedderParams() {
  auto params = mojom::PassageEmbedderParams::New();
  params->user_initiated_priority_num_threads =
      kUserInitiatedPriorityNumThreads.Get();
  params->urgent_priority_num_threads = kUrgentPriorityNumThreads.Get();
  params->passive_priority_num_threads = kPassivePriorityNumThreads.Get();
  params->embedder_cache_size = kEmbedderCacheSize.Get();
  params->allow_gpu_execution = kAllowGpuExecution.Get();
  return params;
}

mojom::PassagePriority PassagePriorityToMojom(PassagePriority priority) {
  switch (priority) {
    case kUserInitiated:
      return mojom::PassagePriority::kUserInitiated;
    case kUrgent:
      return mojom::PassagePriority::kUrgent;
    case kPassive:
    case kLatent:
      return mojom::PassagePriority::kPassive;
  }
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

PassageEmbeddingsServiceController::PassageEmbeddingsServiceController()
    : embedder_(std::make_unique<SchedulingEmbedder>(
          /*embedder_metadata_provider=*/this,
          /*get_embeddings_callback=*/
          base::BindRepeating(
              &PassageEmbeddingsServiceController::GetEmbeddings,
              base::Unretained(this)),
          kSchedulerMaxJobs.Get(),
          kSchedulerMaxBatchSize.Get(),
          kUsePerformanceScenario.Get())) {}

PassageEmbeddingsServiceController::~PassageEmbeddingsServiceController() =
    default;

bool PassageEmbeddingsServiceController::MaybeUpdateModelInfo(
    base::optional_ref<const optimization_guide::ModelInfo> model_info) {
  // Got the same version again. Do not run through rest of logic.
  if (model_info && model_version_ == model_info->GetVersion()) {
    return true;
  }

  // Reset everything, so if the model info is invalid, the service controller
  // would stop accepting requests.
  embeddings_model_path_.clear();
  sp_model_path_.clear();
  model_metadata_ = std::nullopt;
  ResetEmbedderRemote();

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
  observer_list_.Notify(&EmbedderMetadataObserver::EmbedderMetadataUpdated,
                        GetEmbedderMetadata());
  return true;
}

void PassageEmbeddingsServiceController::LoadModelsToService(
    mojo::PendingReceiver<mojom::PassageEmbedder> receiver,
    base::ElapsedTimer service_launch_timer,
    mojom::PassageEmbeddingsLoadModelsParamsPtr params) {
  if (!service_remote_) {
    // Close the model files in a background thread.
    base::ThreadPool::PostTaskAndReply(
        FROM_HERE, {base::MayBlock()},
        base::DoNothingWithBoundArgs(std::move(params)),
        base::BindOnce(&PassageEmbeddingsServiceController::OnLoadModelsResult,
                       weak_ptr_factory_.GetWeakPtr(),
                       std::move(service_launch_timer), /*success=*/false));
    return;
  }

  service_remote_->LoadModels(
      std::move(params), MakeEmbedderParams(), std::move(receiver),
      base::BindOnce(&PassageEmbeddingsServiceController::OnLoadModelsResult,
                     weak_ptr_factory_.GetWeakPtr(),
                     std::move(service_launch_timer)));
}

void PassageEmbeddingsServiceController::OnLoadModelsResult(
    base::ElapsedTimer service_launch_timer,
    bool success) {
  if (!success) {
    ResetEmbedderRemote();
    return;
  }

  base::UmaHistogramTimes("History.Embeddings.Embedder.LaunchDuration",
                          service_launch_timer.Elapsed());
}

Embedder* PassageEmbeddingsServiceController::GetEmbedder() {
  return embedder_.get();
}

void PassageEmbeddingsServiceController::AddObserver(
    EmbedderMetadataObserver* observer) {
  if (EmbedderReady()) {
    observer->EmbedderMetadataUpdated(GetEmbedderMetadata());
  }
  observer_list_.AddObserver(observer);
}

void PassageEmbeddingsServiceController::RemoveObserver(
    EmbedderMetadataObserver* observer) {
  observer_list_.RemoveObserver(observer);
}

void PassageEmbeddingsServiceController::GetEmbeddings(
    std::vector<std::string> passages,
    PassagePriority priority,
    GetEmbeddingsResultCallback callback) {
  if (passages.empty()) {
    std::move(callback).Run({}, ComputeEmbeddingsStatus::kSuccess);
    return;
  }

  if (!EmbedderReady()) {
    VLOG(1) << "Missing model path: embeddings='" << embeddings_model_path_
            << "'; sp='" << sp_model_path_ << "'";
    std::move(callback).Run({}, ComputeEmbeddingsStatus::kModelUnavailable);
    return;
  }

  if (!embedder_remote_) {
    base::ElapsedTimer service_launch_timer;
    MaybeLaunchService();

    auto receiver = embedder_remote_.BindNewPipeAndPassReceiver();
    // Unretained is safe because `this` owns `embedder_remote_`, which
    // synchronously calls the disconnect and idle handlers.
    embedder_remote_.set_disconnect_handler(
        base::BindOnce(&PassageEmbeddingsServiceController::ResetEmbedderRemote,
                       base::Unretained(this)));
    embedder_remote_.set_idle_handler(
        kEmbedderTimeout.Get(),
        base::BindRepeating(
            &PassageEmbeddingsServiceController::ResetEmbedderRemote,
            base::Unretained(this)));
    base::ThreadPool::PostTaskAndReplyWithResult(
        FROM_HERE, {base::MayBlock()},
        base::BindOnce(&MakeModelParams, embeddings_model_path_, sp_model_path_,
                       model_metadata_->input_window_size()),
        base::BindOnce(&PassageEmbeddingsServiceController::LoadModelsToService,
                       weak_ptr_factory_.GetWeakPtr(), std::move(receiver),
                       std::move(service_launch_timer)));
  }

  pending_requests_.push_back(next_request_id_);
  base::ElapsedTimer generate_embeddings_timer;
  embedder_remote_->GenerateEmbeddings(
      std::move(passages), PassagePriorityToMojom(priority),
      mojo::WrapCallbackWithDefaultInvokeIfNotRun(
          base::BindOnce(&PassageEmbeddingsServiceController::OnGotEmbeddings,
                         weak_ptr_factory_.GetWeakPtr(), next_request_id_,
                         std::move(callback),
                         std::move(generate_embeddings_timer), priority),
          std::vector<mojom::PassageEmbeddingsResultPtr>()));
  next_request_id_++;
}

bool PassageEmbeddingsServiceController::EmbedderReady() {
  return !sp_model_path_.empty() && !embeddings_model_path_.empty();
}

EmbedderMetadata PassageEmbeddingsServiceController::GetEmbedderMetadata() {
  if (model_metadata_->score_threshold() > 0.0) {
    return EmbedderMetadata(model_version_, model_metadata_->output_size(),
                            model_metadata_->score_threshold());
  }

  return EmbedderMetadata(model_version_, model_metadata_->output_size());
}

bool PassageEmbeddingsServiceController::EmbedderRunning() {
  return !pending_requests_.empty();
}

void PassageEmbeddingsServiceController::ResetEmbedderRemote() {
  embedder_remote_.reset();
}

void PassageEmbeddingsServiceController::OnGotEmbeddings(
    RequestId request_id,
    GetEmbeddingsResultCallback callback,
    base::ElapsedTimer generate_embeddings_timer,
    PassagePriority priority,
    std::vector<mojom::PassageEmbeddingsResultPtr> results) {
  // Mojo invokes the callbacks in the order in which `GenerateEmbeddings()` was
  // called. Therefore, `request_id` should be expected at the front of
  // `pending_requests_`. However, when `embedder_remote_` disconnects and the
  // callbacks are dropped, `mojo::WrapCallbackWithDefaultInvokeIfNotRun()`
  // invokes the callbacks in the reverse order in which they were bound.
  auto it = std::ranges::find(pending_requests_, request_id);
  if (it != pending_requests_.end()) {
    pending_requests_.erase(it);
  } else {
    NOTREACHED(base::NotFatalUntil::M140);
  }

  auto status = results.empty() ? ComputeEmbeddingsStatus::kExecutionFailure
                                : ComputeEmbeddingsStatus::kSuccess;
  std::move(callback).Run(std::move(results), status);

  if (status == ComputeEmbeddingsStatus::kSuccess) {
    const base::TimeDelta duration = generate_embeddings_timer.Elapsed();
    base::UmaHistogramTimes("History.Embeddings.TaskDuration", duration);
    const char* priority_histogram = nullptr;
    switch (priority) {
      case kUserInitiated:
        priority_histogram = "History.Embeddings.TaskDuration.UserInitiated";
        break;

      case kUrgent:
        priority_histogram = "History.Embeddings.TaskDuration.Urgent";
        break;

      case kPassive:
        priority_histogram = "History.Embeddings.TaskDuration.Passive";
        break;

      default:
        priority_histogram = "History.Embeddings.TaskDuration.Other";
    }
    base::UmaHistogramTimes(priority_histogram, duration);
  }
}

}  // namespace passage_embeddings
