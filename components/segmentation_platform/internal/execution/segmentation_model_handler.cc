// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/segmentation_platform/internal/execution/segmentation_model_handler.h"

#include <memory>
#include <vector>

#include "components/optimization_guide/core/model_executor.h"
#include "components/optimization_guide/proto/common_types.pb.h"
#include "components/optimization_guide/proto/models.pb.h"
#include "components/segmentation_platform/internal/execution/segmentation_model_executor.h"
#include "components/segmentation_platform/internal/proto/model_metadata.pb.h"
#include "components/segmentation_platform/internal/stats.h"

namespace segmentation_platform {

SegmentationModelHandler::SegmentationModelHandler(
    optimization_guide::OptimizationGuideModelProvider* model_provider,
    scoped_refptr<base::SequencedTaskRunner> background_task_runner,
    optimization_guide::proto::OptimizationTarget optimization_target,
    const ModelUpdatedCallback& model_updated_callback)
    : optimization_guide::ModelHandler<float, const std::vector<float>&>(
          model_provider,
          background_task_runner,
          std::make_unique<SegmentationModelExecutor>(),
          optimization_target,
          /*model_metadata=*/absl::nullopt),
      model_updated_callback_(model_updated_callback) {}

SegmentationModelHandler::~SegmentationModelHandler() = default;

void SegmentationModelHandler::OnModelFileUpdated(
    optimization_guide::proto::OptimizationTarget optimization_target,
    const absl::optional<optimization_guide::proto::Any>& model_metadata,
    const base::FilePath& file_path) {
  // First invoke parent to update internal status.
  optimization_guide::ModelHandler<
      float, const std::vector<float>&>::OnModelFileUpdated(optimization_target,
                                                            model_metadata,
                                                            file_path);
  // The parent class should always set the model availability to true after
  // having received an updated model.
  DCHECK(ModelAvailable());

  // Grab the segmentation specific metadata from the Any proto. If we are
  // unable to find it, there is no point in informing the rest of the platform.
  absl::optional<proto::SegmentationModelMetadata> segmentation_model_metadata =
      ParsedSupportedFeaturesForLoadedModel<proto::SegmentationModelMetadata>();
  stats::RecordModelDeliveryHasMetadata(
      optimization_target, segmentation_model_metadata.has_value());
  if (!segmentation_model_metadata.has_value()) {
    // This is not expected to happen, since the optimization guide server is
    // expected to pass this along. Either something failed horribly on the way,
    // we failed to read the metadata, or the server side configuration is
    // wrong.
    return;
  }

  model_updated_callback_.Run(optimization_target,
                              std::move(*segmentation_model_metadata));
}

}  // namespace segmentation_platform
