// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/segmentation_platform/internal/selection/segment_result_provider.h"

#include "base/logging.h"
#include "base/memory/raw_ptr.h"
#include "base/task/sequenced_task_runner.h"
#include "components/segmentation_platform/internal/database/segment_info_database.h"
#include "components/segmentation_platform/internal/database/signal_storage_config.h"
#include "components/segmentation_platform/internal/execution/execution_request.h"
#include "components/segmentation_platform/internal/logging.h"
#include "components/segmentation_platform/internal/metadata/metadata_utils.h"
#include "components/segmentation_platform/internal/proto/model_prediction.pb.h"
#include "components/segmentation_platform/internal/scheduler/execution_service.h"
#include "components/segmentation_platform/internal/stats.h"
#include "components/segmentation_platform/public/model_provider.h"
#include "components/segmentation_platform/public/proto/model_metadata.pb.h"

namespace segmentation_platform {
namespace {

float ComputeDiscreteMapping(const std::string& discrete_mapping_key,
                             float model_score,
                             const proto::SegmentationModelMetadata& metadata) {
  float rank = metadata_utils::ConvertToDiscreteScore(discrete_mapping_key,
                                                      model_score, metadata);
  VLOG(1) << __func__ << ": segment=" << discrete_mapping_key
          << ": result=" << model_score << ", rank=" << rank;

  return rank;
}

ModelProvider* GetModelProvider(ExecutionService* execution_service,
                                SegmentId segment_id,
                                ModelSource model_source) {
  return execution_service
             ? execution_service->GetModelProvider(segment_id, model_source)
             : nullptr;
}

class SegmentResultProviderImpl : public SegmentResultProvider {
 public:
  SegmentResultProviderImpl(SegmentInfoDatabase* segment_database,
                            SignalStorageConfig* signal_storage_config,
                            ExecutionService* execution_service,
                            base::Clock* clock,
                            bool force_refresh_results)
      : segment_database_(segment_database),
        signal_storage_config_(signal_storage_config),
        execution_service_(execution_service),
        clock_(clock),
        force_refresh_results_(force_refresh_results),
        task_runner_(base::SequencedTaskRunner::GetCurrentDefault()) {}

  void GetSegmentResult(std::unique_ptr<GetResultOptions> options) override;

  SegmentResultProviderImpl(const SegmentResultProviderImpl&) = delete;
  SegmentResultProviderImpl& operator=(const SegmentResultProviderImpl&) =
      delete;

 private:
  struct RequestState {
    std::unique_ptr<GetResultOptions> options;
  };

  // TODO (b/294267021) : Refactor this enum to give fallback source to execute.
  // `fallback_action` tells us whether to get score from database or execute
  // server or default model next.
  enum class FallbackAction {
    kGetResultFromDatabaseForServerModel = 0,
    kExecuteServerModel = 1,
    kGetResultFromDatabaseForDefaultModel = 2,
    kExecuteDefaultModel = 3,
  };

  void OnGotModelScore(FallbackAction fallback_action,
                       std::unique_ptr<RequestState> request_state,
                       std::unique_ptr<SegmentResult> db_result);

  using ResultCallbackWithState =
      base::OnceCallback<void(std::unique_ptr<RequestState>,
                              std::unique_ptr<SegmentResult>)>;

  void GetCachedModelScore(std::unique_ptr<RequestState> request_state,
                           ModelSource model_source,
                           ResultCallbackWithState callback);
  void ExecuteModelAndGetScore(std::unique_ptr<RequestState> request_state,
                               ModelSource model_source,
                               ResultCallbackWithState callback);

  void OnModelExecuted(std::unique_ptr<RequestState> request_state,
                       ModelSource model_source,
                       ResultCallbackWithState callback,
                       std::unique_ptr<ModelExecutionResult> result);

  void PostResultCallback(std::unique_ptr<RequestState> request_state,
                          std::unique_ptr<SegmentResult> result);

  void OnSavedSegmentResult(SegmentId segment_id,
                            std::unique_ptr<RequestState> request_state,
                            std::unique_ptr<SegmentResult> segment_result,
                            ResultCallbackWithState callback,
                            bool success);

  const raw_ptr<SegmentInfoDatabase> segment_database_;
  const raw_ptr<SignalStorageConfig> signal_storage_config_;
  const raw_ptr<ExecutionService> execution_service_;
  const raw_ptr<base::Clock> clock_;
  const bool force_refresh_results_;
  scoped_refptr<base::SequencedTaskRunner> task_runner_;

