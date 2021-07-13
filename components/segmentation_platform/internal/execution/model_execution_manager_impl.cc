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
#include "base/time/clock.h"
#include "base/time/time.h"
#include "components/optimization_guide/core/model_executor.h"
#include "components/optimization_guide/proto/models.pb.h"
#include "components/segmentation_platform/internal/database/metadata_utils.h"
#include "components/segmentation_platform/internal/database/signal_database.h"
#include "components/segmentation_platform/internal/execution/feature_aggregator.h"
#include "components/segmentation_platform/internal/execution/model_execution_manager.h"
#include "components/segmentation_platform/internal/execution/model_execution_status.h"
#include "components/segmentation_platform/internal/execution/segmentation_model_handler.h"
#include "components/segmentation_platform/internal/proto/aggregation.pb.h"
#include "components/segmentation_platform/internal/proto/model_metadata.pb.h"
#include "components/segmentation_platform/internal/proto/model_prediction.pb.h"
#include "components/segmentation_platform/internal/proto/types.pb.h"
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
  std::vector<float> input_tensor;
  base::Time end_time;
};

struct ModelExecutionManagerImpl::FeatureState {
  FeatureState() = default;
  ~FeatureState() = default;

  // Disallow copy/assign.
  FeatureState(const FeatureState&) = delete;
  FeatureState& operator=(const FeatureState&) = delete;

  proto::SignalType signal_type;
  proto::Aggregation aggregation;
  absl::optional<std::vector<int32_t>> accepted_enum_ids;
  uint64_t bucket_count;
  uint64_t tensor_length;
};

ModelExecutionManagerImpl::ModelExecutionManagerImpl(
    std::vector<OptimizationTarget> segment_ids,
    ModelHandlerCreator model_handler_creator,
    base::Clock* clock,
    SegmentInfoDatabase* segment_database,
    SignalDatabase* signal_database,
    std::unique_ptr<FeatureAggregator> feature_aggregator,
    const SegmentationModelUpdatedCallback& model_updated_callback)
    : clock_(clock),
      segment_database_(segment_database),
      signal_database_(signal_database),
      feature_aggregator_(std::move(feature_aggregator)),
      model_updated_callback_(model_updated_callback) {
  for (OptimizationTarget segment_id : segment_ids) {
    model_handlers_.emplace(std::make_pair(
        segment_id,
        model_handler_creator.Run(
            segment_id,
            base::BindRepeating(
                &ModelExecutionManagerImpl::OnSegmentationModelUpdated,
                weak_ptr_factory_.GetWeakPtr()))));
  }
}

ModelExecutionManagerImpl::~ModelExecutionManagerImpl() = default;

void ModelExecutionManagerImpl::ExecuteModel(OptimizationTarget segment_id,
                                             ModelExecutionCallback callback) {
  auto model_handler_it = model_handlers_.find(segment_id);
  DCHECK(model_handler_it != model_handlers_.end());

  // Create an ExecutionState that will stay with this request until it has been
  // fully processed.
  auto state = std::make_unique<ExecutionState>();
  state->segment_id = segment_id;
  state->model_handler = (*model_handler_it).second.get();
  state->callback = std::move(callback);

  // We first need to look up all relevant metadata for the related segment, as
  // the metadata informs how we should process the data.
  segment_database_->GetSegmentInfo(
      segment_id,
      base::BindOnce(
          &ModelExecutionManagerImpl::OnSegmentInfoFetchedForExecution,
          weak_ptr_factory_.GetWeakPtr(), std::move(state)));
}

void ModelExecutionManagerImpl::OnSegmentInfoFetchedForExecution(
    std::unique_ptr<ExecutionState> state,
    absl::optional<proto::SegmentInfo> segment_info) {
  // It is required to have a valid and well formed segment info.
  if (!segment_info || metadata_utils::ValidateSegmentInfo(*segment_info) !=
                           metadata_utils::VALIDATION_SUCCESS) {
    RunModelExecutionCallback(std::move(state->callback), 0,
                              ModelExecutionStatus::INVALID_METADATA);
    return;
  }

  // The total bucket duration is defined by product of the bucket_duration
  // value and the length of related time_unit field, e.g. 28 * length(DAY).
  const auto& model_metadata = segment_info->model_metadata();
  uint64_t bucket_duration = model_metadata.bucket_duration();
  base::TimeDelta time_unit_len = metadata_utils::GetTimeUnit(model_metadata);
  state->bucket_duration = bucket_duration * time_unit_len;

  // Now that we have just fetched the metadata, set the end_time to be shared
  // across all features, so we get a consistent picture.
  state->end_time = clock_->Now();

  // Grab the metadata for all the features, which will be processed one at a
  // time, before executing the model.
  for (int i = 0; i < model_metadata.features_size(); ++i)
    state->features.emplace_back(model_metadata.features(i));

  // Process all the features in-order, starting with the first feature.
  ProcessFeatures(std::move(state));
}

