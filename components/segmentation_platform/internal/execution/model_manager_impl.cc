// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/segmentation_platform/internal/execution/model_manager_impl.h"

#include <map>
#include <memory>
#include <optional>

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/time/clock.h"
#include "base/time/time.h"
#include "base/trace_event/typed_macros.h"
#include "components/segmentation_platform/internal/execution/model_manager.h"
#include "components/segmentation_platform/internal/metadata/metadata_utils.h"
#include "components/segmentation_platform/internal/proto/model_prediction.pb.h"
#include "components/segmentation_platform/internal/stats.h"
#include "components/segmentation_platform/public/model_provider.h"
#include "components/segmentation_platform/public/proto/model_metadata.pb.h"
#include "components/segmentation_platform/public/proto/segmentation_platform.pb.h"

namespace optimization_guide {
class OptimizationGuideModelProvider;
}  // namespace optimization_guide

namespace segmentation_platform {

ModelManagerImpl::ModelManagerImpl(
    const base::flat_set<SegmentId>& segment_ids,
    ModelProviderFactory* model_provider_factory,
    base::Clock* clock,
    SegmentInfoDatabase* segment_database,
    const SegmentationModelUpdatedCallback& model_updated_callback)
    : segment_ids_(segment_ids),
      model_provider_factory_(model_provider_factory),
      clock_(clock),
      segment_database_(segment_database),
      model_updated_callback_(model_updated_callback) {}

void ModelManagerImpl::Initialize() {
  for (SegmentId segment_id : segment_ids_) {
    // Server models
    std::unique_ptr<ModelProvider> provider =
        model_provider_factory_->CreateProvider(segment_id);
    provider->InitAndFetchModel(base::BindRepeating(
        &ModelManagerImpl::OnSegmentationModelUpdated,
        weak_ptr_factory_.GetWeakPtr(), ModelSource::SERVER_MODEL_SOURCE));
    model_providers_.emplace(
        std::make_pair(segment_id, ModelSource::SERVER_MODEL_SOURCE),
        std::move(provider));

    // Default models
    std::unique_ptr<DefaultModelProvider> default_provider =
        model_provider_factory_->CreateDefaultProvider(segment_id);
    if (!default_provider) {
      segment_database_->UpdateSegment(segment_id,
                                       ModelSource::DEFAULT_MODEL_SOURCE,
                                       std::nullopt, base::DoNothing());
      continue;
    }
    std::unique_ptr<DefaultModelProvider::ModelConfig> model_config =
        default_provider->GetModelConfig();
    model_providers_.emplace(
        std::make_pair(segment_id, ModelSource::DEFAULT_MODEL_SOURCE),
        std::move(default_provider));
    OnSegmentationModelUpdated(ModelSource::DEFAULT_MODEL_SOURCE, segment_id,
                               std::move(model_config->metadata),
                               model_config->model_version);
  }
}

ModelManagerImpl::~ModelManagerImpl() = default;

ModelProvider* ModelManagerImpl::GetModelProvider(
    proto::SegmentId segment_id,
    proto::ModelSource model_source) {
  auto it = model_providers_.find(std::make_pair(segment_id, model_source));
  if (it == model_providers_.end()) {
    return nullptr;
  }
  return it->second.get();
}

void ModelManagerImpl::SetSegmentationModelUpdatedCallbackForTesting(
    ModelManager::SegmentationModelUpdatedCallback model_updated_callback) {
  model_updated_callback_ = model_updated_callback;
}

void ModelManagerImpl::OnSegmentationModelUpdated(
    proto::ModelSource model_source,
    proto::SegmentId segment_id,
    std::optional<proto::SegmentationModelMetadata> metadata,
    int64_t model_version) {
  TRACE_EVENT("segmentation_platform",
              "ModelManagerImpl::OnSegmentationModelUpdated");
  stats::RecordModelDeliveryReceived(segment_id, model_source);
  if (segment_id == proto::SegmentId::OPTIMIZATION_TARGET_UNKNOWN) {
    return;
  }

  if (!metadata.has_value()) {
    const auto* deleted_segment =
        segment_database_->GetCachedSegmentInfo(segment_id, model_source);
    if (!deleted_segment) {
      return;
    }

    segment_database_->UpdateSegment(
        segment_id, model_source, std::nullopt,
        base::BindOnce(&ModelManagerImpl::OnSegmentInfoDeleted,
                       weak_ptr_factory_.GetWeakPtr(), segment_id, model_source,
                       deleted_segment->model_version()));
    return;
  }

  // Set or overwrite name hashes for metadata features based on the name
  // field.
  metadata_utils::SetFeatureNameHashesFromName(&metadata.value());

  const auto* old_segment_info =
      segment_database_->GetCachedSegmentInfo(segment_id, model_source);
  OnSegmentInfoFetchedForModelUpdate(segment_id, model_source,
                                     std::move(metadata.value()), model_version,
                                     old_segment_info);
}

void ModelManagerImpl::OnSegmentInfoFetchedForModelUpdate(
    proto::SegmentId segment_id,
    proto::ModelSource model_source,
    proto::SegmentationModelMetadata metadata,
    int64_t model_version,
    const proto::SegmentInfo* old_segment_info) {
  TRACE_EVENT("segmentation_platform",
              "ModelManagerImpl::OnSegmentInfoFetchedForModelUpdate");
  proto::SegmentInfo new_segment_info;
  new_segment_info.set_segment_id(segment_id);
  new_segment_info.set_model_source(model_source);

  // Inject the newly updated metadata into the new SegmentInfo.
  auto* new_metadata = new_segment_info.mutable_model_metadata();
  new_metadata->Swap(&metadata);
  new_segment_info.set_model_version(model_version);

  int64_t new_model_update_time_s =
      clock_->Now().ToDeltaSinceWindowsEpoch().InSeconds();

  // If we find an existing SegmentInfo in the database, we can verify that it
  // is valid, and we can copy over the PredictionResult to the new version
  // we are creating.
  std::optional<int64_t> old_model_version;
  if (old_segment_info) {
    // The retrieved SegmentInfo's ID should match the one we looked up,
    // otherwise the DB has not upheld its contract.
    // If does not match, we should just overwrite the old entry with one
    // that has a matching segment ID, otherwise we will keep ignoring it
    // forever and never be able to clean it up.
    stats::RecordModelDeliverySegmentIdMatches(
        new_segment_info.segment_id(), model_source,
        new_segment_info.segment_id() == old_segment_info->segment_id());

    if (old_segment_info->has_model_version()) {
      old_model_version = old_segment_info->model_version();
    }
    bool is_same_model = old_model_version.has_value() &&
                         old_model_version.value() == model_version;

    if (is_same_model) {
      if (old_segment_info->has_prediction_result()) {
        // If we have an old PredictionResult, we need to keep it around in the
        // new version of the SegmentInfo.
        auto* prediction_result = new_segment_info.mutable_prediction_result();
        prediction_result->CopyFrom(old_segment_info->prediction_result());
      }

      if (old_segment_info->has_model_update_time_s()) {
        new_model_update_time_s = old_segment_info->model_update_time_s();
      }

      if (old_segment_info->training_data_size() > 0) {
        for (int i = 0; i < old_segment_info->training_data_size(); i++) {
          new_segment_info.add_training_data()->CopyFrom(
              old_segment_info->training_data(i));
        }
      }
    }
  }
  new_segment_info.set_model_update_time_s(new_model_update_time_s);

  // We have a valid segment id, and the new metadata was valid, therefore the
  // new metadata should be valid. We are not allowed to invoke the callback
  // unless the metadata is valid.
  auto validation =
      metadata_utils::ValidateSegmentInfoMetadataAndFeatures(new_segment_info);
  stats::RecordModelDeliveryMetadataValidation(
      segment_id, model_source, /* processed = */ true, validation);
  if (validation != metadata_utils::ValidationResult::kValidationSuccess) {
    return;
  }

  stats::RecordModelDeliveryMetadataFeatureCount(
      segment_id, model_source,
      new_segment_info.model_metadata().input_features_size());

  // Now that we've merged the old and the new SegmentInfo, we want to store
  // the new version in the database.
  auto update_callback = base::BindOnce(
      &ModelManagerImpl::OnUpdatedSegmentInfoStored,
      weak_ptr_factory_.GetWeakPtr(), new_segment_info, old_model_version);
  segment_database_->UpdateSegment(
      segment_id, model_source, std::make_optional(std::move(new_segment_info)),
      std::move(update_callback));
}

void ModelManagerImpl::OnSegmentInfoDeleted(SegmentId segment_id,
                                            proto::ModelSource model_source,
                                            int64_t deleted_version,
                                            bool success) {
  stats::RecordModelDeliveryDeleteResult(segment_id, model_source, success);

  // `model_updated_callback_` only supports server models.
  if (model_source == proto::ModelSource::DEFAULT_MODEL_SOURCE) {
    return;
  }

  proto::SegmentInfo deleted_segment_info;
  deleted_segment_info.set_segment_id(segment_id);
  deleted_segment_info.set_model_source(model_source);
  model_updated_callback_.Run(std::move(deleted_segment_info), deleted_version);
}

void ModelManagerImpl::OnUpdatedSegmentInfoStored(
    proto::SegmentInfo segment_info,
    std::optional<int64_t> old_model_version,
    bool success) {
  TRACE_EVENT("segmentation_platform",
              "ModelManagerImpl::OnUpdatedSegmentInfoStored");

  stats::RecordModelDeliverySaveResult(segment_info.segment_id(),
                                       segment_info.model_source(), success);
  if (!success) {
    return;
  }

  // We are now ready to receive requests for execution, so invoke the
  // callback.
  // TODO (ritikagup@) : Add handling for default models to run callback.
  // Remove this when callback supports default models.
  if (segment_info.model_source() == proto::ModelSource::DEFAULT_MODEL_SOURCE) {
    return;
  }
  model_updated_callback_.Run(std::move(segment_info), old_model_version);
}

}  // namespace segmentation_platform
