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
#include "base/logging.h"
#include "base/memory/raw_ptr.h"
#include "base/task/sequenced_task_runner.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "base/time/clock.h"
#include "base/time/time.h"
#include "base/trace_event/typed_macros.h"
#include "components/optimization_guide/proto/models.pb.h"
#include "components/segmentation_platform/internal/database/metadata_utils.h"
#include "components/segmentation_platform/internal/database/signal_database.h"
#include "components/segmentation_platform/internal/execution/feature_list_query_processor.h"
#include "components/segmentation_platform/internal/execution/model_execution_manager.h"
#include "components/segmentation_platform/internal/execution/model_execution_status.h"
#include "components/segmentation_platform/internal/proto/aggregation.pb.h"
#include "components/segmentation_platform/internal/proto/model_metadata.pb.h"
#include "components/segmentation_platform/internal/proto/model_prediction.pb.h"
#include "components/segmentation_platform/internal/proto/types.pb.h"
#include "components/segmentation_platform/internal/segmentation_ukm_helper.h"
#include "components/segmentation_platform/internal/stats.h"
#include "components/segmentation_platform/public/model_provider.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/perfetto/include/perfetto/tracing/track.h"

namespace optimization_guide {
class OptimizationGuideModelProvider;
using proto::OptimizationTarget;
}  // namespace optimization_guide

namespace segmentation_platform {
struct ModelExecutionManagerImpl::ModelExecutionTraceEvent {
  ModelExecutionTraceEvent(
      const char* event_name,
      const ModelExecutionManagerImpl::ExecutionState& state);
  ~ModelExecutionTraceEvent();

