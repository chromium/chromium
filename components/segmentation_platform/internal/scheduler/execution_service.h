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
#include "components/segmentation_platform/internal/execution/model_execution_manager_impl.h"
#include "components/segmentation_platform/internal/scheduler/model_execution_scheduler.h"

class PrefService;

namespace segmentation_platform {
namespace processing {
class FeatureListQueryProcessor;
}

struct Config;
struct PlatformOptions;
class ModelExecutor;
class ModelProviderFactory;
class SignalHandler;
class StorageService;
class TrainingDataCollector;

// Handles feature processing and model execution.
class ExecutionService {
 public:
  ExecutionService();
  ~ExecutionService();

  ExecutionService(ExecutionService&) = delete;
  ExecutionService& operator=(ExecutionService&) = delete;

  void InitForTesting(
      std::unique_ptr<processing::FeatureListQueryProcessor> feature_processor,
      std::unique_ptr<ModelExecutor> executor,
      std::unique_ptr<ModelExecutionScheduler> scheduler);

  void Initialize(
      StorageService* storage_service,
      SignalHandler* signal_handler,
      base::Clock* clock,
      ModelExecutionManager::SegmentationModelUpdatedCallback callback,
      scoped_refptr<base::SequencedTaskRunner> task_runner,
      const base::flat_set<OptimizationTarget>& all_segment_ids,
      ModelProviderFactory* model_provider_factory,
      std::vector<ModelExecutionScheduler::Observer*>&& observers,
      const PlatformOptions& platform_options,
      std::vector<std::unique_ptr<Config>>* configs,
      PrefService* profile_prefs);

  // Called whenever a new or updated model is available. Must be a valid
  // SegmentInfo with valid metadata and features.
  void OnNewModelInfoReady(const proto::SegmentInfo& segment_info);

  using ModelExecutionCallback =
      base::OnceCallback<void(const std::pair<float, ModelExecutionStatus>&)>;
  struct ExecutionRequest {
    ExecutionRequest();
    ~ExecutionRequest();

    // Required: The segment info to use for model execution.
    const proto::SegmentInfo* segment_info = nullptr;
    // Required: The model provider used to execute the model.
    ModelProvider* model_provider = nullptr;

    // Save result of execution to the database.
    bool save_result_to_db = false;
    // Record metrics for default model instead of optimization_guide based
    // models.
    bool record_metrics_for_default = false;
    // returns result as by callback, to be used when `save_result_to_db` is
    // false.
    ModelExecutionCallback callback;
  };

  void RequestModelExecution(std::unique_ptr<ExecutionRequest> request);

  void OverwriteModelExecutionResult(
      optimization_guide::proto::OptimizationTarget segment_id,
      const std::pair<float, ModelExecutionStatus>& result);

  // Refreshes model results for all eligible models.
  void RefreshModelResults();

  // Executes daily maintenance and collection tasks.
  void RunDailyTasks(bool is_startup);

 private:
  // Training/inference input data generation.
  std::unique_ptr<processing::FeatureListQueryProcessor>
      feature_list_query_processor_;

  // Traing data collection logic.
  std::unique_ptr<TrainingDataCollector> training_data_collector_;

  // Utility to execute model and return result.
  std::unique_ptr<ModelExecutor> model_executor_;

  // Model execution.
  std::unique_ptr<ModelExecutionManagerImpl> model_execution_manager_;

  // Model execution scheduling logic.
  std::unique_ptr<ModelExecutionScheduler> model_execution_scheduler_;
};

}  // namespace segmentation_platform

#endif  // COMPONENTS_SEGMENTATION_PLATFORM_INTERNAL_SCHEDULER_EXECUTION_SERVICE_H_
