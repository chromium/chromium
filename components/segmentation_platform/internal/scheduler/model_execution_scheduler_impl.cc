// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/segmentation_platform/internal/scheduler/model_execution_scheduler_impl.h"

#include <optional>

#include "base/logging.h"
#include "base/memory/raw_ptr.h"
#include "base/time/clock.h"
#include "components/segmentation_platform/internal/database/segment_info_database.h"
#include "components/segmentation_platform/internal/database/signal_storage_config.h"
#include "components/segmentation_platform/internal/execution/execution_request.h"
#include "components/segmentation_platform/internal/execution/model_manager_impl.h"
#include "components/segmentation_platform/internal/metadata/metadata_utils.h"
#include "components/segmentation_platform/internal/platform_options.h"
#include "components/segmentation_platform/internal/stats.h"
#include "components/segmentation_platform/public/model_provider.h"
#include "components/segmentation_platform/public/proto/segmentation_platform.pb.h"

namespace segmentation_platform {

ModelExecutionSchedulerImpl::ModelExecutionSchedulerImpl(
    std::vector<raw_ptr<Observer, VectorExperimental>>&& observers,
    SegmentInfoDatabase* segment_database,
    SignalStorageConfig* signal_storage_config,
    ModelManager* model_manager,
    ModelExecutor* model_executor,
    base::flat_set<proto::SegmentId> segment_ids,
    base::Clock* clock,
    const PlatformOptions& platform_options)
    : observers_(observers),
      segment_database_(segment_database),
      signal_storage_config_(signal_storage_config),
      model_manager_(model_manager),
      model_executor_(model_executor),
      legacy_output_segment_ids_(std::move(segment_ids)),
      clock_(clock),
      platform_options_(platform_options) {}

ModelExecutionSchedulerImpl::~ModelExecutionSchedulerImpl() = default;

void ModelExecutionSchedulerImpl::OnNewModelInfoReady(
    const proto::SegmentInfo& segment_info) {
  DCHECK(metadata_utils::ValidateSegmentInfoMetadataAndFeatures(segment_info) ==
         metadata_utils::ValidationResult::kValidationSuccess);

  if (!ShouldExecuteSegment(/*expired_only=*/true, segment_info)) {
    // We usually cancel any outstanding requests right before executing the
    // model, but in this case we alreday know that 1) we got a new model, and
    // b) the new model is not yet valid for execution. Therefore, we cancel
    // the current execution and we will have to execute this model later.
    CancelOutstandingExecutionRequests(segment_info.segment_id());
    return;
  }

  RequestModelExecution(segment_info);
}

void ModelExecutionSchedulerImpl::RequestModelExecutionForEligibleSegments(
    bool expired_only) {
  segment_database_->GetSegmentInfoForSegments(
      legacy_output_segment_ids_,
      base::BindOnce(&ModelExecutionSchedulerImpl::FilterEligibleSegments,
                     weak_ptr_factory_.GetWeakPtr(), expired_only));
}

void ModelExecutionSchedulerImpl::RequestModelExecution(
    const proto::SegmentInfo& segment_info) {
  SegmentId segment_id = segment_info.segment_id();
  CancelOutstandingExecutionRequests(segment_id);
  outstanding_requests_.insert(std::make_pair(
      segment_id,
      base::BindOnce(&ModelExecutionSchedulerImpl::OnModelExecutionCompleted,
                     weak_ptr_factory_.GetWeakPtr(), segment_info)));
  auto request = std::make_unique<ExecutionRequest>();
  request->segment_id = segment_info.segment_id();
  request->model_source = proto::ModelSource::SERVER_MODEL_SOURCE;
  request->model_provider = model_manager_->GetModelProvider(
      segment_info.segment_id(), proto::ModelSource::SERVER_MODEL_SOURCE);
  DCHECK(request->model_provider);
  request->callback = outstanding_requests_[segment_id].callback();
  model_executor_->ExecuteModel(std::move(request));
}

void ModelExecutionSchedulerImpl::OnModelExecutionCompleted(
    const proto::SegmentInfo& segment_info,
    std::unique_ptr<ModelExecutionResult> result) {
  // TODO(shaktisahu): Check ModelExecutionStatus and handle failure cases.
  // Should we save it to DB?
  SegmentId segment_id = segment_info.segment_id();
  proto::PredictionResult segment_result;
  bool success = result->status == ModelExecutionStatus::kSuccess;
  if (success) {
    segment_result = metadata_utils::CreatePredictionResult(
        result->scores, segment_info.model_metadata().output_config(),
        clock_->Now(), segment_info.model_version());
  }

  segment_database_->SaveSegmentResult(
      segment_id, proto::ModelSource::SERVER_MODEL_SOURCE,
      success ? std::make_optional(segment_result) : std::nullopt,
      base::BindOnce(&ModelExecutionSchedulerImpl::OnResultSaved,
                     weak_ptr_factory_.GetWeakPtr(), segment_id));
}

void ModelExecutionSchedulerImpl::FilterEligibleSegments(
    bool expired_only,
    std::unique_ptr<SegmentInfoDatabase::SegmentInfoList> all_segments) {
  std::vector<const proto::SegmentInfo*> models_to_run;
  for (const auto& pair : *all_segments) {
    SegmentId segment_id = pair.first;
    const proto::SegmentInfo& segment_info = *pair.second;
    if (!ShouldExecuteSegment(expired_only, segment_info)) {
      VLOG(1) << "Segmentation scheduler: Skipped executed segment "
              << proto::SegmentId_Name(segment_id);
      continue;
    }

    models_to_run.emplace_back(&segment_info);
  }

  for (const proto::SegmentInfo* segment_info : models_to_run)
    RequestModelExecution(*segment_info);
}

bool ModelExecutionSchedulerImpl::ShouldExecuteSegment(
    bool expired_only,
    const proto::SegmentInfo& segment_info) {
  if (platform_options_.force_refresh_results)
    return true;

  // Filter out the segments computed recently.
  if (metadata_utils::HasFreshResults(segment_info, clock_->Now())) {
    VLOG(1) << "Segmentation model not executed since it has fresh results, "
               "segment:"
            << proto::SegmentId_Name(segment_info.segment_id());
    stats::RecordModelExecutionStatus(
        segment_info.segment_id(),
        /*default_provider=*/false,
        ModelExecutionStatus::kSkippedHasFreshResults);
    return false;
  }

  // Filter out the segments that aren't expired yet.
  if (expired_only && !metadata_utils::HasExpiredOrUnavailableResult(
                          segment_info, clock_->Now())) {
    VLOG(1) << "Segmentation model not executed since results are not expired, "
               "segment:"
            << proto::SegmentId_Name(segment_info.segment_id());
    stats::RecordModelExecutionStatus(
        segment_info.segment_id(),
        /*default_provider=*/false,
        ModelExecutionStatus::kSkippedResultNotExpired);
    return false;
  }

  // Filter out segments that don't match signal collection min length.
  if (!signal_storage_config_->MeetsSignalCollectionRequirement(
          segment_info.model_metadata())) {
    stats::RecordModelExecutionStatus(
        segment_info.segment_id(),
        /*default_provider=*/false,
        ModelExecutionStatus::kSkippedNotEnoughSignals);
    VLOG(1) << "Segmentation model not executed since metadata requirements "
               "not met, segment:"
            << proto::SegmentId_Name(segment_info.segment_id());
    return false;
  }

  return true;
}

void ModelExecutionSchedulerImpl::CancelOutstandingExecutionRequests(
    SegmentId segment_id) {
  const auto& iter = outstanding_requests_.find(segment_id);
  if (iter != outstanding_requests_.end()) {
    iter->second.Cancel();
    outstanding_requests_.erase(iter);
  }
}

void ModelExecutionSchedulerImpl::OnResultSaved(SegmentId segment_id,
                                                bool success) {
  stats::RecordModelExecutionSaveResult(segment_id, success);
  if (!success) {
    // TODO(ssid): Consider removing this enum, this is the only case where the
    // execution status is recorded twice for the same execution request.
    stats::RecordModelExecutionStatus(
        segment_id,
        /*default_provider=*/false,
        ModelExecutionStatus::kFailedToSaveResultAfterSuccess);
    return;
  }

  for (Observer* observer : observers_)
    observer->OnModelExecutionCompleted(segment_id);
}

}  // namespace segmentation_platform
