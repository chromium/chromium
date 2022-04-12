// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/segmentation_platform/internal/selection/segment_result_provider.h"

#include <map>

#include "base/logging.h"
#include "base/memory/raw_ptr.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "components/segmentation_platform/internal/database/metadata_utils.h"
#include "components/segmentation_platform/internal/database/segment_info_database.h"
#include "components/segmentation_platform/internal/database/signal_storage_config.h"
#include "components/segmentation_platform/internal/execution/default_model_manager.h"
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
          << ": segment=" << OptimizationTarget_Name(segment_info.segment_id())
          << ": result=" << segment_info.prediction_result().result()
          << ", rank=" << rank;

  return rank;
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

  void GetSegmentResult(OptimizationTarget segment_id,
                        const std::string& segmentation_key,
                        SegmentResultCallback callback) override;

  SegmentResultProviderImpl(SegmentResultProviderImpl&) = delete;
  SegmentResultProviderImpl& operator=(SegmentResultProviderImpl&) = delete;

 private:
  struct RequestState {
    OptimizationTarget segment_id;
    SegmentResultCallback callback;
    raw_ptr<ModelProvider> default_provider;
    std::string segmentation_key;
  };

  void OnGetSegmentInfo(
      std::unique_ptr<RequestState> request_state,
      DefaultModelManager::SegmentInfoList available_segments);

  void TryGetScoreFromDefaultModel(
      std::unique_ptr<RequestState> request_state,
      SegmentResultProvider::ResultState existing_state,
      DefaultModelManager::SegmentInfoList available_segments);
  void OnDefaultModelExecuted(
      std::unique_ptr<RequestState> request_state,
      std::unique_ptr<proto::SegmentInfo> segment_info,
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

void SegmentResultProviderImpl::GetSegmentResult(
    OptimizationTarget segment_id,
    const std::string& segmentation_key,
    SegmentResultCallback callback) {
  auto request_state = std::make_unique<RequestState>();
  request_state = std::make_unique<RequestState>();
  request_state->segment_id = segment_id;
  request_state->segmentation_key = segmentation_key;
  request_state->callback = std::move(callback);
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
  const proto::SegmentInfo* db_segment_info = nullptr;
  for (const auto& info : available_segments) {
    DCHECK_EQ(request_state->segment_id, info->segment_info.segment_id());
    if (info->segment_source == DefaultModelManager::SegmentSource::DATABASE) {
      db_segment_info = &info->segment_info;
      break;
    }
  }

  // Don't compute results if we don't have enough signals, or don't have
  // valid unexpired results for any of the segments.
  if (!db_segment_info) {
    VLOG(1) << __func__ << ": segment="
            << OptimizationTarget_Name(request_state->segment_id)
            << " does not have segment info.";
    TryGetScoreFromDefaultModel(std::move(request_state),
                                ResultState::kSegmentNotAvailable,
                                std::move(available_segments));
    return;
  }

  // TODO(ssid): Remove this check since scheduler does this before executing
  // the model.
  if (!force_refresh_results_ &&
      !signal_storage_config_->MeetsSignalCollectionRequirement(
          db_segment_info->model_metadata())) {
    VLOG(1) << __func__ << ": segment="
            << OptimizationTarget_Name(db_segment_info->segment_id())
            << " does not meet signal collection requirements.";
    TryGetScoreFromDefaultModel(std::move(request_state),
                                ResultState::kSignalsNotCollected,
                                std::move(available_segments));
    return;
  }

  if (metadata_utils::HasExpiredOrUnavailableResult(*db_segment_info,
                                                    clock_->Now())) {
    VLOG(1) << __func__ << ": segment="
            << OptimizationTarget_Name(db_segment_info->segment_id())
            << " has expired or unavailable result.";
    TryGetScoreFromDefaultModel(std::move(request_state),
                                ResultState::kDatabaseScoreNotReady,
                                std::move(available_segments));
    return;
  }

  int rank =
      ComputeDiscreteMapping(request_state->segmentation_key, *db_segment_info);
  PostResultCallback(
      std::move(request_state),
      std::make_unique<SegmentResult>(ResultState::kSuccessFromDatabase, rank));
}

void SegmentResultProviderImpl::TryGetScoreFromDefaultModel(
    std::unique_ptr<RequestState> request_state,
    SegmentResultProvider::ResultState existing_state,
    DefaultModelManager::SegmentInfoList available_segments) {
  if (!request_state->default_provider ||
      !request_state->default_provider->ModelAvailable()) {
    VLOG(1) << __func__ << ": segment="
            << OptimizationTarget_Name(request_state->segment_id)
            << " default provider not available";
    PostResultCallback(std::move(request_state),
                       std::make_unique<SegmentResult>(existing_state));
    return;
  }

  std::unique_ptr<proto::SegmentInfo> default_segment_info;
  for (auto& info : available_segments) {
    DCHECK_EQ(request_state->segment_id, info->segment_info.segment_id());
    if (info->segment_source ==
        DefaultModelManager::SegmentSource::DEFAULT_MODEL) {
      default_segment_info = std::make_unique<proto::SegmentInfo>();
      default_segment_info->Swap(&info->segment_info);
      break;
    }
  }

  if (!default_segment_info) {
    VLOG(1) << __func__ << ": segment="
            << OptimizationTarget_Name(request_state->segment_id)
            << " default segment info not available";
    PostResultCallback(std::move(request_state),
                       std::make_unique<SegmentResult>(
                           ResultState::kDefaultModelMetadataMissing));
    return;
  }

  DCHECK_EQ(
      metadata_utils::ValidationResult::kValidationSuccess,
      metadata_utils::ValidateMetadata(default_segment_info->model_metadata()));
  if (!signal_storage_config_->MeetsSignalCollectionRequirement(
          default_segment_info->model_metadata())) {
    VLOG(1) << __func__ << ": segment="
            << OptimizationTarget_Name(request_state->segment_id)
            << " signal collection not met";
    PostResultCallback(std::move(request_state),
                       std::make_unique<SegmentResult>(
                           ResultState::kDefaultModelSignalNotCollected));
    return;
  }

  ModelProvider* default_provider = request_state->default_provider;
  DCHECK(default_provider);
  auto request = std::make_unique<ExecutionService::ExecutionRequest>();
  // The pointer is kept alive by the unique_ptr in the callback.
  request->segment_info = default_segment_info.get();
  request->record_metrics_for_default = true;
  request->callback =
      base::BindOnce(&SegmentResultProviderImpl::OnDefaultModelExecuted,
                     weak_ptr_factory_.GetWeakPtr(), std::move(request_state),
                     std::move(default_segment_info));
  request->model_provider = default_provider;
  execution_service_->RequestModelExecution(std::move(request));
}

void SegmentResultProviderImpl::OnDefaultModelExecuted(
    std::unique_ptr<RequestState> request_state,
    std::unique_ptr<proto::SegmentInfo> segment_info,
    const std::pair<float, ModelExecutionStatus>& result) {
  if (result.second == ModelExecutionStatus::kSuccess) {
    segment_info->mutable_prediction_result()->set_result(result.first);
    int rank =
        ComputeDiscreteMapping(request_state->segmentation_key, *segment_info);
    PostResultCallback(std::move(request_state),
                       std::make_unique<SegmentResult>(
                           ResultState::kDefaultModelScoreUsed, rank));
  } else {
    PostResultCallback(std::move(request_state),
                       std::make_unique<SegmentResult>(
                           ResultState::kDefaultModelExecutionFailed));
  }
}

void SegmentResultProviderImpl::PostResultCallback(
    std::unique_ptr<RequestState> request_state,
    std::unique_ptr<SegmentResult> result) {
  task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(std::move(request_state->callback), std::move(result)));
}

}  // namespace

SegmentResultProvider::SegmentResult::SegmentResult(ResultState state)
    : state(state) {}
SegmentResultProvider::SegmentResult::SegmentResult(ResultState state, int rank)
    : state(state), rank(rank) {}
SegmentResultProvider::SegmentResult::~SegmentResult() = default;

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
