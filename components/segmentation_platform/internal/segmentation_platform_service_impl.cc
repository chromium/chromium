// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/segmentation_platform/internal/segmentation_platform_service_impl.h"

#include <memory>
#include <string>

#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/metrics/histogram_functions.h"
#include "base/system/sys_info.h"
#include "base/task/single_thread_task_runner.h"
#include "base/time/clock.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/segmentation_platform/internal/config_parser.h"
#include "components/segmentation_platform/internal/constants.h"
#include "components/segmentation_platform/internal/database/storage_service.h"
#include "components/segmentation_platform/internal/database_client_impl.h"
#include "components/segmentation_platform/internal/execution/processing/sync_device_info_observer.h"
#include "components/segmentation_platform/internal/platform_options.h"
#include "components/segmentation_platform/internal/proto/model_prediction.pb.h"
#include "components/segmentation_platform/internal/scheduler/model_execution_scheduler_impl.h"
#include "components/segmentation_platform/internal/selection/request_dispatcher.h"
#include "components/segmentation_platform/internal/selection/segment_score_provider.h"
#include "components/segmentation_platform/internal/selection/segment_selector_impl.h"
#include "components/segmentation_platform/internal/selection/segmentation_result_prefs.h"
#include "components/segmentation_platform/internal/stats.h"
#include "components/segmentation_platform/public/config.h"
#include "components/segmentation_platform/public/features.h"
#include "components/segmentation_platform/public/field_trial_register.h"
#include "components/segmentation_platform/public/input_context.h"
#include "components/segmentation_platform/public/input_delegate.h"
#include "services/metrics/public/cpp/ukm_source_id.h"

