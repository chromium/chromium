// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/segmentation_platform/internal/execution/model_execution_manager_impl.h"

#include <deque>
#include <map>
#include <memory>
#include <vector>

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/time/clock.h"
#include "base/time/time.h"
#include "base/trace_event/typed_macros.h"
#include "components/segmentation_platform/internal/execution/model_execution_manager.h"
#include "components/segmentation_platform/internal/metadata/metadata_utils.h"
#include "components/segmentation_platform/internal/proto/model_prediction.pb.h"
#include "components/segmentation_platform/internal/stats.h"
#include "components/segmentation_platform/public/model_provider.h"
#include "components/segmentation_platform/public/proto/model_metadata.pb.h"
#include "components/segmentation_platform/public/proto/segmentation_platform.pb.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace optimization_guide {
class OptimizationGuideModelProvider;
}  // namespace optimization_guide

namespace segmentation_platform {

ModelExecutionManagerImpl::ModelExecutionManagerImpl(
    const base::flat_set<SegmentId>& segment_ids,
    ModelProviderFactory* model_provider_factory,
    base::Clock* clock,
    SegmentInfoDatabase* segment_database,
    const SegmentationModelUpdatedCallback& model_updated_callback)
    : clock_(clock),
      segment_database_(segment_database),
      model_updated_callback_(model_updated_callback) {
  for (SegmentId segment_id : segment_ids) {
    std::unique_ptr<ModelProvider> provider =
        model_provider_factory->CreateProvider(segment_id);
    provider->InitAndFetchModel(base::BindRepeating(
        &ModelExecutionManagerImpl::OnSegmentationModelUpdated,
        weak_ptr_factory_.GetWeakPtr()));
    model_providers_.emplace(std::make_pair(segment_id, std::move(provider)));
  }
}

ModelExecutionManagerImpl::~ModelExecutionManagerImpl() = default;

ModelProvider* ModelExecutionManagerImpl::GetProvider(
    proto::SegmentId segment_id) {
  auto it = model_providers_.find(segment_id);
  DCHECK(it != model_providers_.end());
  return it->second.get();
}

void ModelExecutionManagerImpl::OnSegmentationModelUpdated(
    proto::SegmentId segment_id,
    proto::SegmentationModelMetadata metadata,
    int64_t model_version) {
  TRACE_EVENT("segmentation_platform",
              "ModelExecutionManagerImpl::OnSegmentationModelUpdated");
  stats::RecordModelDeliveryReceived(segment_id);
  if (segment_id == proto::SegmentId::OPTIMIZATION_TARGET_UNKNOWN) {
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
  // TODO (ritikagup@) : Add handling for default models, if required.
  segment_database_->GetSegmentInfo(
      segment_id, proto::ModelSource::SERVER_MODEL_SOURCE,
      base::BindOnce(
          &ModelExecutionManagerImpl::OnSegmentInfoFetchedForModelUpdate,
          weak_ptr_factory_.GetWeakPtr(), segment_id, std::move(metadata),
          model_version));
}

void ModelExecutionManagerImpl::OnSegmentInfoFetchedForModelUpdate(
    proto::SegmentId segment_id,
    proto::SegmentationModelMetadata metadata,
    int64_t model_version,
    absl::optional<proto::SegmentInfo> old_segment_info) {
  TRACE_EVENT("segmentation_platform",
              "ModelExecutionManagerImpl::OnSegmentInfoFetchedForModelUpdate");
  proto::SegmentInfo new_segment_info;
  new_segment_info.set_segment_id(segment_id);
  new_segment_info.set_model_source(proto::ModelSource::SERVER_MODEL_SOURCE);
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

  int64_t new_model_update_time_s =
      clock_->Now().ToDeltaSinceWindowsEpoch().InSeconds();
  if (old_model_version.has_value() &&
      old_model_version.value() == model_version &&
      old_segment_info->has_model_update_time_s()) {
    new_model_update_time_s = old_segment_info->model_update_time_s();
  }
  new_segment_info.set_model_update_time_s(new_model_update_time_s);

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
      segment_id, new_segment_info.model_source(),
      absl::make_optional(new_segment_info),
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
