// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SEGMENTATION_PLATFORM_INTERNAL_SCHEDULER_EXECUTION_SERVICE_H_
#define COMPONENTS_SEGMENTATION_PLATFORM_INTERNAL_SCHEDULER_EXECUTION_SERVICE_H_

#include <memory>
#include <vector>

#include "base/containers/flat_set.h"
#include "base/task/sequenced_task_runner.h"
#include "base/time/clock.h"
#include "components/optimization_guide/proto/models.pb.h"
#include "components/segmentation_platform/internal/execution/model_execution_manager.h"
#include "components/segmentation_platform/internal/scheduler/model_execution_scheduler.h"

namespace segmentation_platform {

struct PlatformOptions;
class FeatureListQueryProcessor;
class ModelProviderFactory;
class ModelExecutionSchedulerImpl;
class SegmentInfoDatabase;
class SignalDatabase;
class SignalHandler;
class SignalStorageConfig;
class TrainingDataCollector;

// Handles feature processing and model execution.
class ExecutionService {
 public:
  ExecutionService();
  ~ExecutionService();

  ExecutionService(ExecutionService&) = delete;
  ExecutionService& operator=(ExecutionService&) = delete;

  void Initialize(
      SignalDatabase* signal_database,
      SegmentInfoDatabase* segment_info_database,
      SignalStorageConfig* signal_storage_config,
      SignalHandler* signal_handler,
      base::Clock* clock,
      ModelExecutionManager::SegmentationModelUpdatedCallback callback,
      scoped_refptr<base::SequencedTaskRunner> task_runner,
      const base::flat_set<OptimizationTarget>& all_segment_ids,
      ModelProviderFactory* model_provider_factory,
      std::vector<ModelExecutionScheduler::Observer*>&& observers,
      const PlatformOptions& platform_options);

  // Called whenever a new or updated model is available. Must be a valid
  // SegmentInfo with valid metadata and features.
  void OnNewModelInfoReady(const proto::SegmentInfo& segment_info);

  // TODO(ssid): Remove this method and pass in ExecutionService to proxy
  // service.
  ModelExecutionSchedulerImpl* deprecated_model_execution_scheduler() {
    return model_execution_scheduler_.get();
  }
  // TODO(ssid): Remove this method and pass in ExecutionService to selector.
  ModelExecutionManager* deprecated_model_execution_manager() {
    return model_execution_manager_.get();
  }

 private:
  // Training/inference input data generation.
  std::unique_ptr<FeatureListQueryProcessor> feature_list_query_processor_;

  // Traing data collection logic.
  std::unique_ptr<TrainingDataCollector> training_data_collector_;

  // Model execution scheduling logic.
  std::unique_ptr<ModelExecutionSchedulerImpl> model_execution_scheduler_;

  // Model execution.
  std::unique_ptr<ModelExecutionManager> model_execution_manager_;
};

}  // namespace segmentation_platform

#endif  // COMPONENTS_SEGMENTATION_PLATFORM_INTERNAL_SCHEDULER_EXECUTION_SERVICE_H_