namespace segmentation_platform {

namespace {

using proto::SegmentId;

}  // namespace

SegmentationPlatformServiceImpl::InitParams::InitParams() = default;
SegmentationPlatformServiceImpl::InitParams::~InitParams() = default;

SegmentationPlatformServiceImpl::SegmentationPlatformServiceImpl(
    std::unique_ptr<InitParams> init_params)
    : model_provider_factory_(std::move(init_params->model_provider)),
      task_runner_(init_params->task_runner),
      clock_(init_params->clock.get()),
      platform_options_(PlatformOptions::CreateDefault()),
      input_delegate_holder_(std::move(init_params->input_delegate_holder)),
      field_trial_register_(std::move(init_params->field_trial_register)),
      field_trial_recorder_(
          std::make_unique<FieldTrialRecorder>(field_trial_register_.get())),
      profile_prefs_(init_params->profile_prefs.get()),
      creation_time_(clock_->Now()) {
  base::UmaHistogramMediumTimes(
      "SegmentationPlatform.Init.ProcessCreationToServiceCreationLatency",
      base::SysInfo::Uptime());

  DCHECK(task_runner_);
  DCHECK(clock);
  DCHECK(init_params->profile_prefs);

  if (init_params->storage_service) {
    // Test only:
    storage_service_ = std::move(init_params->storage_service);
    storage_service_->model_manager()
        ->SetSegmentationModelUpdatedCallbackForTesting(base::BindRepeating(
            &SegmentationPlatformServiceImpl::OnSegmentationModelUpdated,
            weak_ptr_factory_.GetWeakPtr()));
  } else {
    DCHECK(model_provider_factory_ && init_params->db_provider);
    DCHECK(!init_params->storage_dir.empty() && init_params->ukm_data_manager);
    storage_service_ = std::make_unique<StorageService>(
        init_params->storage_dir, init_params->db_provider,
        init_params->task_runner, init_params->clock,
        init_params->ukm_data_manager, std::move(init_params->configs),
        model_provider_factory_.get(), profile_prefs_, init_params->profile_id,
        base::BindRepeating(
            &SegmentationPlatformServiceImpl::OnSegmentationModelUpdated,
            weak_ptr_factory_.GetWeakPtr()));
  }

  const auto* config_holder = storage_service_->config_holder();

  prefs_migrator_ = std::make_unique<PrefsMigrator>(
      init_params->profile_prefs.get(), storage_service_->client_result_prefs(),
      config_holder->configs());

  // Construct signal processors.
  DCHECK(!init_params->profile_id.empty());
  signal_handler_.Initialize(
      storage_service_.get(), init_params->history_service, profile_prefs_,
      config_holder->all_segment_ids(), init_params->profile_id,
      base::BindRepeating(
          &SegmentationPlatformServiceImpl::OnModelRefreshNeeded,
          weak_ptr_factory_.GetWeakPtr()));

  prefs_migrator_->MigrateOldPrefsToNewPrefs();

  field_trial_recorder_->RecordFieldTrialAtStartup(
      config_holder->configs(), storage_service_->cached_result_provider());

  request_dispatcher_ =
      std::make_unique<RequestDispatcher>(storage_service_.get());

  for (const auto& config : config_holder->configs()) {
    if (!metadata_utils::ConfigUsesLegacyOutput(config.get())) {
      continue;
    }
    segment_selectors_[config->segmentation_key] =
        std::make_unique<SegmentSelectorImpl>(
            storage_service_->segment_info_database(),
            storage_service_->signal_storage_config(),
            init_params->profile_prefs, config.get(),
            field_trial_register_.get(), init_params->clock, platform_options_);
  }

  proxy_ = std::make_unique<ServiceProxyImpl>(
      storage_service_->segment_info_database(),
      storage_service_->signal_storage_config(), &config_holder->configs(),
      platform_options_, &segment_selectors_);
  segment_score_provider_ =
      SegmentScoreProvider::Create(storage_service_->segment_info_database(),
                                   config_holder->all_segment_ids());

  // Kick off initialization of all databases. Internal operations will be
  // delayed until they are all complete.
  storage_service_->Initialize(
      base::BindOnce(&SegmentationPlatformServiceImpl::OnDatabaseInitialized,
                     weak_ptr_factory_.GetWeakPtr()));

  // Create sync device info observer.
  input_delegate_holder_->SetDelegate(
      proto::CustomInput::FILL_SYNC_DEVICE_INFO,
      std::make_unique<processing::SyncDeviceInfoObserver>(
          init_params->device_info_tracker));

  result_refresh_manager_ = std::make_unique<ResultRefreshManager>(
      config_holder, std::move(storage_service_->cached_result_writer()),
      platform_options_);
}

SegmentationPlatformServiceImpl::~SegmentationPlatformServiceImpl() {
  signal_handler_.TearDown();
  ClearAllUserData();
}

void SegmentationPlatformServiceImpl::GetSelectedSegment(
    const std::string& segmentation_key,
    SegmentSelectionCallback callback) {
  CHECK(segment_selectors_.find(segmentation_key) != segment_selectors_.end());
  auto& selector = segment_selectors_.at(segmentation_key);
  selector->GetSelectedSegment(std::move(callback));
}

void SegmentationPlatformServiceImpl::GetClassificationResult(
    const std::string& segmentation_key,
    const PredictionOptions& prediction_options,
    scoped_refptr<InputContext> input_context,
    ClassificationResultCallback callback) {
  request_dispatcher_->GetClassificationResult(
      segmentation_key, prediction_options, input_context, std::move(callback));
}

void SegmentationPlatformServiceImpl::GetAnnotatedNumericResult(
    const std::string& segmentation_key,
    const PredictionOptions& prediction_options,
    scoped_refptr<InputContext> input_context,
    AnnotatedNumericResultCallback callback) {
  request_dispatcher_->GetAnnotatedNumericResult(
      segmentation_key, prediction_options, input_context, std::move(callback));
}

SegmentSelectionResult SegmentationPlatformServiceImpl::GetCachedSegmentResult(
    const std::string& segmentation_key) {
  CHECK(segment_selectors_.find(segmentation_key) != segment_selectors_.end());
  auto& selector = segment_selectors_.at(segmentation_key);
  return selector->GetCachedSegmentResult();
}

void SegmentationPlatformServiceImpl::CollectTrainingData(
    SegmentId segment_id,
    TrainingRequestId request_id,
    const TrainingLabels& param,
    SuccessCallback callback) {
  execution_service_.training_data_collector()->CollectTrainingData(
      segment_id, request_id, ukm::kInvalidSourceId, param,
      std::move(callback));
}

void SegmentationPlatformServiceImpl::CollectTrainingData(
    SegmentId segment_id,
    TrainingRequestId request_id,
    ukm::SourceId ukm_source_id,
    const TrainingLabels& param,
    SuccessCallback callback) {
  execution_service_.training_data_collector()->CollectTrainingData(
      segment_id, request_id, ukm_source_id, param, std::move(callback));
}

void SegmentationPlatformServiceImpl::EnableMetrics(
    bool signal_collection_allowed) {
  signal_handler_.EnableMetrics(signal_collection_allowed);
}

ServiceProxy* SegmentationPlatformServiceImpl::GetServiceProxy() {
  return proxy_.get();
}

DatabaseClient* SegmentationPlatformServiceImpl::GetDatabaseClient() {
  if (base::FeatureList::IsEnabled(features::kSegmentationPlatformUkmEngine)) {
    return database_client_.get();
  } else {
    // The database is not created when the feature is disabled.
    return nullptr;
  }
}

bool SegmentationPlatformServiceImpl::IsPlatformInitialized() {
  return storage_init_status_.has_value() && storage_init_status_.value();
}

void SegmentationPlatformServiceImpl::OnDatabaseInitialized(bool success) {
  storage_init_status_ = success;
  OnServiceStatusChanged();

  const auto* config_holder = storage_service_->config_holder();

  if (!success) {
    for (const auto& config : config_holder->configs()) {
      stats::RecordSegmentSelectionFailure(
          *config, stats::SegmentationSelectionFailureReason::kDBInitFailure);
    }
    return;
  }

  segment_score_provider_->Initialize(base::DoNothing());

  signal_handler_.OnSignalListUpdated();

  std::vector<
      raw_ptr<ModelExecutionSchedulerImpl::Observer, VectorExperimental>>
      observers;
  for (auto& key_and_selector : segment_selectors_)
    observers.push_back(key_and_selector.second.get());
  observers.push_back(proxy_.get());
  execution_service_.Initialize(
      storage_service_.get(), &signal_handler_, clock_, task_runner_,
      config_holder->legacy_output_segment_ids(), model_provider_factory_.get(),
      std::move(observers), platform_options_,
      std::move(input_delegate_holder_), profile_prefs_,
      storage_service_->cached_result_provider());

  proxy_->SetExecutionService(&execution_service_);
  database_client_ = std::make_unique<DatabaseClientImpl>(
      &execution_service_, storage_service_->ukm_data_manager());

  for (auto& selector : segment_selectors_) {
    selector.second->OnPlatformInitialized(&execution_service_);
  }

  request_dispatcher_->OnPlatformInitialized(success, &execution_service_,
                                             CreateSegmentResultProviders());

  // Run any method calls that were received during initialization.
  while (!pending_actions_.empty()) {
    auto callback = std::move(pending_actions_.front());
    pending_actions_.pop_front();
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, std::move(callback));
  }

