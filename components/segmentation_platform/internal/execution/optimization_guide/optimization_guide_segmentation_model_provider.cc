// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/segmentation_platform/internal/execution/optimization_guide/optimization_guide_segmentation_model_provider.h"

#include <memory>
#include <vector>

#include "base/task/sequenced_task_runner.h"
#include "components/optimization_guide/core/model_executor.h"
#include "components/optimization_guide/proto/common_types.pb.h"
#include "components/segmentation_platform/internal/execution/optimization_guide/optimization_guide_segmentation_model_handler.h"
#include "components/segmentation_platform/internal/execution/optimization_guide/segmentation_model_executor.h"
#include "components/segmentation_platform/internal/segment_id_convertor.h"
#include "components/segmentation_platform/internal/stats.h"
#include "components/segmentation_platform/public/proto/model_metadata.pb.h"
#include "components/segmentation_platform/public/proto/segmentation_platform.pb.h"

namespace segmentation_platform {

namespace {

const char kSegmentationModelMetadataTypeUrl[] =
    "type.googleapis.com/"
    "google.internal.chrome.optimizationguide.v1.SegmentationModelMetadata";

std::optional<optimization_guide::proto::Any> GetModelFetchConfig() {
  // Preparing the version data to be sent to server along with the request to
  // download the model.
  optimization_guide::proto::Any any_metadata;
  any_metadata.set_type_url(kSegmentationModelMetadataTypeUrl);
  proto::SegmentationModelMetadata model_metadata;
  proto::VersionInfo* version_info = model_metadata.mutable_version_info();
  version_info->set_metadata_cur_version(
      proto::CurrentVersion::METADATA_VERSION);
  model_metadata.SerializeToString(any_metadata.mutable_value());
  return any_metadata;
}

}  // namespace

OptimizationGuideSegmentationModelProvider::
    OptimizationGuideSegmentationModelProvider(
        optimization_guide::OptimizationGuideModelProvider* model_provider,
        scoped_refptr<base::SequencedTaskRunner> background_task_runner,
        proto::SegmentId segment_id)
    : ModelProvider(segment_id),
      model_provider_(model_provider),
      background_task_runner_(background_task_runner) {}

OptimizationGuideSegmentationModelProvider::
    ~OptimizationGuideSegmentationModelProvider() = default;

void OptimizationGuideSegmentationModelProvider::InitAndFetchModel(
    const ModelUpdatedCallback& model_updated_callback) {
  DCHECK(!model_handler_);
  std::optional<optimization_guide::proto::OptimizationTarget> target =
      SegmentIdToOptimizationTarget(segment_id_);
  if (!target) {
    // If the segment ID is not an OptimizationTarget then do not request a
    // model.
    return;
  }
  model_handler_ = std::make_unique<OptimizationGuideSegmentationModelHandler>(
      model_provider_, background_task_runner_, *target, model_updated_callback,
      GetModelFetchConfig());
}

void OptimizationGuideSegmentationModelProvider::ExecuteModelWithInput(
    const ModelProvider::Request& inputs,
    ExecutionCallback callback) {
  if (!model_handler_) {
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), std::nullopt));
    return;
  }
  model_handler_->ExecuteModelWithInput(std::move(callback), inputs);
}

bool OptimizationGuideSegmentationModelProvider::ModelAvailable() {
  if (!model_handler_) {
    return false;
  }
  return model_handler_->ModelAvailable();
}

}  // namespace segmentation_platform
