// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/segmentation_platform/internal/execution/segmentation_model_handler.h"

#include <memory>
#include <vector>

#include "components/optimization_guide/core/model_executor.h"
#include "components/optimization_guide/proto/models.pb.h"
#include "components/segmentation_platform/internal/execution/segmentation_model_executor.h"

namespace segmentation_platform {

SegmentationModelHandler::SegmentationModelHandler(
    optimization_guide::OptimizationGuideModelProvider* model_provider,
    scoped_refptr<base::SequencedTaskRunner> background_task_runner,
    optimization_guide::proto::OptimizationTarget optimization_target)
    : optimization_guide::ModelHandler<float, const std::vector<float>&>(
          model_provider,
          background_task_runner,
          std::make_unique<SegmentationModelExecutor>(),
          optimization_target,
          /*model_metadata=*/absl::nullopt) {}

SegmentationModelHandler::~SegmentationModelHandler() = default;

}  // namespace segmentation_platform