  result_refresh_manager_->Initialize(CreateSegmentResultProviders(),
                                      &execution_service_);

  // Run any daily maintenance tasks.
  RunDailyTasks(/*is_startup=*/true);

  init_time_ = clock_->Now();
  base::UmaHistogramMediumTimes(
      "SegmentationPlatform.Init.CreationToInitializationLatency",
      init_time_ - creation_time_);
}

void SegmentationPlatformServiceImpl::OnSegmentationModelUpdated(
    proto::SegmentInfo segment_info,
    std::optional<int64_t> old_model_version) {
  CHECK(IsPlatformInitialized());
  if (!segment_info.has_model_metadata()) {
    signal_handler_.OnSignalListUpdated();
    storage_service_->ExecuteDatabaseMaintenanceTasks(false);
    return;
  }

  DCHECK(metadata_utils::ValidateSegmentInfoMetadataAndFeatures(segment_info) ==
         metadata_utils::ValidationResult::kValidationSuccess);

  // This method is called when model is available for execution at startup. The
  // segment info would not have changed for most cases.
  const bool version_updated =
      !old_model_version || *old_model_version != segment_info.model_version();
  if (version_updated) {
    signal_handler_.OnSignalListUpdated();
  }

  if (!metadata_utils::SegmentUsesLegacyOutput(segment_info.segment_id())) {
    result_refresh_manager_->OnModelUpdated(&segment_info);
    request_dispatcher_->OnModelUpdated(segment_info.segment_id());
  } else {
    execution_service_.OnNewModelInfoReadyLegacy(segment_info);
  }

  // Update the service status for proxy.
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(&SegmentationPlatformServiceImpl::OnServiceStatusChanged,
                     weak_ptr_factory_.GetWeakPtr()));
}

void SegmentationPlatformServiceImpl::OnModelRefreshNeeded() {
  // TODO(b/303707413) : Migrate this to use RRM instead.
  execution_service_.RefreshModelResults();
}

void SegmentationPlatformServiceImpl::OnServiceStatusChanged() {
  proxy_->OnServiceStatusChanged(storage_init_status_.has_value(),
                                 storage_service_->GetServiceStatus());
}

void SegmentationPlatformServiceImpl::RunDailyTasks(bool is_startup) {
  result_refresh_manager_->RefreshModelResults(is_startup);
  execution_service_.RunDailyTasks(is_startup);
  storage_service_->ExecuteDatabaseMaintenanceTasks(is_startup);

  base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&SegmentationPlatformServiceImpl::RunDailyTasks,
                     weak_ptr_factory_.GetWeakPtr(), /*is_startup=*/false),
      base::Days(1));
}

std::map<std::string, std::unique_ptr<SegmentResultProvider>>
SegmentationPlatformServiceImpl::CreateSegmentResultProviders() {
  std::map<std::string, std::unique_ptr<SegmentResultProvider>>
      result_providers;
  for (const auto& config : storage_service_->config_holder()->configs()) {
    result_providers[config->segmentation_key] = CreateSegmentResultProvider();
  }
  return result_providers;
}

std::unique_ptr<SegmentResultProvider>
SegmentationPlatformServiceImpl::CreateSegmentResultProvider() {
  return SegmentResultProvider::Create(
      storage_service_->segment_info_database(),
      storage_service_->signal_storage_config(), &execution_service_, clock_,
      platform_options_.force_refresh_results);
}

// static
void SegmentationPlatformService::RegisterProfilePrefs(
    PrefRegistrySimple* registry) {
  registry->RegisterDictionaryPref(kSegmentationResultPref);
  registry->RegisterStringPref(kSegmentationClientResultPrefs, std::string());
  registry->RegisterTimePref(kSegmentationLastDBCompactionTimePref,
                             base::Time());
  registry->RegisterTimePref(kSegmentationUmaSqlDatabaseStartTimePref,
                             base::Time());
}

// static
void SegmentationPlatformService::RegisterLocalStatePrefs(
    PrefRegistrySimple* registry) {
  registry->RegisterTimePref(kSegmentationUkmMostRecentAllowedTimeKey,
                             base::Time());
  registry->RegisterTimePref(kSegmentationLastCollectionTimePref, base::Time());
}

}  // namespace segmentation_platform