void ModelExecutionManagerImpl::ProcessFeatures(
    std::unique_ptr<ExecutionState> state) {
  // When there are no more features to process, we are done, so we execute the
  // model.
  if (state->features.empty()) {
    ExecuteModel(std::move(state));
    return;
  }

  proto::Feature feature;
  do {
    // Copy and pop the next feature.
    feature = state->features.front();
    state->features.pop_front();

    // Validate the proto::Feature metadata.
    if (metadata_utils::ValidateMetadataFeature(feature) !=
        metadata_utils::VALIDATION_SUCCESS) {
      RunModelExecutionCallback(std::move(state->callback), 0,
                                ModelExecutionStatus::INVALID_METADATA);
      return;
    }
  } while (feature.bucket_count() == 0);  // Skip collection-only features.

  // Capture all relevant metadata for the current proto::Feature into the
  // FeatureState.
  auto feature_state = std::make_unique<FeatureState>();
  feature_state->signal_type = metadata_utils::GetSignalTypeForFeature(feature);
  feature_state->aggregation = feature.aggregation();
  feature_state->bucket_count = feature.bucket_count();
  feature_state->tensor_length = feature.tensor_length();

  absl::optional<int64_t> name_hash =
      metadata_utils::GetNameHashForFeature(feature);

  // Enum histograms can optionally only accept some of the enum values.
  // While the proto::Feature is available, capture a vector of the accepted
  // enum values. An empty vector is ignored (all values are considered
  // accepted).
  if (feature_state->signal_type == proto::SignalType::HISTOGRAM_ENUM) {
    std::vector<int32_t> accepted_enum_ids{};
    for (int i = 0; i < feature.enum_ids_size(); ++i)
      accepted_enum_ids.emplace_back(feature.enum_ids(i));

    feature_state->accepted_enum_ids = absl::make_optional(accepted_enum_ids);
  }

  // Only fetch data that is relevant for the current proto::Feature, since
  // the FeatureAggregator assumes that only relevant data is given to it.
  base::TimeDelta duration =
      state->bucket_duration * feature_state->bucket_count;
  base::Time start_time = state->end_time - duration;

  // Fetch the relevant samples for the current proto::Feature. Once the result
  // has come back, it will be processed and inserted into the
  // ExecutorState::input_tensor and will then invoke ProcessFeatures(...)
  // again to ensure we continue until all features have been processed.
  // Note: All parameters from the ExecutorState need to be captured locally
  // before invoking GetSamples, because the state is moved with the callback,
  // and the order of the move and accessing the members while invoking
  // GetSamples is not guaranteed.
  auto signal_type = feature_state->signal_type;
  auto end_time = state->end_time;
  signal_database_->GetSamples(
      signal_type, *name_hash, start_time, end_time,
      base::BindOnce(&ModelExecutionManagerImpl::OnGetSamplesForFeature,
                     weak_ptr_factory_.GetWeakPtr(), std::move(state),
                     std::move(feature_state)));
}

void ModelExecutionManagerImpl::OnGetSamplesForFeature(
    std::unique_ptr<ExecutionState> state,
    std::unique_ptr<FeatureState> feature_state,
    std::vector<SignalDatabase::Sample> samples) {
  // HISTOGRAM_ENUM features might require us to filter out the result to only
  // keep enum values that match the accepted list. If the accepted list is'
  // empty, all histogram enum values are kept.
  // The SignalDatabase does not currently support this type of data filter,
  // so instead we are doing this here.
  if (feature_state->signal_type == proto::SignalType::HISTOGRAM_ENUM) {
    DCHECK(feature_state->accepted_enum_ids.has_value());
    feature_aggregator_->FilterEnumSamples(*feature_state->accepted_enum_ids,
                                           samples);
  }

  // We now have all the data required to process a single feature, so we can
  // process it synchronously, and insert it into the
  // ExecutorState::input_tensor so we can later pass it to the ML model
  // executor.
  std::vector<float> feature_data = feature_aggregator_->Process(
      feature_state->signal_type, feature_state->aggregation,
      feature_state->bucket_count, state->end_time, state->bucket_duration,
      samples);
  DCHECK_EQ(feature_state->tensor_length, feature_data.size());
  state->input_tensor.insert(state->input_tensor.end(), feature_data.begin(),
                             feature_data.end());

  // Continue with the rest of the features.
  ProcessFeatures(std::move(state));
}

