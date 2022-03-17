// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/segmentation_platform/internal/segmentation_platform_service_impl.h"

#include <string>

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/task/sequenced_task_runner.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/time/clock.h"
#include "components/leveldb_proto/public/proto_database_provider.h"
#include "components/leveldb_proto/public/shared_proto_database_client_list.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/segmentation_platform/internal/constants.h"
#include "components/segmentation_platform/internal/data_collection/training_data_collector.h"
#include "components/segmentation_platform/internal/database/database_maintenance_impl.h"
#include "components/segmentation_platform/internal/database/metadata_utils.h"
#include "components/segmentation_platform/internal/database/segment_info_database.h"
#include "components/segmentation_platform/internal/database/signal_database_impl.h"
#include "components/segmentation_platform/internal/database/signal_storage_config.h"
#include "components/segmentation_platform/internal/execution/feature_aggregator_impl.h"
#include "components/segmentation_platform/internal/execution/feature_list_query_processor.h"
#include "components/segmentation_platform/internal/execution/model_execution_manager.h"
#include "components/segmentation_platform/internal/execution/model_execution_manager_factory.h"
#include "components/segmentation_platform/internal/platform_options.h"
#include "components/segmentation_platform/internal/proto/model_prediction.pb.h"
#include "components/segmentation_platform/internal/proto/signal.pb.h"
#include "components/segmentation_platform/internal/proto/signal_storage_config.pb.h"
#include "components/segmentation_platform/internal/scheduler/model_execution_scheduler_impl.h"
#include "components/segmentation_platform/internal/selection/segment_score_provider.h"
#include "components/segmentation_platform/internal/selection/segment_selector_impl.h"
#include "components/segmentation_platform/internal/selection/segmentation_result_prefs.h"
#include "components/segmentation_platform/internal/signals/histogram_signal_handler.h"
#include "components/segmentation_platform/internal/signals/history_service_observer.h"
#include "components/segmentation_platform/internal/signals/signal_filter_processor.h"
#include "components/segmentation_platform/internal/signals/user_action_signal_handler.h"
#include "components/segmentation_platform/internal/stats.h"
#include "components/segmentation_platform/internal/ukm_data_manager.h"
#include "components/segmentation_platform/public/config.h"
#include "components/segmentation_platform/public/model_provider.h"

using optimization_guide::proto::OptimizationTarget;

