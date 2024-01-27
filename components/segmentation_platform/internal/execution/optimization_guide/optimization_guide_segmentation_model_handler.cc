// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/segmentation_platform/internal/execution/optimization_guide/optimization_guide_segmentation_model_handler.h"

#include <memory>
#include <vector>

#include "base/task/sequenced_task_runner.h"
#include "components/optimization_guide/core/model_executor.h"
#include "components/optimization_guide/proto/common_types.pb.h"
#include "components/optimization_guide/proto/models.pb.h"
#include "components/segmentation_platform/internal/execution/optimization_guide/segmentation_model_executor.h"
#include "components/segmentation_platform/internal/segment_id_convertor.h"
#include "components/segmentation_platform/internal/stats.h"
#include "components/segmentation_platform/public/proto/model_metadata.pb.h"

namespace segmentation_platform {

OptimizationGuideSegmentationModelHandler::
    OptimizationGuideSegmentationModelHandler(
        optimization_guide::OptimizationGuideModelProvider* model_provider,
        scoped_refptr<base::SequencedTaskRunner> background_task_runner,
        optimization_guide::proto::OptimizationTarget segment_id,
        const ModelUpdatedCallback& model_updated_callback,
        std::optional<optimization_guide::proto::Any>&& model_metadata)
    : optimization_guide::ModelHandler<ModelProvider::Response,
                                       const ModelProvider::Request&>(
          model_provider,
          background_task_runner,
          std::make_unique<SegmentationModelExecutor>(),
          /*model_inference_timeout=*/std::nullopt,
          segment_id,
          model_metadata),
      model_updated_callback_(model_updated_callback) {
  stats::RecordModelAvailability(
      OptimizationTargetToSegmentId(segment_id),
      stats::SegmentationModelAvailability::kModelHandlerCreated);
}

OptimizationGuideSegmentationModelHandler::
    ~OptimizationGuideSegmentationModelHandler() = default;

void OptimizationGuideSegmentationModelHandler::OnModelUpdated(
    optimization_guide::proto::OptimizationTarget optimization_target,
    base::optional_ref<const optimization_guide::ModelInfo> model_info) {
  // First invoke parent to update internal status.
  optimization_guide::ModelHandler<
      ModelProvider::Response,
      const ModelProvider::Request&>::OnModelUpdated(optimization_target,
                                                     model_info);
  proto::SegmentId segment_id =
      OptimizationTargetToSegmentId(optimization_target);

  // This can happen if a server model stops being served. We'll receive one
  // last update from optimization guide after it deleted its local data so we
  // should do the same.
  if (!model_info.has_value()) {
    // The parent class should reset the model availability if it receives no
    // metadata.
    DCHECK(!ModelAvailable());
    stats::RecordModelAvailability(
        segment_id, stats::SegmentationModelAvailability::kNoModelAvailable);

    model_updated_callback_.Run(segment_id, std::nullopt,
                                /*model_version = */ 0);
    return;
  }
  // The parent class should always set the model availability to true after
  // having received an updated model.
  DCHECK(ModelAvailable());

  // Grab the segmentation specific metadata from the Any proto. If we are
  // unable to find it, there is no point in informing the rest of the platform.
  std::optional<proto::SegmentationModelMetadata> segmentation_model_metadata =
      ParsedSupportedFeaturesForLoadedModel<proto::SegmentationModelMetadata>();
  stats::RecordModelDeliveryHasMetadata(
      segment_id, segmentation_model_metadata.has_value());
  if (!segmentation_model_metadata.has_value()) {
    // This is not expected to happen, since the optimization guide server is
    // expected to pass this along. Either something failed horribly on the way,
    // we failed to read the metadata, or the server side configuration is
    // wrong.
    stats::RecordModelAvailability(
        segment_id, stats::SegmentationModelAvailability::kMetadataInvalid);
    return;
  }
  stats::RecordModelAvailability(
      segment_id, stats::SegmentationModelAvailability::kModelAvailable);

  model_updated_callback_.Run(segment_id,
                              std::move(*segmentation_model_metadata),
                              model_info->GetVersion());
}

}  // namespace segmentation_platform
