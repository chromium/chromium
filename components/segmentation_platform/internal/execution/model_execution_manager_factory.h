// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SEGMENTATION_PLATFORM_INTERNAL_EXECUTION_MODEL_EXECUTION_MANAGER_FACTORY_H_
#define COMPONENTS_SEGMENTATION_PLATFORM_INTERNAL_EXECUTION_MODEL_EXECUTION_MANAGER_FACTORY_H_

#include <memory>
#include <vector>

#include "base/sequenced_task_runner.h"
#include "components/optimization_guide/proto/models.pb.h"
#include "components/segmentation_platform/internal/execution/model_execution_manager.h"

namespace optimization_guide {
class OptimizationGuideModelProvider;
}  // namespace optimization_guide

namespace segmentation_platform {
class SegmentInfoDatabase;

// Creates a ModelExecutionManager that is appropriate for the current platform.
// In particular, it creates a DummyModelExecutionManager in cases where
// BUILDFLAG(BUILD_WITH_TFLITE_LIB) is not set, in case of the full
// implementation provided by ModelExecutionManagerImpl.
std::unique_ptr<ModelExecutionManager> CreateModelExecutionManager(
    optimization_guide::OptimizationGuideModelProvider* model_provider,
    scoped_refptr<base::SequencedTaskRunner> background_task_runner,
    std::vector<optimization_guide::proto::OptimizationTarget> segment_ids,
    SegmentInfoDatabase* segment_database);

}  // namespace segmentation_platform

#endif  // COMPONENTS_SEGMENTATION_PLATFORM_INTERNAL_EXECUTION_MODEL_EXECUTION_MANAGER_FACTORY_H_
