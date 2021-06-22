// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/segmentation_platform/internal/execution/model_execution_manager_factory.h"

#include <memory>
#include <vector>

#include "base/sequenced_task_runner.h"
#include "components/optimization_guide/machine_learning_tflite_buildflags.h"
#include "components/optimization_guide/proto/models.pb.h"
#include "components/segmentation_platform/internal/execution/model_execution_manager.h"

#if BUILDFLAG(BUILD_WITH_TFLITE_LIB)
#include "components/segmentation_platform/internal/execution/model_execution_manager_impl.h"
#else
#include "components/segmentation_platform/internal/execution/dummy_model_execution_manager.h"
#endif  // BUILDFLAG(BUILD_WITH_TFLITE_LIB)

namespace optimization_guide {
class OptimizationGuideModelProvider;
}  // namespace optimization_guide

namespace segmentation_platform {
class SegmentInfoDatabase;

std::unique_ptr<ModelExecutionManager> CreateModelExecutionManager(
    optimization_guide::OptimizationGuideModelProvider* model_provider,
    scoped_refptr<base::SequencedTaskRunner> background_task_runner,
    std::vector<optimization_guide::proto::OptimizationTarget> segment_ids,
    SegmentInfoDatabase* segment_database) {
#if BUILDFLAG(BUILD_WITH_TFLITE_LIB)
  return std::make_unique<ModelExecutionManagerImpl>(
      model_provider, background_task_runner, segment_ids, segment_database);
#else
  return std::make_unique<DummyModelExecutionManager>();
#endif  // BUILDFLAG(BUILD_WITH_TFLITE_LIB)
}

}  // namespace segmentation_platform
