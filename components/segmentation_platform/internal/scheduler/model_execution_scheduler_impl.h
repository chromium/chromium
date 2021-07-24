// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SEGMENTATION_PLATFORM_INTERNAL_SCHEDULER_MODEL_EXECUTION_SCHEDULER_IMPL_H_
#define COMPONENTS_SEGMENTATION_PLATFORM_INTERNAL_SCHEDULER_MODEL_EXECUTION_SCHEDULER_IMPL_H_

#include "components/segmentation_platform/internal/scheduler/model_execution_scheduler.h"

#include "base/cancelable_callback.h"
#include "base/memory/weak_ptr.h"
#include "components/optimization_guide/proto/models.pb.h"
#include "components/segmentation_platform/internal/execution/model_execution_manager.h"
#include "components/segmentation_platform/internal/execution/model_execution_status.h"

namespace segmentation_platform {

namespace proto {
class SegmentInfo;
}  // namespace proto

class SegmentInfoDatabase;
class SignalStorageConfig;

class ModelExecutionSchedulerImpl : public ModelExecutionScheduler {
 public:
  ModelExecutionSchedulerImpl(Observer* observer,
                              SegmentInfoDatabase* segment_database,
                              SignalStorageConfig* signal_storage_config,
                              ModelExecutionManager* model_execution_manager);
  ~ModelExecutionSchedulerImpl() override;

  // Disallow copy/assign.
  ModelExecutionSchedulerImpl(const ModelExecutionSchedulerImpl&) = delete;
  ModelExecutionSchedulerImpl& operator=(const ModelExecutionSchedulerImpl&) =
      delete;

  // ModelExecutionScheduler overrides.
  void OnNewModelInfoReady(const proto::SegmentInfo& segment_info) override;
  void RequestModelExecutionForEligibleSegments(bool expired_only) override;
  void RequestModelExecution(OptimizationTarget segment_id) override;
  void OnModelExecutionCompleted(
      OptimizationTarget segment_id,
      const std::pair<float, ModelExecutionStatus>& score) override;

 private:
  void FilterEligibleSegments(
      bool expired_only,
      std::vector<std::pair<OptimizationTarget, proto::SegmentInfo>>
          all_segments);
  bool ShouldExecuteSegment(bool expired_only,
                            const proto::SegmentInfo& segment_info);
  void CancelOutstandingExecutionRequests(OptimizationTarget segment_id);

  void OnResultSaved(OptimizationTarget segment_id, bool success);

  // Observer listening to model exeuction events. Required by the segment
  // selection pipeline.
  Observer* observer_;

  // The database storing metadata and results.
  SegmentInfoDatabase* segment_database_;

  // Used for confirming if the signals have been collected long enough.
  SignalStorageConfig* signal_storage_config_;

  // The class that executes the models.
  ModelExecutionManager* model_execution_manager_;

  // In-flight model execution requests. Will be killed if we get a model
  // update.
  std::map<OptimizationTarget,
           base::CancelableOnceCallback<
               ModelExecutionManager::ModelExecutionCallback::RunType>>
      outstanding_requests_;

  base::WeakPtrFactory<ModelExecutionSchedulerImpl> weak_ptr_factory_{this};
};

}  // namespace segmentation_platform

#endif  // COMPONENTS_SEGMENTATION_PLATFORM_INTERNAL_SCHEDULER_MODEL_EXECUTION_SCHEDULER_IMPL_H_
