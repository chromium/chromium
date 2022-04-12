// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/segmentation_platform/internal/scheduler/execution_service.h"

#include "components/segmentation_platform/internal/data_collection/training_data_collector.h"
#include "components/segmentation_platform/internal/database/segment_info_database.h"
#include "components/segmentation_platform/internal/database/signal_database.h"
#include "components/segmentation_platform/internal/execution/feature_aggregator_impl.h"
#include "components/segmentation_platform/internal/execution/feature_list_query_processor.h"
#include "components/segmentation_platform/internal/execution/model_executor_impl.h"
#include "components/segmentation_platform/internal/scheduler/model_execution_scheduler_impl.h"
#include "components/segmentation_platform/internal/signals/signal_handler.h"

namespace segmentation_platform {

ExecutionService::ExecutionRequest::ExecutionRequest() = default;
ExecutionService::ExecutionRequest::~ExecutionRequest() = default;

ExecutionService::ExecutionService() = default;
ExecutionService::~ExecutionService() = default;

void ExecutionService::InitForTesting(
    std::unique_ptr<FeatureListQueryProcessor> feature_processor,
    std::unique_ptr<ModelExecutor> executor,
    std::unique_ptr<ModelExecutionScheduler> scheduler) {
  feature_list_query_processor_ = std::move(feature_processor);
  model_executor_ = std::move(executor);
  model_execution_scheduler_ = std::move(scheduler);
}

void ExecutionService::Initialize(
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
    const PlatformOptions& platform_options) {
  feature_list_query_processor_ = std::make_unique<FeatureListQueryProcessor>(
      signal_database, std::make_unique<FeatureAggregatorImpl>());

  training_data_collector_ = TrainingDataCollector::Create(
      segment_info_database, feature_list_query_processor_.get(),
      signal_handler->deprecated_histogram_signal_handler(),
      signal_storage_config, clock);
  training_data_collector_->OnServiceInitialized();

  model_executor_ = std::make_unique<ModelExecutorImpl>(
      clock, feature_list_query_processor_.get());

  model_execution_manager_ = std::make_unique<ModelExecutionManagerImpl>(
      all_segment_ids, model_provider_factory, clock, segment_info_database,
      callback);

  model_execution_scheduler_ = std::make_unique<ModelExecutionSchedulerImpl>(
      std::move(observers), segment_info_database, signal_storage_config,
      model_execution_manager_.get(), model_executor_.get(), all_segment_ids,
      clock, platform_options);

  model_execution_scheduler_->RequestModelExecutionForEligibleSegments(
      /*expired_only=*/true);
}

void ExecutionService::OnNewModelInfoReady(
    const proto::SegmentInfo& segment_info) {
  model_execution_scheduler_->OnNewModelInfoReady(segment_info);
}

void ExecutionService::RequestModelExecution(
    std::unique_ptr<ExecutionRequest> request) {
  DCHECK(request->segment_info);
  if (!request->callback.is_null() && request->model_provider) {
    DCHECK(!request->save_result_to_db)
        << "save_result_to_db + callback cannot be set together";
    model_executor_->ExecuteModel(
        *request->segment_info, request->model_provider,
        request->record_metrics_for_default, std::move(request->callback));
  } else if (request->save_result_to_db) {
    DCHECK(!request->record_metrics_for_default)
        << "cannot record metics for default model from scheduler";
    DCHECK(!request->model_provider)
        << "using custom model provider to save result is not supported";
    DCHECK(request->callback.is_null())
        << "save_result_to_db + callback cannot be set together";
    model_execution_scheduler_->RequestModelExecution(*request->segment_info);
  } else {
    NOTREACHED()
        << "On demand xecution of non-default model is not yet supported";
  }
}

void ExecutionService::OverwriteModelExecutionResult(
    optimization_guide::proto::OptimizationTarget segment_id,
    const std::pair<float, ModelExecutionStatus>& result) {
  model_execution_scheduler_->OnModelExecutionCompleted(segment_id, result);
}

}  // namespace segmentation_platform