namespace segmentation_platform {
namespace {
const base::FilePath::CharType kSegmentInfoDBName[] =
    FILE_PATH_LITERAL("SegmentInfoDB");
const base::FilePath::CharType kSignalDBName[] = FILE_PATH_LITERAL("SignalDB");
const base::FilePath::CharType kSignalStorageConfigDBName[] =
    FILE_PATH_LITERAL("SignalStorageConfigDB");
const base::TimeDelta kDatabaseMaintenanceDelay = base::Seconds(30);
}  // namespace

SegmentationPlatformServiceImpl::SegmentationPlatformServiceImpl(
    std::unique_ptr<ModelProviderFactory> model_provider,
    leveldb_proto::ProtoDatabaseProvider* db_provider,
    const base::FilePath& storage_dir,
    UkmDataManager* ukm_data_manager,
    PrefService* pref_service,
    history::HistoryService* history_service,
    const scoped_refptr<base::SequencedTaskRunner>& task_runner,
    base::Clock* clock,
    std::vector<std::unique_ptr<Config>> configs)
    : SegmentationPlatformServiceImpl(
          db_provider->GetDB<proto::SegmentInfo>(
              leveldb_proto::ProtoDbType::SEGMENT_INFO_DATABASE,
              storage_dir.Append(kSegmentInfoDBName),
              task_runner),
          db_provider->GetDB<proto::SignalData>(
              leveldb_proto::ProtoDbType::SIGNAL_DATABASE,
              storage_dir.Append(kSignalDBName),
              task_runner),
          db_provider->GetDB<proto::SignalStorageConfigs>(
              leveldb_proto::ProtoDbType::SIGNAL_STORAGE_CONFIG_DATABASE,
              storage_dir.Append(kSignalStorageConfigDBName),
              task_runner),
          ukm_data_manager,
          std::move(model_provider),
          pref_service,
          history_service,
          task_runner,
          clock,
          std::move(configs)) {}

SegmentationPlatformServiceImpl::SegmentationPlatformServiceImpl(
    std::unique_ptr<leveldb_proto::ProtoDatabase<proto::SegmentInfo>>
        segment_db,
    std::unique_ptr<leveldb_proto::ProtoDatabase<proto::SignalData>> signal_db,
    std::unique_ptr<leveldb_proto::ProtoDatabase<proto::SignalStorageConfigs>>
        signal_storage_config_db,
    UkmDataManager* ukm_data_manager,
    std::unique_ptr<ModelProviderFactory> model_provider,
    PrefService* pref_service,
    history::HistoryService* history_service,
    const scoped_refptr<base::SequencedTaskRunner>& task_runner,
    base::Clock* clock,
    std::vector<std::unique_ptr<Config>> configs)
    : model_provider_factory_(std::move(model_provider)),
      task_runner_(task_runner),
      clock_(clock),
      platform_options_(PlatformOptions::CreateDefault()),
      configs_(std::move(configs)),
      ukm_data_manager_(ukm_data_manager) {
  // Construct databases.
  segment_info_database_ =
      std::make_unique<SegmentInfoDatabase>(std::move(segment_db));
  signal_database_ =
      std::make_unique<SignalDatabaseImpl>(std::move(signal_db), clock);
  signal_storage_config_ = std::make_unique<SignalStorageConfig>(
      std::move(signal_storage_config_db), clock);
  segmentation_result_prefs_ =
      std::make_unique<SegmentationResultPrefs>(pref_service);
  ukm_data_manager_->AddRef();

  // Construct signal processors.
  user_action_signal_handler_ =
      std::make_unique<UserActionSignalHandler>(signal_database_.get());
  histogram_signal_handler_ =
      std::make_unique<HistogramSignalHandler>(signal_database_.get());
  signal_filter_processor_ = std::make_unique<SignalFilterProcessor>(
      segment_info_database_.get(), user_action_signal_handler_.get(),
      histogram_signal_handler_.get(), ukm_data_manager_);

  if (ukm_data_manager_->IsUkmEngineEnabled() && history_service) {
    // If UKM engine is enabled and history service is not available, then we
    // would write metrics without URLs to the database, which is OK.
    history_service_observer_ = std::make_unique<HistoryServiceObserver>(
        history_service, ukm_data_manager_->GetOrCreateUrlHandler());
  }

  for (const auto& config : configs_) {
    segment_selectors_[config->segmentation_key] =
        std::make_unique<SegmentSelectorImpl>(
            segment_info_database_.get(), signal_storage_config_.get(),
            segmentation_result_prefs_.get(), config.get(), clock,
            platform_options_);
  }

  proxy_ = std::make_unique<ServiceProxyImpl>(segment_info_database_.get(),
                                              signal_storage_config_.get(),
                                              &configs_, &segment_selectors_);
  for (const auto& config : configs_) {
    for (const auto& segment_id : config->segment_ids)
      all_segment_ids_.insert(segment_id);
  }

  segment_score_provider_ =
      SegmentScoreProvider::Create(segment_info_database_.get());

  database_maintenance_ = std::make_unique<DatabaseMaintenanceImpl>(
      all_segment_ids_, clock, segment_info_database_.get(),
      signal_database_.get(), signal_storage_config_.get());

  // Kick off initialization of all databases. Internal operations will be
  // delayed until they are all complete.
  segment_info_database_->Initialize(base::BindOnce(
      &SegmentationPlatformServiceImpl::OnSegmentInfoDatabaseInitialized,
      weak_ptr_factory_.GetWeakPtr()));
  signal_database_->Initialize(base::BindOnce(
      &SegmentationPlatformServiceImpl::OnSignalDatabaseInitialized,
      weak_ptr_factory_.GetWeakPtr()));
  signal_storage_config_->InitAndLoad(base::BindOnce(
      &SegmentationPlatformServiceImpl::OnSignalStorageConfigInitialized,
      weak_ptr_factory_.GetWeakPtr()));
}

SegmentationPlatformServiceImpl::~SegmentationPlatformServiceImpl() {
  history_service_observer_.reset();
  ukm_data_manager_->RemoveRef();
}

void SegmentationPlatformServiceImpl::GetSelectedSegment(
    const std::string& segmentation_key,
    SegmentSelectionCallback callback) {
  CHECK(segment_selectors_.find(segmentation_key) != segment_selectors_.end());
  auto& selector = segment_selectors_.at(segmentation_key);
  selector->GetSelectedSegment(std::move(callback));
}

SegmentSelectionResult SegmentationPlatformServiceImpl::GetCachedSegmentResult(
    const std::string& segmentation_key) {
  CHECK(segment_selectors_.find(segmentation_key) != segment_selectors_.end());
  auto& selector = segment_selectors_.at(segmentation_key);
  return selector->GetCachedSegmentResult();
}

void SegmentationPlatformServiceImpl::EnableMetrics(
    bool signal_collection_allowed) {
  signal_filter_processor_->EnableMetrics(signal_collection_allowed);
}

ServiceProxy* SegmentationPlatformServiceImpl::GetServiceProxy() {
  return proxy_.get();
}

void SegmentationPlatformServiceImpl::OnSegmentInfoDatabaseInitialized(
    bool success) {
  segment_info_database_initialized_ = success;
  segment_score_provider_->Initialize(base::DoNothing());
  MaybeRunPostInitializationRoutines();
}

void SegmentationPlatformServiceImpl::OnSignalDatabaseInitialized(
    bool success) {
  signal_database_initialized_ = success;
  MaybeRunPostInitializationRoutines();
}

void SegmentationPlatformServiceImpl::OnSignalStorageConfigInitialized(
    bool success) {
  signal_storage_config_initialized_ = success;
  MaybeRunPostInitializationRoutines();
}

bool SegmentationPlatformServiceImpl::IsInitializationFinished() const {
  return segment_info_database_initialized_.has_value() &&
         signal_database_initialized_.has_value() &&
         signal_storage_config_initialized_.has_value();
}

void SegmentationPlatformServiceImpl::MaybeRunPostInitializationRoutines() {
  if (!IsInitializationFinished())
    return;

  bool init_success = segment_info_database_initialized_ &&
                      signal_database_initialized_ &&
                      signal_storage_config_initialized_;

  OnServiceStatusChanged();
  if (!init_success) {
    for (const auto& config : configs_) {
      stats::RecordSegmentSelectionFailure(
          config->segmentation_key,
          stats::SegmentationSelectionFailureReason::kDBInitFailure);
    }
    return;
  }

  feature_list_query_processor_ = std::make_unique<FeatureListQueryProcessor>(
      signal_database_.get(), std::make_unique<FeatureAggregatorImpl>());

  training_data_collector_ = TrainingDataCollector::Create(
      segment_info_database_.get(), feature_list_query_processor_.get(),
      histogram_signal_handler_.get(), signal_storage_config_.get(), clock_);
  training_data_collector_->OnServiceInitialized();

  model_execution_manager_ = CreateModelExecutionManager(
      std::move(model_provider_factory_), task_runner_, all_segment_ids_,
      clock_, segment_info_database_.get(), signal_database_.get(),
      feature_list_query_processor_.get(),
      base::BindRepeating(
          &SegmentationPlatformServiceImpl::OnSegmentationModelUpdated,
          weak_ptr_factory_.GetWeakPtr()));

  std::vector<ModelExecutionSchedulerImpl::Observer*> observers;
  for (auto& key_and_selector : segment_selectors_)
    observers.push_back(key_and_selector.second.get());
  model_execution_scheduler_ = std::make_unique<ModelExecutionSchedulerImpl>(
      std::move(observers), segment_info_database_.get(),
      signal_storage_config_.get(), model_execution_manager_.get(),
      all_segment_ids_, clock_, platform_options_);

  signal_filter_processor_->OnSignalListUpdated();
  model_execution_scheduler_->RequestModelExecutionForEligibleSegments(
      /*expired_only=*/true);

  // Initiate database maintenance tasks with a small delay.
  base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(
          &SegmentationPlatformServiceImpl::OnExecuteDatabaseMaintenanceTasks,
          weak_ptr_factory_.GetWeakPtr()),
      kDatabaseMaintenanceDelay);