  const ModelExecutionManagerImpl::ExecutionState& state;
};

struct ModelExecutionManagerImpl::ExecutionState {
  ExecutionState()
      : trace_event(std::make_unique<ModelExecutionTraceEvent>(
            "ModelExecutionManagerImpl::ExecutionState",
            *this)) {}
  ~ExecutionState() {
    trace_event.reset();
    // Emit another event to ensure that the event emitted by resetting
    // trace_event can be scraped by the tracing service (crbug.com/1021571).
    TRACE_EVENT_INSTANT("segmentation_platform",
                        "ModelExecutionManagerImpl::~ExecutionState()");
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

  OptimizationTarget segment_id;
  int64_t model_version = 0;
  raw_ptr<ModelProvider> model_provider = nullptr;
  ModelExecutionCallback callback;
  std::vector<float> input_tensor;
  base::Time total_execution_start_time;
  base::Time model_execution_start_time;
};

ModelExecutionManagerImpl::ModelExecutionTraceEvent::ModelExecutionTraceEvent(
    const char* event_name,
    const ModelExecutionManagerImpl::ExecutionState& state)
    : state(state) {
  TRACE_EVENT_BEGIN("segmentation_platform", perfetto::StaticString(event_name),
                    perfetto::Track::FromPointer(&state));
}

ModelExecutionManagerImpl::ModelExecutionTraceEvent::
    ~ModelExecutionTraceEvent() {
  TRACE_EVENT_END("segmentation_platform",
                  perfetto::Track::FromPointer(&state));
}

ModelExecutionManagerImpl::ModelExecutionManagerImpl(
    const base::flat_set<OptimizationTarget>& segment_ids,
    std::unique_ptr<ModelProviderFactory> model_provider_factory,
    base::Clock* clock,
    SegmentInfoDatabase* segment_database,
    SignalDatabase* signal_database,
    FeatureListQueryProcessor* feature_list_query_processor,
    const SegmentationModelUpdatedCallback& model_updated_callback)
    : clock_(clock),
      segment_database_(segment_database),
      signal_database_(signal_database),
      model_updated_callback_(model_updated_callback) {
  feature_list_query_processor_ = feature_list_query_processor;
  for (OptimizationTarget segment_id : segment_ids) {
    std::unique_ptr<ModelProvider> provider =
        model_provider_factory->CreateProvider(segment_id);
    provider->InitAndFetchModel(base::BindRepeating(
        &ModelExecutionManagerImpl::OnSegmentationModelUpdated,
        weak_ptr_factory_.GetWeakPtr()));
    model_providers_.emplace(std::make_pair(segment_id, std::move(provider)));
  }
}

ModelExecutionManagerImpl::~ModelExecutionManagerImpl() = default;

void ModelExecutionManagerImpl::ExecuteModel(
    const proto::SegmentInfo& segment_info,
    ModelExecutionCallback callback) {
  OptimizationTarget segment_id = segment_info.segment_id();
  auto model_provider_it = model_providers_.find(segment_id);
  DCHECK(model_provider_it != model_providers_.end());

  ModelProvider& provider = *model_provider_it->second;

  // Create an ExecutionState that will stay with this request until it has been
  // fully processed.
  auto state = std::make_unique<ExecutionState>();
  state->segment_id = segment_id;
  state->model_provider = &provider;
  state->callback = std::move(callback);
  state->total_execution_start_time = clock_->Now();

  ModelExecutionTraceEvent trace_event(
      "ModelExecutionManagerImpl::ExecuteModel", *state);

  if (!provider.ModelAvailable()) {
    RunModelExecutionCallback(std::move(state), 0,
                              ModelExecutionStatus::kSkippedModelNotReady);
    return;
  }

  // It is required to have a valid and well formed segment info.
  if (metadata_utils::ValidateSegmentInfo(segment_info) !=
      metadata_utils::ValidationResult::kValidationSuccess) {
    RunModelExecutionCallback(std::move(state), 0,
                              ModelExecutionStatus::kSkippedInvalidMetadata);
    return;
  }

  state->model_version = segment_info.model_version();
  feature_list_query_processor_->ProcessFeatureList(
      segment_info.model_metadata(), segment_id, clock_->Now(),
      base::BindOnce(
          &ModelExecutionManagerImpl::OnProcessingFeatureListComplete,
          weak_ptr_factory_.GetWeakPtr(), std::move(state)));
}

void ModelExecutionManagerImpl::OnProcessingFeatureListComplete(
    std::unique_ptr<ExecutionState> state,
    bool error,
    const std::vector<float>& input_tensor) {
  if (error) {
    // Validation error occurred on model's metadata.
    RunModelExecutionCallback(std::move(state), 0,
                              ModelExecutionStatus::kSkippedInvalidMetadata);
    return;
  }
  state->input_tensor.insert(state->input_tensor.end(), input_tensor.begin(),
                             input_tensor.end());

  ExecuteModel(std::move(state));
}

void ModelExecutionManagerImpl::ExecuteModel(
    std::unique_ptr<ExecutionState> state) {
  ModelExecutionTraceEvent trace_event(
      "ModelExecutionManagerImpl::ExecuteModel", *state);
  auto it = model_providers_.find(state->segment_id);
  DCHECK(it != model_providers_.end());

  ModelProvider* handler = (*it).second.get();

  if (VLOG_IS_ON(1)) {
    std::stringstream log_input;
    for (unsigned i = 0; i < state->input_tensor.size(); ++i)
      log_input << " feature " << i << ": " << state->input_tensor[i];
    VLOG(1) << "Segmentation model input: " << log_input.str()
            << " for segment "
            << optimization_guide::proto::OptimizationTarget_Name(
                   state->segment_id);
  }
  const std::vector<float>& const_input_tensor = std::move(state->input_tensor);
  stats::RecordModelExecutionZeroValuePercent(state->segment_id,
                                              const_input_tensor);
  state->model_execution_start_time = clock_->Now();
  handler->ExecuteModelWithInput(
      const_input_tensor,
      base::BindOnce(&ModelExecutionManagerImpl::OnModelExecutionComplete,
                     weak_ptr_factory_.GetWeakPtr(), std::move(state)));
}

void ModelExecutionManagerImpl::OnModelExecutionComplete(
    std::unique_ptr<ExecutionState> state,
    const absl::optional<float>& result) {
  ModelExecutionTraceEvent trace_event(
      "ModelExecutionManagerImpl::OnModelExecutionComplete", *state);
  stats::RecordModelExecutionDurationModel(
      state->segment_id, result.has_value(),
      clock_->Now() - state->model_execution_start_time);
  if (result.has_value()) {
    VLOG(1) << "Segmentation model result: " << *result << " for segment "
            << optimization_guide::proto::OptimizationTarget_Name(
                   state->segment_id);
    stats::RecordModelExecutionResult(state->segment_id, result.value());
    if (state->model_version) {
      SegmentationUkmHelper::GetInstance()->RecordModelExecutionResult(
          state->segment_id, state->model_version, state->input_tensor,
          result.value());
    }
    RunModelExecutionCallback(std::move(state), *result,
                              ModelExecutionStatus::kSuccess);
  } else {
    VLOG(1) << "Segmentation model returned no result for segment "
            << optimization_guide::proto::OptimizationTarget_Name(
                   state->segment_id);
    RunModelExecutionCallback(std::move(state), 0,
                              ModelExecutionStatus::kExecutionError);
  }
}

void ModelExecutionManagerImpl::RunModelExecutionCallback(
    std::unique_ptr<ExecutionState> state,
    float result,
    ModelExecutionStatus status) {
  stats::RecordModelExecutionDurationTotal(
      state->segment_id, status,
      clock_->Now() - state->total_execution_start_time);
  stats::RecordModelExecutionStatus(state->segment_id, status);
  std::move(state->callback).Run(std::make_pair(result, status));
}

void ModelExecutionManagerImpl::OnSegmentationModelUpdated(
    optimization_guide::proto::OptimizationTarget segment_id,
    proto::SegmentationModelMetadata metadata,
    int64_t model_version) {
  TRACE_EVENT("segmentation_platform",
              "ModelExecutionManagerImpl::OnSegmentationModelUpdated");
  stats::RecordModelDeliveryReceived(segment_id);
  if (segment_id == optimization_guide::proto::OptimizationTarget::
                        OPTIMIZATION_TARGET_UNKNOWN) {
    return;
  }

  // Set or overwrite name hashes for metadata features based on the name
  // field.
  metadata_utils::SetFeatureNameHashesFromName(&metadata);

  auto validation = metadata_utils::ValidateMetadataAndFeatures(metadata);
  stats::RecordModelDeliveryMetadataValidation(
      segment_id, /* processed = */ false, validation);
  if (validation != metadata_utils::ValidationResult::kValidationSuccess)
    return;

  segment_database_->GetSegmentInfo(
      segment_id,
      base::BindOnce(
          &ModelExecutionManagerImpl::OnSegmentInfoFetchedForModelUpdate,
          weak_ptr_factory_.GetWeakPtr(), segment_id, std::move(metadata),
          model_version));
}

void ModelExecutionManagerImpl::OnSegmentInfoFetchedForModelUpdate(
    optimization_guide::proto::OptimizationTarget segment_id,
    proto::SegmentationModelMetadata metadata,
    int64_t model_version,
    absl::optional<proto::SegmentInfo> old_segment_info) {
  TRACE_EVENT("segmentation_platform",
              "ModelExecutionManagerImpl::OnSegmentInfoFetchedForModelUpdate");
  proto::SegmentInfo new_segment_info;
  new_segment_info.set_segment_id(segment_id);

  // If we find an existing SegmentInfo in the database, we can verify that it
  // is valid, and we can copy over the PredictionResult to the new version
  // we are creating.
  absl::optional<int64_t> old_model_version;
  if (old_segment_info.has_value()) {
    // The retrieved SegmentInfo's ID should match the one we looked up,
    // otherwise the DB has not upheld its contract.
    // If does not match, we should just overwrite the old entry with one
    // that has a matching segment ID, otherwise we will keep ignoring it
    // forever and never be able to clean it up.
    stats::RecordModelDeliverySegmentIdMatches(
        new_segment_info.segment_id(),
        new_segment_info.segment_id() == old_segment_info->segment_id());

    if (old_segment_info->has_prediction_result()) {
      // If we have an old PredictionResult, we need to keep it around in the
      // new version of the SegmentInfo.
      auto* prediction_result = new_segment_info.mutable_prediction_result();
      prediction_result->CopyFrom(old_segment_info->prediction_result());
    }

    if (old_segment_info->has_model_version()) {
      old_model_version = old_segment_info->model_version();
    }
  }

  // Inject the newly updated metadata into the new SegmentInfo.
  auto* new_metadata = new_segment_info.mutable_model_metadata();
  new_metadata->CopyFrom(metadata);
  new_segment_info.set_model_version(model_version);

  if (!old_model_version.has_value() ||
      old_model_version.value() != model_version) {
    new_segment_info.set_model_update_time_s(
        clock_->Now().ToDeltaSinceWindowsEpoch().InSeconds());
  }

  // We have a valid segment id, and the new metadata was valid, therefore the
  // new metadata should be valid. We are not allowed to invoke the callback
  // unless the metadata is valid.
  auto validation =
      metadata_utils::ValidateSegmentInfoMetadataAndFeatures(new_segment_info);
  stats::RecordModelDeliveryMetadataValidation(
      segment_id, /* processed = */ true, validation);
  if (validation != metadata_utils::ValidationResult::kValidationSuccess)
    return;

  stats::RecordModelDeliveryMetadataFeatureCount(
      segment_id, new_segment_info.model_metadata().features_size());
  // Now that we've merged the old and the new SegmentInfo, we want to store
  // the new version in the database.
  segment_database_->UpdateSegment(
      segment_id, absl::make_optional(new_segment_info),
      base::BindOnce(&ModelExecutionManagerImpl::OnUpdatedSegmentInfoStored,
                     weak_ptr_factory_.GetWeakPtr(), new_segment_info));
}

void ModelExecutionManagerImpl::OnUpdatedSegmentInfoStored(
    proto::SegmentInfo segment_info,
    bool success) {
  TRACE_EVENT("segmentation_platform",
              "ModelExecutionManagerImpl::OnUpdatedSegmentInfoStored");
  stats::RecordModelDeliverySaveResult(segment_info.segment_id(), success);
  if (!success)
    return;

  // We are now ready to receive requests for execution, so invoke the
  // callback.
  model_updated_callback_.Run(std::move(segment_info));
}

}  // namespace segmentation_platform
