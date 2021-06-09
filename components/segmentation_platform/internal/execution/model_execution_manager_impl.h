// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SEGMENTATION_PLATFORM_INTERNAL_EXECUTION_MODEL_EXECUTION_MANAGER_IMPL_H_
#define COMPONENTS_SEGMENTATION_PLATFORM_INTERNAL_EXECUTION_MODEL_EXECUTION_MANAGER_IMPL_H_

#include <map>
#include <memory>
#include <vector>

#include "base/memory/weak_ptr.h"
#include "base/sequenced_task_runner.h"
#include "components/optimization_guide/core/model_executor.h"
#include "components/optimization_guide/proto/models.pb.h"
#include "components/segmentation_platform/internal/database/segment_info_database.h"
#include "components/segmentation_platform/internal/execution/model_execution_manager.h"
#include "components/segmentation_platform/internal/execution/segmentation_model_handler.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace optimization_guide {
class OptimizationGuideModelProvider;
using proto::OptimizationTarget;
}  // namespace optimization_guide

namespace segmentation_platform {
namespace proto {
class SegmentInfo;
}  // namespace proto

// The ModelExecutionManagerImpl is the core implementation of the
// ModelExecutionManager that hooks up the SegmentInfoDatabase (metadata) and
// SignalDatabase (raw signals) databases, and uses a FeatureCalculator for each
// feature to go from metadata and raw signals to create an input tensor to use
// when executing the ML model. It then uses this input tensor to execute the
// model and returns the result through a callback.
// This class is implemented by having a long chain of callbacks and storing all
// necessary state as part of an ExecutionState struct. This simplifies state
// management, particularly in the case of executing multiple models
// simultaneously, or the same model multiple times without waiting for the
// requests to finish.
// The vector of OptimizationTargets need to be passed in at construction time
// so the SegmentationModelHandler instances can be created early.
class ModelExecutionManagerImpl : public ModelExecutionManager {
 public:
  explicit ModelExecutionManagerImpl(
      optimization_guide::OptimizationGuideModelProvider* model_provider,
      scoped_refptr<base::SequencedTaskRunner> background_task_runner,
      std::vector<OptimizationTarget> segment_ids,
      SegmentInfoDatabase* segment_database);
  ~ModelExecutionManagerImpl() override;

  // Disallow copy/assign.
  ModelExecutionManagerImpl(const ModelExecutionManagerImpl&) = delete;
  ModelExecutionManagerImpl& operator=(const ModelExecutionManagerImpl&) =
      delete;

  // ModelExecutionManager overrides.
  void ExecuteModel(optimization_guide::proto::OptimizationTarget segment_id,
                    ModelExecutionCallback callback) override;

 private:
  struct ExecutionState;

  void OnSegmentInfoFetched(std::unique_ptr<ExecutionState> state,
                            absl::optional<proto::SegmentInfo> segment_info);
  void ProcessFeatures(std::unique_ptr<ExecutionState> state);

  void RunModelExecutionCallback(ModelExecutionCallback callback,
                                 float result,
                                 ModelExecutionStatus status);

  std::map<OptimizationTarget, std::unique_ptr<SegmentationModelHandler>>
      model_handlers_;
  SegmentInfoDatabase* segment_database_;
  base::WeakPtrFactory<ModelExecutionManagerImpl> weak_ptr_factory_{this};
};

}  // namespace segmentation_platform

#endif  // COMPONENTS_SEGMENTATION_PLATFORM_INTERNAL_EXECUTION_MODEL_EXECUTION_MANAGER_IMPL_H_
