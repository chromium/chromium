// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/segmentation_platform/internal/scheduler/execution_service.h"

#include "base/memory/raw_ptr.h"
#include "base/task/sequenced_task_runner.h"
#include "components/prefs/pref_service.h"
#include "components/segmentation_platform/internal/database/cached_result_provider.h"
#include "components/segmentation_platform/internal/database/storage_service.h"
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
#include "components/segmentation_platform/public/proto/model_metadata.pb.h"

namespace segmentation_platform {

ExecutionService::ExecutionService() = default;
ExecutionService::~ExecutionService() = default;

void ExecutionService::InitForTesting(
    std::unique_ptr<processing::FeatureListQueryProcessor> feature_processor,
    std::unique_ptr<ModelExecutor> executor,
    std::unique_ptr<ModelExecutionScheduler> scheduler,
    ModelManager* model_manager) {
  feature_list_query_processor_ = std::move(feature_processor);
  model_executor_ = std::move(executor);
  model_execution_scheduler_ = std::move(scheduler);
  model_manager_ = model_manager;
}

void ExecutionService::Initialize(
    StorageService* storage_service,
    SignalHandler* signal_handler,
    base::Clock* clock,
    scoped_refptr<base::SequencedTaskRunner> task_runner,
    const base::flat_set<SegmentId>& legacy_output_segment_ids,
    ModelProviderFactory* model_provider_factory,
    std::vector<raw_ptr<ModelExecutionScheduler::Observer,
                        VectorExperimental>>&& observers,
    const PlatformOptions& platform_options,
    std::unique_ptr<processing::InputDelegateHolder> input_delegate_holder,
    PrefService* profile_prefs,
    CachedResultProvider* cached_result_provider) {
  storage_service_ = storage_service;

  feature_list_query_processor_ =
      std::make_unique<processing::FeatureListQueryProcessor>(
          storage_service, std::move(input_delegate_holder),
          std::make_unique<processing::FeatureAggregatorImpl>());

  training_data_collector_ = TrainingDataCollector::Create(
      platform_options, feature_list_query_processor_.get(),
      signal_handler->deprecated_histogram_signal_handler(),
      signal_handler->user_action_signal_handler(), storage_service,
      profile_prefs, clock, cached_result_provider);

  model_executor_ = std::make_unique<ModelExecutorImpl>(
      clock, storage_service->segment_info_database(),
      feature_list_query_processor_.get());

  model_manager_ = storage_service->model_manager();

  model_execution_scheduler_ = std::make_unique<ModelExecutionSchedulerImpl>(
      std::move(observers), storage_service->segment_info_database(),
      storage_service->signal_storage_config(), model_manager_,
      model_executor_.get(), legacy_output_segment_ids, clock,
      platform_options);
}

void ExecutionService::OnNewModelInfoReadyLegacy(
    const proto::SegmentInfo& segment_info) {
  // TODO(crbug.com/40258591): Change path flow as
  // SPSI->RRM->EE::RequestModelExecution and migrate
  // MES::CancelOutstandingExecutionRequests() to EE.
  model_execution_scheduler_->OnNewModelInfoReady(segment_info);
}

ModelProvider* ExecutionService::GetModelProvider(SegmentId segment_id,
                                                  ModelSource model_source) {
  return model_manager_->GetModelProvider(segment_id, model_source);
}

void ExecutionService::RequestModelExecution(
    std::unique_ptr<ExecutionRequest> request) {
  DCHECK_NE(request->segment_id, SegmentId::OPTIMIZATION_TARGET_UNKNOWN);
  DCHECK_NE(request->model_source, proto::ModelSource::UNKNOWN_MODEL_SOURCE);
  DCHECK(!request->callback.is_null());
  model_executor_->ExecuteModel(std::move(request));
}

void ExecutionService::OverwriteModelExecutionResult(
    proto::SegmentId segment_id,
    const std::pair<float, ModelExecutionStatus>& result) {
  // TODO(ritikagup): Change the use of this according to MultiOutputModel.
  auto execution_result = std::make_unique<ModelExecutionResult>(
      ModelProvider::Request(), ModelProvider::Response(1, result.first));
  proto::SegmentInfo segment_info;
  segment_info.set_segment_id(segment_id);
  model_execution_scheduler_->OnModelExecutionCompleted(
      segment_info, std::move(execution_result));
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