void ModelExecutionManagerImpl::ExecuteModel(
    std::unique_ptr<ExecutionState> state) {
  auto it = model_handlers_.find(state->segment_id);
  DCHECK(it != model_handlers_.end());

  SegmentationModelHandler* handler = (*it).second.get();
  if (!handler->ModelAvailable()) {
    RunModelExecutionCallback(std::move(state->callback), 0,
                              ModelExecutionStatus::EXECUTION_ERROR);
    return;
  }

  const std::vector<float>& const_input_tensor = std::move(state->input_tensor);
  handler->ExecuteModelWithInput(
      base::BindOnce(&ModelExecutionManagerImpl::OnModelExecutionComplete,
                     weak_ptr_factory_.GetWeakPtr(), std::move(state)),
      const_input_tensor);
}

void ModelExecutionManagerImpl::OnModelExecutionComplete(
    std::unique_ptr<ExecutionState> state,
    const absl::optional<float>& result) {
  if (result.has_value()) {
    RunModelExecutionCallback(std::move(state->callback), *result,
                              ModelExecutionStatus::SUCCESS);
  } else {
    RunModelExecutionCallback(std::move(state->callback), 0,
                              ModelExecutionStatus::EXECUTION_ERROR);
  }
}

void ModelExecutionManagerImpl::RunModelExecutionCallback(
    ModelExecutionCallback callback,
    float result,
    ModelExecutionStatus status) {
  std::move(callback).Run(std::make_pair(result, status));
}

void ModelExecutionManagerImpl::OnSegmentationModelUpdated(
    optimization_guide::proto::OptimizationTarget segment_id,
    proto::SegmentationModelMetadata metadata) {
  auto validation = metadata_utils::ValidateMetadataAndFeatures(metadata);
  if (validation != metadata_utils::ValidationResult::VALIDATION_SUCCESS) {
    // TODO(nyquist): Add metrics for this.
    return;
  }

  segment_database_->GetSegmentInfo(
      segment_id,
      base::BindOnce(
          &ModelExecutionManagerImpl::OnSegmentInfoFetchedForModelUpdate,
          weak_ptr_factory_.GetWeakPtr(), segment_id, std::move(metadata)));
}
void ModelExecutionManagerImpl::OnSegmentInfoFetchedForModelUpdate(
    optimization_guide::proto::OptimizationTarget segment_id,
    proto::SegmentationModelMetadata metadata,
    absl::optional<proto::SegmentInfo> old_segment_info) {
  proto::SegmentInfo new_segment_info;
  new_segment_info.set_segment_id(segment_id);

  // If we find an existing SegmentInfo in the database, we can verify that it
  // is valid, and we can copy over the PredictionResult to the new version
  // we are creating.
  if (old_segment_info.has_value()) {
    if (old_segment_info->has_segment_id()) {
      // The retrieved SegmentInfo's ID should match the one we looked up,
      // otherwise the DB has not upheld its contract.
      DCHECK_EQ(new_segment_info.segment_id(), old_segment_info->segment_id());
      // If does not match, we should just overwrite the old entry with one
      // that has a matching segment ID, otherwise we will keep ignoring it
      // forever and never be able to clean it up.
      // TODO(nyquist): Add metrics for this.
    }

    if (old_segment_info->has_prediction_result()) {
      // If we have an old PredictionResult, we need to keep it around in the
      // new version of the SegmentInfo.
      auto* prediction_result = new_segment_info.mutable_prediction_result();
      prediction_result->CopyFrom(old_segment_info->prediction_result());
    }
  }

  // Inject the newly updated metadata into the new SegmentInfo.
  auto* new_metadata = new_segment_info.mutable_model_metadata();
  new_metadata->CopyFrom(metadata);

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
  if (!success) {
    // TODO(nyquist): Add metrics for this.
    return;
  }

  // We are now ready to receive requests for execution, so invoke the callback.
  model_updated_callback_.Run(std::move(segment_info));
}

}  // namespace segmentation_platform
