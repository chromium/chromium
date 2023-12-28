// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SEGMENTATION_PLATFORM_INTERNAL_SCHEDULER_MODEL_EXECUTION_SCHEDULER_IMPL_H_
#define COMPONENTS_SEGMENTATION_PLATFORM_INTERNAL_SCHEDULER_MODEL_EXECUTION_SCHEDULER_IMPL_H_

#include "base/memory/raw_ptr.h"
#include "components/segmentation_platform/internal/scheduler/model_execution_scheduler.h"

#include "base/cancelable_callback.h"
#include "base/containers/flat_set.h"
#include "base/memory/weak_ptr.h"
#include "components/segmentation_platform/internal/database/segment_info_database.h"
#include "components/segmentation_platform/internal/execution/model_execution_status.h"
#include "components/segmentation_platform/internal/execution/model_executor.h"
#include "components/segmentation_platform/internal/platform_options.h"
#include "components/segmentation_platform/public/proto/segmentation_platform.pb.h"

namespace base {
class Clock;
}  // namespace base

namespace segmentation_platform {

namespace proto {
class SegmentInfo;
}  // namespace proto

class ModelManager;
class SignalStorageConfig;

class ModelExecutionSchedulerImpl : public ModelExecutionScheduler {
 public:
  ModelExecutionSchedulerImpl(
      std::vector<raw_ptr<Observer, VectorExperimental>>&& observers,
      SegmentInfoDatabase* segment_database,
      SignalStorageConfig* signal_storage_config,
      ModelManager* model_manager,
      ModelExecutor* model_executor,
      base::flat_set<proto::SegmentId> segment_ids,
      base::Clock* clock,
      const PlatformOptions& platform_options);
  ~ModelExecutionSchedulerImpl() override;

  // Disallow copy/assign.
  ModelExecutionSchedulerImpl(const ModelExecutionSchedulerImpl&) = delete;
  ModelExecutionSchedulerImpl& operator=(const ModelExecutionSchedulerImpl&) =
      delete;

  // ModelExecutionScheduler overrides.
  void OnNewModelInfoReady(const proto::SegmentInfo& segment_info) override;
  void RequestModelExecutionForEligibleSegments(bool expired_only) override;
  void RequestModelExecution(const proto::SegmentInfo& segment_info) override;
  void OnModelExecutionCompleted(
      const proto::SegmentInfo& segment_info,
      std::unique_ptr<ModelExecutionResult> score) override;

 private:
  void FilterEligibleSegments(
      bool expired_only,
      std::unique_ptr<SegmentInfoDatabase::SegmentInfoList> all_segments);
  bool ShouldExecuteSegment(bool expired_only,
                            const proto::SegmentInfo& segment_info);
  void CancelOutstandingExecutionRequests(SegmentId segment_id);

  void OnResultSaved(SegmentId segment_id, bool success);

  // Observers listening to model exeuction events. Required by the segment
  // selection pipeline.
  std::vector<raw_ptr<Observer, VectorExperimental>> observers_;

  // The database storing metadata and results.
  const raw_ptr<SegmentInfoDatabase> segment_database_;

  // Used for confirming if the signals have been collected long enough.
  const raw_ptr<SignalStorageConfig> signal_storage_config_;

  // The class that executes the models.
  const raw_ptr<ModelManager> model_manager_;
  const raw_ptr<ModelExecutor> model_executor_;

  // The set of all known segments.
  base::flat_set<proto::SegmentId> legacy_output_segment_ids_;

  // The time provider.
  raw_ptr<base::Clock> clock_;

  const PlatformOptions platform_options_;

  // In-flight model execution requests. Will be killed if we get a model
  // update.
  // TODO(ritikagup) : Remove outstanding request handling if not required.
  std::map<SegmentId,
           base::CancelableOnceCallback<
               ModelExecutor::ModelExecutionCallback::RunType>>
      outstanding_requests_;

  base::WeakPtrFactory<ModelExecutionSchedulerImpl> weak_ptr_factory_{this};
};

}  // namespace segmentation_platform

#endif  // COMPONENTS_SEGMENTATION_PLATFORM_INTERNAL_SCHEDULER_MODEL_EXECUTION_SCHEDULER_IMPL_H_
