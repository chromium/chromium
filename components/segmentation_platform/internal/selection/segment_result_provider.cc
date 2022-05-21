// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/segmentation_platform/internal/selection/segment_result_provider.h"

#include <map>

#include "base/logging.h"
#include "base/memory/raw_ptr.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "components/segmentation_platform/internal/database/segment_info_database.h"
#include "components/segmentation_platform/internal/database/signal_storage_config.h"
#include "components/segmentation_platform/internal/execution/default_model_manager.h"
#include "components/segmentation_platform/internal/execution/execution_request.h"
#include "components/segmentation_platform/internal/metadata/metadata_utils.h"
#include "components/segmentation_platform/internal/proto/model_metadata.pb.h"
#include "components/segmentation_platform/internal/proto/model_prediction.pb.h"
#include "components/segmentation_platform/internal/scheduler/execution_service.h"
#include "components/segmentation_platform/public/model_provider.h"

namespace segmentation_platform {
namespace {

int ComputeDiscreteMapping(const std::string& segmentation_key,
                           const proto::SegmentInfo& segment_info) {
  int rank = metadata_utils::ConvertToDiscreteScore(
      segmentation_key, segment_info.prediction_result().result(),
      segment_info.model_metadata());
  VLOG(1) << __func__
          << ": segment=" << SegmentId_Name(segment_info.segment_id())
          << ": result=" << segment_info.prediction_result().result()
          << ", rank=" << rank;

  return rank;
}

proto::SegmentInfo* GetSegmentInfo(
    DefaultModelManager::SegmentInfoList& available_segments,
    bool default_model) {
  proto::SegmentInfo* segment_info = nullptr;
  DefaultModelManager::SegmentSource needed_source =
      default_model ? DefaultModelManager::SegmentSource::DEFAULT_MODEL
                    : DefaultModelManager::SegmentSource::DATABASE;
  for (const auto& info : available_segments) {
    if (info->segment_source == needed_source) {
      segment_info = &info->segment_info;
      break;
    }
  }
  return segment_info;
}

class SegmentResultProviderImpl : public SegmentResultProvider {
 public:
  SegmentResultProviderImpl(SegmentInfoDatabase* segment_database,
                            SignalStorageConfig* signal_storage_config,
                            DefaultModelManager* default_model_manager,
                            ExecutionService* execution_service,
                            base::Clock* clock,
                            bool force_refresh_results)
      : segment_database_(segment_database),
        signal_storage_config_(signal_storage_config),
        default_model_manager_(default_model_manager),
        execution_service_(execution_service),
        clock_(clock),
        force_refresh_results_(force_refresh_results),
        task_runner_(base::SequencedTaskRunnerHandle::Get()) {}

  void GetSegmentResult(GetResultOptions&& options) override;

  SegmentResultProviderImpl(SegmentResultProviderImpl&) = delete;
  SegmentResultProviderImpl& operator=(SegmentResultProviderImpl&) = delete;

 private:
  struct RequestState {
    raw_ptr<ModelProvider> default_provider;
    GetResultOptions options;
  };

  void OnGetSegmentInfo(
      std::unique_ptr<RequestState> request_state,
      DefaultModelManager::SegmentInfoList available_segments);

  void TryExecuteModelAndGetScore(
      std::unique_ptr<RequestState> request_state,
      DefaultModelManager::SegmentInfoList available_segments);

  void TryGetScoreFromDefaultModel(
      std::unique_ptr<RequestState> request_state,
      SegmentResultProvider::ResultState existing_state,
      DefaultModelManager::SegmentInfoList available_segments);
  void OnModelExecuted(std::unique_ptr<RequestState> request_state,
                       std::unique_ptr<proto::SegmentInfo> segment_info,
                       bool is_metrics_for_default,
                       const std::pair<float, ModelExecutionStatus>& result);

  void PostResultCallback(std::unique_ptr<RequestState> request_state,
                          std::unique_ptr<SegmentResult> result);

  const raw_ptr<SegmentInfoDatabase> segment_database_;
  const raw_ptr<SignalStorageConfig> signal_storage_config_;
  const raw_ptr<DefaultModelManager> default_model_manager_;
  const raw_ptr<ExecutionService> execution_service_;
  const raw_ptr<base::Clock> clock_;
  const bool force_refresh_results_;
  scoped_refptr<base::SequencedTaskRunner> task_runner_;