  proxy_->SetModelExecutionScheduler(model_execution_scheduler_.get());
}

void SegmentationPlatformServiceImpl::OnSegmentationModelUpdated(
    proto::SegmentInfo segment_info) {
  DCHECK(metadata_utils::ValidateSegmentInfoMetadataAndFeatures(segment_info) ==
         metadata_utils::ValidationResult::kValidationSuccess);

  signal_storage_config_->OnSignalCollectionStarted(
      segment_info.model_metadata());
  signal_filter_processor_->OnSignalListUpdated();

  model_execution_scheduler_->OnNewModelInfoReady(segment_info);

  // Update the service status for proxy.
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::BindOnce(&SegmentationPlatformServiceImpl::OnServiceStatusChanged,
                     weak_ptr_factory_.GetWeakPtr()));
}

void SegmentationPlatformServiceImpl::OnExecuteDatabaseMaintenanceTasks() {
  database_maintenance_->ExecuteMaintenanceTasks();
}

void SegmentationPlatformServiceImpl::OnServiceStatusChanged() {
  int status = static_cast<int>(ServiceStatus::kUninitialized);
  if (segment_info_database_initialized_)
    status |= static_cast<int>(ServiceStatus::kSegmentationInfoDbInitialized);
  if (signal_database_initialized_)
    status |= static_cast<int>(ServiceStatus::kSignalDbInitialized);
  if (signal_storage_config_initialized_) {
    status |= static_cast<int>(ServiceStatus::kSignalStorageConfigInitialized);
  }

  proxy_->OnServiceStatusChanged(IsInitializationFinished(), status);
}

// static
void SegmentationPlatformService::RegisterProfilePrefs(
    PrefRegistrySimple* registry) {
  registry->RegisterDictionaryPref(kSegmentationResultPref);
}

}  // namespace segmentation_platform
