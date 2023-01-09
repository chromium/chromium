// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/segmentation_platform/internal/scheduler/execution_service.h"

#include "base/task/sequenced_task_runner.h"
#include "components/prefs/pref_service.h"
#include "components/segmentation_platform/internal/database/storage_service.h"
#include "components/segmentation_platform/internal/execution/default_model_manager.h"
#include "components/segmentation_platform/internal/execution/execution_request.h"
#include "components/segmentation_platform/internal/execution/model_executor_impl.h"
#include "components/segmentation_platform/internal/execution/processing/feature_aggregator_impl.h"
#include "components/segmentation_platform/internal/execution/processing/feature_list_query_processor.h"
#include "components/segmentation_platform/internal/scheduler/model_execution_scheduler_impl.h"
#include "components/segmentation_platform/internal/segmentation_ukm_helper.h"
#include "components/segmentation_platform/internal/signals/signal_handler.h"
#include "components/segmentation_platform/public/config.h"
#include "components/segmentation_platform/public/input_delegate.h"
#include "components/segmentation_platform/public/model_provider.h"

namespace segmentation_platform {

ExecutionService::ExecutionService() = default;
ExecutionService::~ExecutionService() = default;

void ExecutionService::InitForTesting(
    std::unique_ptr<processing::FeatureListQueryProcessor> feature_processor,
    std::unique_ptr<ModelExecutor> executor,
    std::unique_ptr<ModelExecutionScheduler> scheduler,
    std::unique_ptr<ModelExecutionManager> execution_manager) {
  feature_list_query_processor_ = std::move(feature_processor);
  model_executor_ = std::move(executor);
  model_execution_scheduler_ = std::move(scheduler);
  model_execution_manager_ = std::move(execution_manager);
}

void ExecutionService::Initialize(
    StorageService* storage_service,
    SignalHandler* signal_handler,
    base::Clock* clock,
    ModelExecutionManager::SegmentationModelUpdatedCallback callback,
    scoped_refptr<base::SequencedTaskRunner> task_runner,
    const base::flat_set<SegmentId>& all_segment_ids,
    ModelProviderFactory* model_provider_factory,
    std::vector<ModelExecutionScheduler::Observer*>&& observers,
    const PlatformOptions& platform_options,
    std::unique_ptr<processing::InputDelegateHolder> input_delegate_holder,
    std::vector<std::unique_ptr<Config>>* configs,
    PrefService* profile_prefs) {
  storage_service_ = storage_service;

  feature_list_query_processor_ =
      std::make_unique<processing::FeatureListQueryProcessor>(
          storage_service, std::move(input_delegate_holder),
          std::make_unique<processing::FeatureAggregatorImpl>());

  training_data_collector_ = TrainingDataCollector::Create(
      feature_list_query_processor_.get(),
      signal_handler->deprecated_histogram_signal_handler(), storage_service,
      configs, profile_prefs, clock);

  model_executor_ = std::make_unique<ModelExecutorImpl>(
      clock, feature_list_query_processor_.get());

  model_execution_manager_ = std::make_unique<ModelExecutionManagerImpl>(
      all_segment_ids, model_provider_factory, clock,
      storage_service->segment_info_database(), callback);

  model_execution_scheduler_ = std::make_unique<ModelExecutionSchedulerImpl>(
      std::move(observers), storage_service->segment_info_database(),
      storage_service->signal_storage_config(), model_execution_manager_.get(),
      model_executor_.get(), all_segment_ids, clock, platform_options);
}

void ExecutionService::OnNewModelInfoReady(
    const proto::SegmentInfo& segment_info) {
  model_execution_scheduler_->OnNewModelInfoReady(segment_info);
}

ModelProvider* ExecutionService::GetModelProvider(SegmentId segment_id) {
  return model_execution_manager_->GetProvider(segment_id);
}

void ExecutionService::RequestModelExecution(
    std::unique_ptr<ExecutionRequest> request) {
  DCHECK(request->segment_info);
  if (request->save_result_to_db) {
    DCHECK(!request->record_metrics_for_default)
        << "cannot record metics for default model from scheduler";
    // TODO(ssid): Scheduler should use the `request` instead of fetching the
    // model provider.
    DCHECK(!request->model_provider)
        << "using custom model provider to save result is not supported";
    DCHECK(request->callback.is_null())
        << "save_result_to_db + callback cannot be set together";
    DCHECK(!request->input_context)
        << "saving results keyed on input context is not supported";
    model_execution_scheduler_->RequestModelExecution(*request->segment_info);
    return;
  }

  DCHECK(!request->callback.is_null());
  model_executor_->ExecuteModel(std::move(request));
}

void ExecutionService::OverwriteModelExecutionResult(
    proto::SegmentId segment_id,
    const std::pair<float, ModelExecutionStatus>& result) {
  // TODO(ritikagup): Change the use of this according to MultiOutputModel.
  auto execution_result = std::make_unique<ModelExecutionResult>(
      ModelProvider::Request(), ModelProvider::Response(1, result.first));
  model_execution_scheduler_->OnModelExecutionCompleted(
      segment_id, std::move(execution_result));
}

void ExecutionService::RefreshModelResults() {
  model_execution_scheduler_->RequestModelExecutionForEligibleSegments(
      /*expired_only=*/true);
}

void ExecutionService::RunDailyTasks(bool is_startup) {
  RefreshModelResults();

  if (is_startup) {
    // This will trigger data collection after initialization finishes.
    training_data_collector_->OnServiceInitialized();
  } else {
    training_data_collector_->ReportCollectedContinuousTrainingData();
  }
}

}  // namespace segmentation_platform