  base::WeakPtrFactory<SegmentResultProviderImpl> weak_ptr_factory_{this};
};

void SegmentResultProviderImpl::GetSegmentResult(
    std::unique_ptr<GetResultOptions> options) {
  auto request_state = std::make_unique<RequestState>();
  request_state->options = std::move(options);
  // If `ignore_db_scores` is true than the server model will be executed now,
  // if that fails to give result, fallback to default model, hence default
  // model is the `fallback_action` if `ignore_db_score` is true. If
  // `ignore_db_scores` is false than the score from database would be read, if
  // that fails to read score from database, fallback to running server model,
  // hence running server model is the `fallback_action` if
  // `ignore_db_score` is false.
  FallbackAction fallback_action = request_state->options->ignore_db_scores
                                       ? FallbackAction::kExecuteDefaultModel
                                       : FallbackAction::kExecuteServerModel;
  auto db_score_callback =
      base::BindOnce(&SegmentResultProviderImpl::OnGotModelScore,
                     weak_ptr_factory_.GetWeakPtr(), fallback_action);

  if (request_state->options->ignore_db_scores) {
    VLOG(1) << __func__ << ": segment="
            << SegmentId_Name(request_state->options->segment_id)
            << " ignoring DB score, executing model.";
    ExecuteModelAndGetScore(std::move(request_state),
                            ModelSource::SERVER_MODEL_SOURCE,
                            std::move(db_score_callback));
    return;
  }

  GetCachedModelScore(std::move(request_state),
                      ModelSource::SERVER_MODEL_SOURCE,
                      std::move(db_score_callback));
}

void SegmentResultProviderImpl::OnGotModelScore(
    FallbackAction fallback_action,
    std::unique_ptr<RequestState> request_state,
    std::unique_ptr<SegmentResult> db_result) {
  if (db_result && db_result->rank.has_value()) {
    PostResultCallback(std::move(request_state), std::move(db_result));
    return;
  }

  // If previously the `fallback_action` was server model, that means
  // that the server model will be running this time, and if that fails to
  // provide the result, the fallback to this would be eithier getting score for
  // default model from database or executing default models based on
  // `ignore_db_scores`.
  if (fallback_action == FallbackAction::kExecuteServerModel) {
    FallbackAction new_fallback_action =
        request_state->options->ignore_db_scores
            ? FallbackAction::kExecuteDefaultModel
            : FallbackAction::kGetResultFromDatabaseForDefaultModel;
    auto db_score_callback =
        base::BindOnce(&SegmentResultProviderImpl::OnGotModelScore,
                       weak_ptr_factory_.GetWeakPtr(), new_fallback_action);
    VLOG(1) << __func__ << ": segment="
            << SegmentId_Name(request_state->options->segment_id)
            << " failed to get score from database, executing server model.";
    ExecuteModelAndGetScore(std::move(request_state),
                            ModelSource::SERVER_MODEL_SOURCE,
                            std::move(db_score_callback));
    return;
  }

  // Handling default models.
  ModelProvider* default_model =
      GetModelProvider(execution_service_, request_state->options->segment_id,
                       ModelSource::DEFAULT_MODEL_SOURCE);
  if (!default_model || !default_model->ModelAvailable()) {
    VLOG(1) << __func__ << ": segment="
            << SegmentId_Name(request_state->options->segment_id)
            << " default provider not available";
    // Make sure the metrics record state of database model failure when client
    // did not provide a default model.
    PostResultCallback(std::move(request_state),
                       std::make_unique<SegmentResult>(db_result->state));
    return;
  }

  if (fallback_action ==
      FallbackAction::kGetResultFromDatabaseForDefaultModel) {
    auto db_score_callback = base::BindOnce(
        &SegmentResultProviderImpl::OnGotModelScore,
        weak_ptr_factory_.GetWeakPtr(), FallbackAction::kExecuteDefaultModel);
    VLOG(1) << __func__ << ": segment="
            << SegmentId_Name(request_state->options->segment_id)
            << " failed to get score from executing server model, getting "
               "score from default model from db.";
    GetCachedModelScore(std::move(request_state),
                        ModelSource::DEFAULT_MODEL_SOURCE,
                        std::move(db_score_callback));
    return;
  }
  VLOG(1) << __func__
          << ": segment=" << SegmentId_Name(request_state->options->segment_id)
          << " failed to get database model score, trying default model.";
  ExecuteModelAndGetScore(
      std::move(request_state), ModelSource::DEFAULT_MODEL_SOURCE,
      base::BindOnce(&SegmentResultProviderImpl::PostResultCallback,
                     weak_ptr_factory_.GetWeakPtr()));
}

void SegmentResultProviderImpl::GetCachedModelScore(
    std::unique_ptr<RequestState> request_state,
    ModelSource model_source,
    ResultCallbackWithState callback) {
  const auto* db_segment_info = segment_database_->GetCachedSegmentInfo(
      request_state->options->segment_id, model_source);
  if (!db_segment_info) {
    VLOG(1) << __func__ << ": segment="
            << SegmentId_Name(request_state->options->segment_id)
            << " does not have a segment info.";
    std::move(callback).Run(
        std::move(request_state),
        std::make_unique<SegmentResult>(
            model_source == ModelSource::DEFAULT_MODEL_SOURCE
                ? ResultState::kDefaultModelSegmentInfoNotAvailable
                : ResultState::kServerModelSegmentInfoNotAvailable));
    return;
  }

  if (force_refresh_results_ || metadata_utils::HasExpiredOrUnavailableResult(
                                    *db_segment_info, clock_->Now())) {
    VLOG(1) << __func__ << ": segment="
            << SegmentId_Name(request_state->options->segment_id)
            << " has expired or unavailable result.";
    std::move(callback).Run(
        std::move(request_state),
        std::make_unique<SegmentResult>(
            model_source == ModelSource::DEFAULT_MODEL_SOURCE
                ? ResultState::kDefaultModelDatabaseScoreNotReady
                : ResultState::kServerModelDatabaseScoreNotReady));
    return;
  }

  VLOG(1) << __func__ << ": Retrieved prediction from database: "
          << segmentation_platform::PredictionResultToDebugString(
                 db_segment_info->prediction_result())
          << " for segment "
          << proto::SegmentId_Name(request_state->options->segment_id);

  float rank =
      ComputeDiscreteMapping(request_state->options->discrete_mapping_key,
                             db_segment_info->prediction_result().result()[0],
                             db_segment_info->model_metadata());
  std::move(callback).Run(std::move(request_state),
                          std::make_unique<SegmentResult>(
                              model_source == ModelSource::DEFAULT_MODEL_SOURCE
                                  ? ResultState::kDefaultModelDatabaseScoreUsed
                                  : ResultState::kServerModelDatabaseScoreUsed,
                              db_segment_info->prediction_result(), rank));
}

void SegmentResultProviderImpl::ExecuteModelAndGetScore(
    std::unique_ptr<RequestState> request_state,
    ModelSource model_source,
    ResultCallbackWithState callback) {
  const auto* segment_info = segment_database_->GetCachedSegmentInfo(
      request_state->options->segment_id, model_source);
  if (!segment_info) {
    VLOG(1) << __func__ << ": segment="
            << SegmentId_Name(request_state->options->segment_id)
            << (model_source == ModelSource::SERVER_MODEL_SOURCE ? " server"
                                                                 : " default")
            << " segment info not available";
    auto state = model_source == ModelSource::SERVER_MODEL_SOURCE
                     ? ResultState::kServerModelSegmentInfoNotAvailable
                     : ResultState::kDefaultModelSegmentInfoNotAvailable;
    std::move(callback).Run(std::move(request_state),
                            std::make_unique<SegmentResult>(state));
    return;
  }

  DCHECK_EQ(metadata_utils::ValidationResult::kValidationSuccess,
            metadata_utils::ValidateMetadata(segment_info->model_metadata()));
  if (!force_refresh_results_ &&
      !signal_storage_config_->MeetsSignalCollectionRequirement(
          segment_info->model_metadata())) {
    VLOG(1) << __func__ << ": segment="
            << SegmentId_Name(request_state->options->segment_id)
            << " signal collection not met";
    auto state = model_source == ModelSource::SERVER_MODEL_SOURCE
                     ? ResultState::kServerModelSignalsNotCollected
                     : ResultState::kDefaultModelSignalsNotCollected;
    std::move(callback).Run(std::move(request_state),
                            std::make_unique<SegmentResult>(state));
    return;
  }

  ModelProvider* provider = GetModelProvider(
      execution_service_, request_state->options->segment_id, model_source);

  auto request = std::make_unique<ExecutionRequest>();
  request->input_context = request_state->options->input_context;
  request->segment_id = segment_info->segment_id();
  request->model_source = model_source;

  request->callback =
      base::BindOnce(&SegmentResultProviderImpl::OnModelExecuted,
                     weak_ptr_factory_.GetWeakPtr(), std::move(request_state),
                     model_source, std::move(callback));
  request->model_provider = provider;

  execution_service_->RequestModelExecution(std::move(request));
}

void SegmentResultProviderImpl::OnModelExecuted(
    std::unique_ptr<RequestState> request_state,
    ModelSource model_source,
    ResultCallbackWithState callback,
    std::unique_ptr<ModelExecutionResult> result) {
  SegmentId segment_id = request_state->options->segment_id;
  ResultState state = ResultState::kUnknown;
  proto::PredictionResult prediction_result;

  const auto* segment_info =
      segment_database_->GetCachedSegmentInfo(segment_id, model_source);
  if (!segment_info) {
    state = model_source == ModelSource::SERVER_MODEL_SOURCE
                 ? ResultState::kServerModelSegmentInfoNotAvailable
                 : ResultState::kDefaultModelSegmentInfoNotAvailable;
    std::move(callback).Run(std::move(request_state),
                            std::make_unique<SegmentResult>(state));
    return;
  }

  bool is_default_model = model_source == ModelSource::DEFAULT_MODEL_SOURCE;
  bool success = result->status == ModelExecutionStatus::kSuccess &&
                 !result->scores.empty();
  std::unique_ptr<SegmentResult> segment_result;
  if (success) {
    state = is_default_model ? ResultState::kDefaultModelExecutionScoreUsed
                             : ResultState::kServerModelExecutionScoreUsed;
    prediction_result = metadata_utils::CreatePredictionResult(
        result->scores, segment_info->model_metadata().output_config(),
        clock_->Now(), segment_info->model_version());
    float rank = ComputeDiscreteMapping(
        request_state->options->discrete_mapping_key,
        prediction_result.result(0), segment_info->model_metadata());
    segment_result =
        std::make_unique<SegmentResult>(state, prediction_result, rank);
    segment_result->model_inputs = std::move(result->inputs);
    VLOG(1) << __func__ << ": " << (is_default_model ? "Default" : "Server")
            << " model executed successfully. Result: "
            << segmentation_platform::PredictionResultToDebugString(
                   prediction_result)
            << " for segment " << proto::SegmentId_Name(segment_id);
  } else {
    state = is_default_model ? ResultState::kDefaultModelExecutionFailed
                             : ResultState::kServerModelExecutionFailed;
    segment_result = std::make_unique<SegmentResult>(state);
    VLOG(1) << __func__ << ": " << (is_default_model ? "Default" : "Server")
            << " model execution failed" << " for segment "
            << proto::SegmentId_Name(segment_id);
  }

  if (request_state->options->save_results_to_db) {
    segment_database_->SaveSegmentResult(
        segment_id, model_source,
        success ? std::make_optional(prediction_result) : std::nullopt,
        base::BindOnce(&SegmentResultProviderImpl::OnSavedSegmentResult,
                       weak_ptr_factory_.GetWeakPtr(),
                       segment_info->segment_id(), std::move(request_state),
                       std::move(segment_result), std::move(callback)));
    return;
  }
  std::move(callback).Run(std::move(request_state), std::move(segment_result));
}

void SegmentResultProviderImpl::PostResultCallback(
    std::unique_ptr<RequestState> request_state,
    std::unique_ptr<SegmentResult> result) {
  task_runner_->PostTask(
      FROM_HERE, base::BindOnce(std::move(request_state->options->callback),
                                std::move(result)));
}

void SegmentResultProviderImpl::OnSavedSegmentResult(
    SegmentId segment_id,
    std::unique_ptr<RequestState> request_state,
    std::unique_ptr<SegmentResult> segment_result,
    ResultCallbackWithState callback,
    bool success) {
  stats::RecordModelExecutionSaveResult(segment_id, success);
  if (!success) {
    // TODO(ssid): Consider removing this enum, this is the only case where the
    // execution status is recorded twice for the same execution request.
    stats::RecordModelExecutionStatus(
        segment_id,
        /*default_provider=*/false,
        ModelExecutionStatus::kFailedToSaveResultAfterSuccess);
  }
  std::move(callback).Run(std::move(request_state), std::move(segment_result));
}

}  // namespace

SegmentResultProvider::SegmentResult::SegmentResult(ResultState state)
    : state(state) {}
SegmentResultProvider::SegmentResult::SegmentResult(
    ResultState state,
    const proto::PredictionResult& prediction_result,
    float rank)
    : state(state), result(prediction_result), rank(rank) {}
SegmentResultProvider::SegmentResult::~SegmentResult() = default;

SegmentResultProvider::GetResultOptions::GetResultOptions() = default;
SegmentResultProvider::GetResultOptions::~GetResultOptions() = default;

// static
std::unique_ptr<SegmentResultProvider> SegmentResultProvider::Create(
    SegmentInfoDatabase* segment_database,
    SignalStorageConfig* signal_storage_config,
    ExecutionService* execution_service,
    base::Clock* clock,
    bool force_refresh_results) {
  return std::make_unique<SegmentResultProviderImpl>(
      segment_database, signal_storage_config, execution_service, clock,
      force_refresh_results);
}

}  // namespace segmentation_platform
