// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/segmentation_platform/internal/execution/model_executor_impl.h"
#include <memory>

#include "base/callback.h"
#include "base/logging.h"
#include "base/time/clock.h"
#include "base/time/time.h"
#include "base/trace_event/typed_macros.h"
#include "components/segmentation_platform/internal/execution/execution_request.h"
#include "components/segmentation_platform/internal/execution/processing/feature_list_query_processor.h"
#include "components/segmentation_platform/internal/segmentation_ukm_helper.h"
#include "components/segmentation_platform/internal/stats.h"
#include "components/segmentation_platform/public/model_provider.h"
#include "components/segmentation_platform/public/proto/segmentation_platform.pb.h"
#include "third_party/perfetto/include/perfetto/tracing/track.h"

namespace segmentation_platform {
namespace {
using processing::FeatureListQueryProcessor;
using proto::SegmentId;
}  // namespace

struct ModelExecutorImpl::ModelExecutionTraceEvent {
  ModelExecutionTraceEvent(const char* event_name,
                           const ModelExecutorImpl::ExecutionState& state);
  ~ModelExecutionTraceEvent();

  const ModelExecutorImpl::ExecutionState& state;
};

struct ModelExecutorImpl::ExecutionState {
  ExecutionState()
      : trace_event(std::make_unique<ModelExecutionTraceEvent>(
            "ModelExecutorImpl::ExecutionState",
            *this)) {}
  ~ExecutionState() {
    trace_event.reset();
    // Emit another event to ensure that the event emitted by resetting
    // trace_event can be scraped by the tracing service (crbug.com/1021571).
    TRACE_EVENT_INSTANT("segmentation_platform",
                        "ModelExecutorImpl::~ExecutionState()");
  }

  // Disallow copy/assign.
  ExecutionState(const ExecutionState&) = delete;
  ExecutionState& operator=(const ExecutionState&) = delete;

  // The top level event for all ExecuteModel calls is the ExecutionState
  // trace event. This is std::unique_ptr to be able to easily reset it right
  // before we emit an instant event at destruction time. If this is the last
  // trace event for a thread, it will not be emitted. See
  // https://crbug.com/1021571.
  std::unique_ptr<ModelExecutionTraceEvent> trace_event;

