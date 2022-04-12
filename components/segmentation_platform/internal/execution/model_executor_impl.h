// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SEGMENTATION_PLATFORM_INTERNAL_EXECUTION_MODEL_EXECUTOR_IMPL_H_
#define COMPONENTS_SEGMENTATION_PLATFORM_INTERNAL_EXECUTION_MODEL_EXECUTOR_IMPL_H_

#include <memory>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/time/clock.h"
#include "components/segmentation_platform/internal/execution/model_executor.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace segmentation_platform {

class FeatureListQueryProcessor;

// Uses SignalDatabase (raw signals), and uses a FeatureListQueryProcessor for
// each feature to go from metadata and raw signals to create an input tensor to
// use when executing the ML model. It then uses this input tensor to execute
// the model and returns the result through a callback. Uses a state within
// callbacks for executing multiple models simultaneously, or the same model
// multiple times without waiting for the requests to finish.
class ModelExecutorImpl : public ModelExecutor {
 public:
  ModelExecutorImpl(base::Clock* clock,
                    FeatureListQueryProcessor* feature_list_query_processor);
  ~ModelExecutorImpl() override;

  ModelExecutorImpl(ModelExecutorImpl&) = delete;
  ModelExecutorImpl& operator=(ModelExecutorImpl&) = delete;

  // ModelExecutionManager impl:.
  void ExecuteModel(const proto::SegmentInfo& segment_info,
                    ModelProvider* model_provider,
                    bool record_metrics_for_default,
                    ModelExecutionCallback callback) override;

 private:
  struct ExecutionState;
  struct ModelExecutionTraceEvent;

  // Callback method for when the processing of the model metadata's feature
  // list has completed, which either result in an error or a valid input tensor
  // for executing the model.
  void OnProcessingFeatureListComplete(std::unique_ptr<ExecutionState> state,
                                       bool error,
                                       const std::vector<float>& input_tensor);

  // ExecuteModel takes the current input tensor and passes it to the ML
  // model for execution.
  void ExecuteModel(std::unique_ptr<ExecutionState> state);

  // Callback method for when the model execution has completed which gives
  // the end result to the initial ModelExecutionCallback passed to
  // ExecuteModel(...).
  void OnModelExecutionComplete(std::unique_ptr<ExecutionState> state,
                                const absl::optional<float>& result);

  // Helper function for synchronously invoking the callback with the given
  // result and status.
  void RunModelExecutionCallback(std::unique_ptr<ExecutionState> state,
                                 float result,
                                 ModelExecutionStatus status);

  const raw_ptr<base::Clock> clock_;

  // Feature list processor for processing a model metadata's feature list.
  const raw_ptr<FeatureListQueryProcessor> feature_list_query_processor_;

  base::WeakPtrFactory<ModelExecutorImpl> weak_ptr_factory_{this};
};

}  // namespace segmentation_platform

#endif  // COMPONENTS_SEGMENTATION_PLATFORM_INTERNAL_EXECUTION_MODEL_EXECUTOR_IMPL_H_
