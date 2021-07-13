// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/segmentation_platform/internal/execution/model_execution_manager_factory.h"

#include <memory>
#include <vector>

#include "base/bind.h"
#include "base/sequenced_task_runner.h"
#include "components/optimization_guide/machine_learning_tflite_buildflags.h"
#include "components/optimization_guide/proto/models.pb.h"
#include "components/segmentation_platform/internal/execution/feature_aggregator.h"
#include "components/segmentation_platform/internal/execution/model_execution_manager.h"

#if BUILDFLAG(BUILD_WITH_TFLITE_LIB)
#include "components/segmentation_platform/internal/execution/model_execution_manager_impl.h"
#include "components/segmentation_platform/internal/execution/segmentation_model_handler.h"
#else
#include "components/segmentation_platform/internal/execution/dummy_model_execution_manager.h"
#endif  // BUILDFLAG(BUILD_WITH_TFLITE_LIB)

namespace base {
class Clock;
}  // namespace base

namespace optimization_guide {
class OptimizationGuideModelProvider;
}  // namespace optimization_guide

namespace segmentation_platform {
class FeatureAggregator;
class SegmentInfoDatabase;
class SignalDatabase;

#if BUILDFLAG(BUILD_WITH_TFLITE_LIB)
// CreateModelHandler makes it possible to pass in any creator of the
// SegmentationModelHandler, which makes it possible to create mock versions.
std::unique_ptr<SegmentationModelHandler> CreateModelHandler(
    optimization_guide::OptimizationGuideModelProvider* model_provider,
    scoped_refptr<base::SequencedTaskRunner> background_task_runner,
    optimization_guide::proto::OptimizationTarget optimization_target,
    const SegmentationModelHandler::ModelUpdatedCallback&
        model_updated_callback) {
  return std::make_unique<SegmentationModelHandler>(
      model_provider, background_task_runner, optimization_target,
      model_updated_callback);
}
#endif  // BUILDFLAG(BUILD_WITH_TFLITE_LIB)

std::unique_ptr<ModelExecutionManager> CreateModelExecutionManager(
    optimization_guide::OptimizationGuideModelProvider* model_provider,
    scoped_refptr<base::SequencedTaskRunner> background_task_runner,
    std::vector<optimization_guide::proto::OptimizationTarget> segment_ids,
    base::Clock* clock,
    SegmentInfoDatabase* segment_database,
    SignalDatabase* signal_database,
    std::unique_ptr<FeatureAggregator> feature_aggregator,
    const ModelExecutionManager::SegmentationModelUpdatedCallback&
        model_updated_callback) {
#if BUILDFLAG(BUILD_WITH_TFLITE_LIB)
  return std::make_unique<ModelExecutionManagerImpl>(
      segment_ids,
      base::BindRepeating(&CreateModelHandler, model_provider,
                          background_task_runner),
      clock, segment_database, signal_database, std::move(feature_aggregator),
      model_updated_callback);
#else
  return std::make_unique<DummyModelExecutionManager>();
#endif  // BUILDFLAG(BUILD_WITH_TFLITE_LIB)
}

}  // namespace segmentation_platform
