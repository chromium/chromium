// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SEGMENTATION_PLATFORM_INTERNAL_EXECUTION_MODEL_EXECUTOR_IMPL_H_
#define COMPONENTS_SEGMENTATION_PLATFORM_INTERNAL_EXECUTION_MODEL_EXECUTOR_IMPL_H_

#include <memory>
#include <optional>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/time/clock.h"
#include "components/segmentation_platform/internal/database/segment_info_database.h"
#include "components/segmentation_platform/internal/execution/execution_request.h"
#include "components/segmentation_platform/internal/execution/model_executor.h"
#include "components/segmentation_platform/public/model_provider.h"

namespace segmentation_platform {

namespace processing {
class FeatureListQueryProcessor;
}

// Uses SignalDatabase (raw signals), and uses a
// processing::FeatureListQueryProcessor for each feature to go from metadata
// and raw signals to create an input tensor to use when executing the ML model.
// It then uses this input tensor to execute the model and returns the result
// through a callback. Uses a state within callbacks for executing multiple
// models simultaneously, or the same model multiple times without waiting for
// the requests to finish.
class ModelExecutorImpl : public ModelExecutor {
 public:
  ModelExecutorImpl(
      base::Clock* clock,
      SegmentInfoDatabase* segment_db,
      processing::FeatureListQueryProcessor* feature_list_query_processor);
  ~ModelExecutorImpl() override;

  ModelExecutorImpl(const ModelExecutorImpl&) = delete;
  ModelExecutorImpl& operator=(const ModelExecutorImpl&) = delete;

  // ModelExecutor override.
  void ExecuteModel(std::unique_ptr<ExecutionRequest> request) override;

 private:
  struct ExecutionState;
  struct ModelExecutionTraceEvent;

  // Callback method for when the processing of the model metadata's feature
  // list has completed, which either result in an error or a valid input tensor
  // for executing the model.
  void OnProcessingFeatureListComplete(
      std::unique_ptr<ExecutionState> state,
      bool error,
      const ModelProvider::Request& input_tensor,
      const ModelProvider::Response& output_tensor);

  // ExecuteModel takes the current input tensor and passes it to the ML
  // model for execution.
  void ExecuteModel(std::unique_ptr<ExecutionState> state);

  // Callback method for when the model execution has completed which gives
  // the end result to the initial ModelExecutionCallback passed to
  // ExecuteModel(...).
  void OnModelExecutionComplete(
      std::unique_ptr<ExecutionState> state,
      const std::optional<ModelProvider::Response>& result);

  // Helper function for synchronously invoking the callback with the given
  // result and status. Before invoking this, it is required to move the
  // ExecutionState::callback out as a separate parameter, e.g.:
  // `RunModelExecutionCallback(*state, std::move(state->callback), ...)`.
  void RunModelExecutionCallback(const ExecutionState& state,
                                 ModelExecutionCallback callback,
                                 std::unique_ptr<ModelExecutionResult> result);

  const raw_ptr<base::Clock> clock_;

  const raw_ptr<SegmentInfoDatabase> segment_db_;

  // Feature list processor for processing a model metadata's feature list.
  const raw_ptr<processing::FeatureListQueryProcessor>
      feature_list_query_processor_;

  base::WeakPtrFactory<ModelExecutorImpl> weak_ptr_factory_{this};
};

}  // namespace segmentation_platform

#endif  // COMPONENTS_SEGMENTATION_PLATFORM_INTERNAL_EXECUTION_MODEL_EXECUTOR_IMPL_H_