  SegmentId segment_id;
  int64_t model_version = 0;
  raw_ptr<ModelProvider> model_provider = nullptr;
  bool record_metrics_for_default = false;
  ModelExecutionCallback callback;
  std::vector<float> input_tensor;
  base::Time total_execution_start_time;
  base::Time model_execution_start_time;
  base::TimeDelta signal_storage_length;
  bool upload_tensors;
};

ModelExecutorImpl::ModelExecutionTraceEvent::ModelExecutionTraceEvent(
    const char* event_name,
    const ModelExecutorImpl::ExecutionState& state)
    : state(state) {
  TRACE_EVENT_BEGIN("segmentation_platform", perfetto::StaticString(event_name),
                    perfetto::Track::FromPointer(&state));
}

ModelExecutorImpl::ModelExecutionTraceEvent::~ModelExecutionTraceEvent() {
  TRACE_EVENT_END("segmentation_platform",
                  perfetto::Track::FromPointer(&state));
}

ModelExecutorImpl::ModelExecutorImpl(
    base::Clock* clock,
    processing::FeatureListQueryProcessor* feature_list_query_processor)
    : clock_(clock),
      feature_list_query_processor_(feature_list_query_processor) {}

ModelExecutorImpl::~ModelExecutorImpl() = default;

void ModelExecutorImpl::ExecuteModel(
    std::unique_ptr<ExecutionRequest> request) {
  const proto::SegmentInfo& segment_info = *request->segment_info;
  SegmentId segment_id = segment_info.segment_id();

  // Create an ExecutionState that will stay with this request until it has been
  // fully processed.
  auto state = std::make_unique<ExecutionState>();
  state->segment_id = segment_id;

  state->model_provider = request->model_provider;
  state->record_metrics_for_default = request->record_metrics_for_default;

  state->callback = std::move(request->callback);
  state->total_execution_start_time = clock_->Now();

  ModelExecutionTraceEvent trace_event("ModelExecutorImpl::ExecuteModel",
                                       *state);

  if (!state->model_provider || !state->model_provider->ModelAvailable()) {
    RunModelExecutionCallback(std::move(state),
                              std::make_unique<ModelExecutionResult>(
                                  ModelExecutionStatus::kSkippedModelNotReady));
    return;
  }

  // It is required to have a valid and well formed segment info.
  if (metadata_utils::ValidateSegmentInfo(segment_info) !=
      metadata_utils::ValidationResult::kValidationSuccess) {
    RunModelExecutionCallback(
        std::move(state), std::make_unique<ModelExecutionResult>(
                              ModelExecutionStatus::kSkippedInvalidMetadata));
    return;
  }

  state->model_version = segment_info.model_version();
  const proto::SegmentationModelMetadata& model_metadata =
      segment_info.model_metadata();
  state->signal_storage_length = model_metadata.signal_storage_length() *
                                 metadata_utils::GetTimeUnit(model_metadata);
  state->upload_tensors =
      SegmentationUkmHelper::GetInstance()->CanUploadTensors(segment_info);
  feature_list_query_processor_->ProcessFeatureList(
      segment_info.model_metadata(), request->input_context, segment_id,
      clock_->Now(), FeatureListQueryProcessor::ProcessOption::kInputsOnly,
      base::BindOnce(&ModelExecutorImpl::OnProcessingFeatureListComplete,
                     weak_ptr_factory_.GetWeakPtr(), std::move(state)));
}

void ModelExecutorImpl::OnProcessingFeatureListComplete(
    std::unique_ptr<ExecutionState> state,
    bool error,
    const std::vector<float>& input_tensor,
    const std::vector<float>& output_tensor) {
  if (error) {
    // Validation error occurred on model's metadata.
    RunModelExecutionCallback(
        std::move(state), std::make_unique<ModelExecutionResult>(
                              ModelExecutionStatus::kSkippedInvalidMetadata));
    return;
  }
  state->input_tensor.insert(state->input_tensor.end(), input_tensor.begin(),
                             input_tensor.end());

  ExecuteModel(std::move(state));
}

void ModelExecutorImpl::ExecuteModel(std::unique_ptr<ExecutionState> state) {
  ModelExecutionTraceEvent trace_event("ModelExecutorImpl::ExecuteModel",
                                       *state);
  if (VLOG_IS_ON(1)) {
    std::stringstream log_input;
    for (unsigned i = 0; i < state->input_tensor.size(); ++i)
      log_input << " feature " << i << ": " << state->input_tensor[i];
    VLOG(1) << "Segmentation model input: " << log_input.str()
            << " for segment " << proto::SegmentId_Name(state->segment_id);
  }
  const std::vector<float>& const_input_tensor = state->input_tensor;
  stats::RecordModelExecutionZeroValuePercent(state->segment_id,
                                              const_input_tensor);
  state->model_execution_start_time = clock_->Now();
  ModelProvider* model = state->model_provider;
  model->ExecuteModelWithInput(
      const_input_tensor,
      base::BindOnce(&ModelExecutorImpl::OnModelExecutionComplete,
                     weak_ptr_factory_.GetWeakPtr(), std::move(state)));
}

void ModelExecutorImpl::OnModelExecutionComplete(
    std::unique_ptr<ExecutionState> state,
    const absl::optional<float>& result) {
  ModelExecutionTraceEvent trace_event(
      "ModelExecutorImpl::OnModelExecutionComplete", *state);
  stats::RecordModelExecutionDurationModel(
      state->segment_id, result.has_value(),
      clock_->Now() - state->model_execution_start_time);
  if (result.has_value()) {
    VLOG(1) << "Segmentation model result: " << *result << " for segment "
            << proto::SegmentId_Name(state->segment_id);
    stats::RecordModelExecutionResult(state->segment_id, result.value());
    if (state->model_version && SegmentationUkmHelper::AllowedToUploadData(
                                    state->signal_storage_length, clock_)) {
      if (state->upload_tensors) {
        SegmentationUkmHelper::GetInstance()->RecordModelExecutionResult(
            state->segment_id, state->model_version, state->input_tensor,
            result.value());
      }
    }
    ModelExecutionResult::Tensor input_tensor = state->input_tensor;
    RunModelExecutionCallback(std::move(state),
                              std::make_unique<ModelExecutionResult>(
                                  std::move(input_tensor), *result));
  } else {
    VLOG(1) << "Segmentation model returned no result for segment "
            << proto::SegmentId_Name(state->segment_id);
    RunModelExecutionCallback(std::move(state),
                              std::make_unique<ModelExecutionResult>(
                                  ModelExecutionStatus::kExecutionError));
  }
}

void ModelExecutorImpl::RunModelExecutionCallback(
    std::unique_ptr<ExecutionState> state,
    std::unique_ptr<ModelExecutionResult> result) {
  stats::RecordModelExecutionDurationTotal(
      state->segment_id, result->status,
      clock_->Now() - state->total_execution_start_time);
  stats::RecordModelExecutionStatus(
      state->segment_id, state->record_metrics_for_default, result->status);
  std::move(state->callback).Run(std::move(result));
}

}  // namespace segmentation_platform
