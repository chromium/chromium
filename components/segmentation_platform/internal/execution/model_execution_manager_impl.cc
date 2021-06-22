// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/segmentation_platform/internal/execution/model_execution_manager_impl.h"

#include <deque>
#include <map>
#include <memory>
#include <vector>

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/location.h"
#include "base/sequenced_task_runner.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "base/time/time.h"
#include "components/optimization_guide/core/model_executor.h"
#include "components/optimization_guide/proto/models.pb.h"
#include "components/segmentation_platform/internal/database/metadata_utils.h"
#include "components/segmentation_platform/internal/execution/model_execution_manager.h"
#include "components/segmentation_platform/internal/execution/model_execution_status.h"
#include "components/segmentation_platform/internal/execution/segmentation_model_handler.h"
#include "components/segmentation_platform/internal/proto/model_metadata.pb.h"
#include "components/segmentation_platform/internal/proto/model_prediction.pb.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace optimization_guide {
class OptimizationGuideModelProvider;
using proto::OptimizationTarget;
}  // namespace optimization_guide

namespace segmentation_platform {

struct ModelExecutionManagerImpl::ExecutionState {
  ExecutionState() = default;
  ~ExecutionState() = default;

  // Disallow copy/assign.
  ExecutionState(const ExecutionState&) = delete;
  ExecutionState& operator=(const ExecutionState&) = delete;

  OptimizationTarget segment_id;
  SegmentationModelHandler* model_handler = nullptr;
  ModelExecutionCallback callback;
  base::TimeDelta bucket_duration;
  std::deque<proto::Feature> features;
};

ModelExecutionManagerImpl::ModelExecutionManagerImpl(
    optimization_guide::OptimizationGuideModelProvider* model_provider,
    scoped_refptr<base::SequencedTaskRunner> background_task_runner,
    std::vector<OptimizationTarget> segment_ids,
    SegmentInfoDatabase* segment_database)
    : segment_database_(segment_database) {
  for (OptimizationTarget segment_id : segment_ids) {
    model_handlers_.emplace(std::make_pair(
        segment_id, std::make_unique<SegmentationModelHandler>(
                        model_provider, background_task_runner, segment_id)));
  }
}

ModelExecutionManagerImpl::~ModelExecutionManagerImpl() = default;

void ModelExecutionManagerImpl::ExecuteModel(OptimizationTarget segment_id,
                                             ModelExecutionCallback callback) {
  auto model_handler_it = model_handlers_.find(segment_id);
  DCHECK(model_handler_it != model_handlers_.end());

  auto state = std::make_unique<ExecutionState>();
  state->segment_id = segment_id;
  state->model_handler = (*model_handler_it).second.get();
  state->callback = std::move(callback);

  segment_database_->GetSegmentInfo(
      segment_id,
      base::BindOnce(&ModelExecutionManagerImpl::OnSegmentInfoFetched,
                     weak_ptr_factory_.GetWeakPtr(), std::move(state)));
}

void ModelExecutionManagerImpl::OnSegmentInfoFetched(
    std::unique_ptr<ExecutionState> state,
    absl::optional<proto::SegmentInfo> segment_info) {
  if (!segment_info || metadata_utils::ValidateSegmentInfo(*segment_info) !=
                           metadata_utils::VALIDATION_SUCCESS) {
    RunModelExecutionCallback(std::move(state->callback), 0,
                              ModelExecutionStatus::INVALID_METADATA);
    return;
  }

  const auto& model_metadata = segment_info->model_metadata();
  state->bucket_duration = metadata_utils::GetTimeUnit(model_metadata);

  for (int i = 0; i < model_metadata.features_size(); ++i)
    state->features.push_back(model_metadata.features(i));

  ProcessFeatures(std::move(state));
}

void ModelExecutionManagerImpl::ProcessFeatures(
    std::unique_ptr<ExecutionState> state) {
  // For now, return a fake result. See below for how this code will evolve.
  RunModelExecutionCallback(std::move(state->callback), 1,
                            ModelExecutionStatus::SUCCESS);

  // TOOD(nyquist): Finish the rest of this implementation.
  // Everything should be passed through the callback itself. The following
  // operations should happen in sequence:
  // * Create a vector for the output result, which will become the input
  //   tensor for the ML model.
  // * For each of the ML features, look up enough signal data from the
  //   SignalDatabase, and pass the following to a FeatureCalculator in a
  //   synchronous call:
  //   * The feature metadata.
  //   * All data fetched from the DB for that signal.
  // * Append the resulting vector of data from the calculation to the input
  //   tensor.
  // * Pop the value we just processed, and move on to the next feature.
  // * Continue until no more input features.
  // * Invoke the SegmentationModelHandler.
  // * Return the result.
  // * All state will go away at this point, since everything is owned by the
  //   ExecutorState.
}

void ModelExecutionManagerImpl::RunModelExecutionCallback(
    ModelExecutionCallback callback,
    float result,
    ModelExecutionStatus status) {
  std::move(callback).Run(std::make_pair(result, status));
}

}  // namespace segmentation_platform