  base::WeakPtrFactory<SegmentResultProviderImpl> weak_ptr_factory_{this};
};

void SegmentResultProviderImpl::GetSegmentResult(GetResultOptions&& options) {
  const SegmentId segment_id = options.segment_id;
  auto request_state = std::make_unique<RequestState>();
  request_state = std::make_unique<RequestState>();
  request_state->options = std::move(options);
  // Factory can be null in tests.
  request_state->default_provider =
      default_model_manager_
          ? default_model_manager_->GetDefaultProvider(segment_id)
          : nullptr;

  default_model_manager_->GetAllSegmentInfoFromBothModels(
      {segment_id}, segment_database_,
      base::BindOnce(&SegmentResultProviderImpl::OnGetSegmentInfo,
                     weak_ptr_factory_.GetWeakPtr(), std::move(request_state)));
}

void SegmentResultProviderImpl::OnGetSegmentInfo(
    std::unique_ptr<RequestState> request_state,
    DefaultModelManager::SegmentInfoList available_segments) {
  const proto::SegmentInfo* db_segment_info =
      GetSegmentInfo(available_segments, /*default_model=*/false);

  // Don't compute results if we don't have enough signals, or don't have
  // valid unexpired results for any of the segments.
  if (!db_segment_info) {
    VLOG(1) << __func__
            << ": segment=" << SegmentId_Name(request_state->options.segment_id)
            << " does not have segment info.";
    TryGetScoreFromDefaultModel(std::move(request_state),
                                ResultState::kSegmentNotAvailable,
                                std::move(available_segments));
    return;
  }

  // TODO(ssid): Remove this check when using database results since the
  // scheduler does this before executing the model.
  if (!force_refresh_results_ &&
      !signal_storage_config_->MeetsSignalCollectionRequirement(
          db_segment_info->model_metadata())) {
    VLOG(1) << __func__
            << ": segment=" << SegmentId_Name(db_segment_info->segment_id())
            << " does not meet signal collection requirements.";
    TryGetScoreFromDefaultModel(std::move(request_state),
                                ResultState::kSignalsNotCollected,
                                std::move(available_segments));
    return;
  }

  if (request_state->options.ignore_db_scores) {
    VLOG(1) << __func__
            << ": segment=" << SegmentId_Name(db_segment_info->segment_id())
            << " executing model to get score";
    TryExecuteModelAndGetScore(std::move(request_state),
                               std::move(available_segments));
    return;
  }

  if (metadata_utils::HasExpiredOrUnavailableResult(*db_segment_info,
                                                    clock_->Now())) {
    VLOG(1) << __func__
            << ": segment=" << SegmentId_Name(db_segment_info->segment_id())
            << " has expired or unavailable result.";
    TryGetScoreFromDefaultModel(std::move(request_state),
                                ResultState::kDatabaseScoreNotReady,
                                std::move(available_segments));
    return;
  }

  int rank = ComputeDiscreteMapping(request_state->options.segmentation_key,
                                    *db_segment_info);
  PostResultCallback(
      std::move(request_state),
      std::make_unique<SegmentResult>(ResultState::kSuccessFromDatabase, rank));
}

void SegmentResultProviderImpl::TryExecuteModelAndGetScore(
    std::unique_ptr<RequestState> request_state,
    DefaultModelManager::SegmentInfoList available_segments) {
  auto db_segment_info = std::make_unique<proto::SegmentInfo>();
  proto::SegmentInfo* segment_info =
      GetSegmentInfo(available_segments, /*default_model=*/false);
  // Note: The db segment info in `available_segments` is no longer usable.
  db_segment_info->Swap(segment_info);
  DCHECK(db_segment_info);

  auto request = std::make_unique<ExecutionRequest>();
  // The pointer is kept alive by the unique_ptr in the callback.
  request->segment_info = db_segment_info.get();
  request->model_provider =
      execution_service_->GetModelProvider(db_segment_info->segment_id());
  request->record_metrics_for_default = true;
  request->callback = base::BindOnce(
      &SegmentResultProviderImpl::OnModelExecuted,
      weak_ptr_factory_.GetWeakPtr(), std::move(request_state),
      std::move(db_segment_info), /*is_metrics_for_default=*/false);
  execution_service_->RequestModelExecution(std::move(request));
}

void SegmentResultProviderImpl::TryGetScoreFromDefaultModel(
    std::unique_ptr<RequestState> request_state,
    SegmentResultProvider::ResultState existing_state,
    DefaultModelManager::SegmentInfoList available_segments) {
  if (!request_state->default_provider ||
      !request_state->default_provider->ModelAvailable()) {
    VLOG(1) << __func__
            << ": segment=" << SegmentId_Name(request_state->options.segment_id)
            << " default provider not available";
    PostResultCallback(std::move(request_state),
                       std::make_unique<SegmentResult>(existing_state));
    return;
  }

  proto::SegmentInfo* segment_info =
      GetSegmentInfo(available_segments, /*default_model=*/true);
  if (!segment_info) {
    VLOG(1) << __func__
            << ": segment=" << SegmentId_Name(request_state->options.segment_id)
            << " default segment info not available";
    PostResultCallback(std::move(request_state),
                       std::make_unique<SegmentResult>(
                           ResultState::kDefaultModelMetadataMissing));
    return;
  }

  auto default_segment_info = std::make_unique<proto::SegmentInfo>();
  default_segment_info->Swap(segment_info);

  DCHECK_EQ(
      metadata_utils::ValidationResult::kValidationSuccess,
      metadata_utils::ValidateMetadata(default_segment_info->model_metadata()));
  if (!force_refresh_results_ &&
      !signal_storage_config_->MeetsSignalCollectionRequirement(
          default_segment_info->model_metadata())) {
    VLOG(1) << __func__
            << ": segment=" << SegmentId_Name(request_state->options.segment_id)
            << " signal collection not met";
    PostResultCallback(std::move(request_state),
                       std::make_unique<SegmentResult>(
                           ResultState::kDefaultModelSignalNotCollected));
    return;
  }

  ModelProvider* default_provider = request_state->default_provider;
  DCHECK(default_provider);
  auto request = std::make_unique<ExecutionRequest>();
  // The pointer is kept alive by the unique_ptr in the callback.
  request->segment_info = default_segment_info.get();
  request->record_metrics_for_default = true;
  request->callback = base::BindOnce(
      &SegmentResultProviderImpl::OnModelExecuted,
      weak_ptr_factory_.GetWeakPtr(), std::move(request_state),
      std::move(default_segment_info), /*is_metrics_for_default=*/true);
  request->model_provider = default_provider;
  execution_service_->RequestModelExecution(std::move(request));
}

void SegmentResultProviderImpl::OnModelExecuted(
    std::unique_ptr<RequestState> request_state,
    std::unique_ptr<proto::SegmentInfo> segment_info,
    bool is_metrics_for_default,
    const std::pair<float, ModelExecutionStatus>& result) {
  if (result.second == ModelExecutionStatus::kSuccess) {
    segment_info->mutable_prediction_result()->set_result(result.first);
    int rank = ComputeDiscreteMapping(request_state->options.segmentation_key,
                                      *segment_info);
    ResultState state = is_metrics_for_default
                            ? ResultState::kDefaultModelScoreUsed
                            : ResultState::kTfliteModelScoreUsed;
    PostResultCallback(std::move(request_state),
                       std::make_unique<SegmentResult>(state, rank));
  } else {
    // TODO(ssid): If the real model execution failed, fallback to default
    // execution.
    ResultState state = is_metrics_for_default
                            ? ResultState::kDefaultModelExecutionFailed
                            : ResultState::kTfliteModelExecutionFailed;
    PostResultCallback(std::move(request_state),
                       std::make_unique<SegmentResult>(state));
  }
}

void SegmentResultProviderImpl::PostResultCallback(
    std::unique_ptr<RequestState> request_state,
    std::unique_ptr<SegmentResult> result) {
  task_runner_->PostTask(
      FROM_HERE, base::BindOnce(std::move(request_state->options.callback),
                                std::move(result)));
}

}  // namespace

SegmentResultProvider::SegmentResult::SegmentResult(ResultState state)
    : state(state) {}
SegmentResultProvider::SegmentResult::SegmentResult(ResultState state, int rank)
    : state(state), rank(rank) {}
SegmentResultProvider::SegmentResult::~SegmentResult() = default;

SegmentResultProvider::GetResultOptions::GetResultOptions() = default;
SegmentResultProvider::GetResultOptions::~GetResultOptions() = default;
SegmentResultProvider::GetResultOptions&
SegmentResultProvider::GetResultOptions::operator=(GetResultOptions&&) =
    default;

// static
std::unique_ptr<SegmentResultProvider> SegmentResultProvider::Create(
    SegmentInfoDatabase* segment_database,
    SignalStorageConfig* signal_storage_config,
    DefaultModelManager* default_model_manager,
    ExecutionService* execution_service,
    base::Clock* clock,
    bool force_refresh_results) {
  return std::make_unique<SegmentResultProviderImpl>(
      segment_database, signal_storage_config, default_model_manager,
      execution_service, clock, force_refresh_results);
}

}  // namespace segmentation_platform
