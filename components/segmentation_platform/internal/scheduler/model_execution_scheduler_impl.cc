// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/segmentation_platform/internal/scheduler/model_execution_scheduler_impl.h"

#include "base/threading/thread_task_runner_handle.h"
#include "base/time/time.h"
#include "components/segmentation_platform/internal/database/metadata_utils.h"
#include "components/segmentation_platform/internal/database/segment_info_database.h"
#include "components/segmentation_platform/internal/execution/model_execution_manager.h"

namespace segmentation_platform {

ModelExecutionSchedulerImpl::ModelExecutionSchedulerImpl(
    Observer* observer,
    SegmentInfoDatabase* segment_database,
    ModelExecutionManager* model_execution_manager)
    : observer_(observer),
      segment_database_(segment_database),
      model_execution_manager_(model_execution_manager) {}

ModelExecutionSchedulerImpl::~ModelExecutionSchedulerImpl() = default;

void ModelExecutionSchedulerImpl::OnNewModelInfoReady(
    OptimizationTarget segment_id) {
  // Cancel any outstanding execution request.
  const auto& iter = outstanding_requests_.find(segment_id);
  if (iter != outstanding_requests_.end()) {
    iter->second.Cancel();
    outstanding_requests_.erase(iter);
  }

  RequestModelExecution(segment_id);
}

void ModelExecutionSchedulerImpl::RequestModelExecutionForEligibleSegments(
    bool expired_only) {
  segment_database_->GetAllSegmentInfo(
      base::BindOnce(&ModelExecutionSchedulerImpl::FilterEligibleSegments,
                     weak_ptr_factory_.GetWeakPtr(), expired_only));
}

void ModelExecutionSchedulerImpl::RequestModelExecution(
    OptimizationTarget segment_id) {
  outstanding_requests_.insert(std::make_pair(
      segment_id,
      base::BindOnce(&ModelExecutionSchedulerImpl::OnModelExecutionCompleted,
                     weak_ptr_factory_.GetWeakPtr(), segment_id)));
  model_execution_manager_->ExecuteModel(
      segment_id, outstanding_requests_[segment_id].callback());
}

void ModelExecutionSchedulerImpl::OnModelExecutionCompleted(
    OptimizationTarget segment_id,
    const std::pair<float, ModelExecutionStatus>& result) {
  // TODO(shaktisahu): Check ModelExecutionStatus and handle failure cases.
  // Should we save it to DB?
  proto::PredictionResult segment_result;
  bool success = result.second == ModelExecutionStatus::SUCCESS;
  if (success) {
    segment_result.set_result(result.first);
    segment_result.set_timestamp_us(
        base::Time::Now().ToDeltaSinceWindowsEpoch().InMicroseconds());
  }

  segment_database_->SaveSegmentResult(
      segment_id, success ? &segment_result : nullptr,
      base::BindOnce(&ModelExecutionSchedulerImpl::OnResultSaved,
                     weak_ptr_factory_.GetWeakPtr(), segment_id));
}

void ModelExecutionSchedulerImpl::FilterEligibleSegments(
    bool expired_only,
    std::vector<std::pair<OptimizationTarget, proto::SegmentInfo>>
        all_segments) {
  std::vector<OptimizationTarget> models_to_run;
  for (const auto& pair : all_segments) {
    OptimizationTarget segment_id = pair.first;
    const proto::SegmentInfo& segment_info = pair.second;
    // Filter out the segments computed recently.
    if (metadata_utils::HasFreshResults(segment_info))
      continue;

    // Filter out the segments that aren't expired yet.
    if (expired_only &&
        !metadata_utils::HasExpiredOrUnavailableResult(segment_info))
      continue;

    // TODO(shaktisahu): Filter out segments that don't match signal collection
    // min length.
    models_to_run.emplace_back(segment_id);
  }

  for (OptimizationTarget segment_id : models_to_run)
    RequestModelExecution(segment_id);
}

void ModelExecutionSchedulerImpl::OnResultSaved(OptimizationTarget segment_id,
                                                bool success) {
  if (!success)
    return;

  observer_->OnModelExecutionCompleted(segment_id);
}

}  // namespace segmentation_platform
