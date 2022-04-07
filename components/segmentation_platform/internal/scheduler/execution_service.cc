// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/segmentation_platform/internal/scheduler/execution_service.h"

#include "components/segmentation_platform/internal/data_collection/training_data_collector.h"
#include "components/segmentation_platform/internal/database/segment_info_database.h"
#include "components/segmentation_platform/internal/database/signal_database.h"
#include "components/segmentation_platform/internal/execution/feature_aggregator_impl.h"
#include "components/segmentation_platform/internal/execution/feature_list_query_processor.h"
#include "components/segmentation_platform/internal/execution/model_execution_manager_factory.h"
#include "components/segmentation_platform/internal/scheduler/model_execution_scheduler_impl.h"
#include "components/segmentation_platform/internal/signals/signal_handler.h"

namespace segmentation_platform {

ExecutionService::ExecutionService() = default;
ExecutionService::~ExecutionService() = default;

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

  model_execution_manager_ = CreateModelExecutionManager(
      model_provider_factory, task_runner, all_segment_ids, clock,
      segment_info_database, signal_database,
      feature_list_query_processor_.get(), callback);

  model_execution_scheduler_ = std::make_unique<ModelExecutionSchedulerImpl>(
      std::move(observers), segment_info_database, signal_storage_config,
      model_execution_manager_.get(), all_segment_ids, clock, platform_options);

  model_execution_scheduler_->RequestModelExecutionForEligibleSegments(
      /*expired_only=*/true);
}

void ExecutionService::OnNewModelInfoReady(
    const proto::SegmentInfo& segment_info) {
  model_execution_scheduler_->OnNewModelInfoReady(segment_info);
}

}  // namespace segmentation_platform
