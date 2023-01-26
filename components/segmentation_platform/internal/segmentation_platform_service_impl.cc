// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/segmentation_platform/internal/segmentation_platform_service_impl.h"

#include <string>

#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/scoped_refptr.h"
#include "base/metrics/histogram_functions.h"
#include "base/system/sys_info.h"
#include "base/task/single_thread_task_runner.h"
#include "base/time/clock.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/segmentation_platform/internal/constants.h"
#include "components/segmentation_platform/internal/database/storage_service.h"
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
#include "components/segmentation_platform/public/field_trial_register.h"
#include "components/segmentation_platform/public/input_context.h"
#include "components/segmentation_platform/public/input_delegate.h"
#include "components/segmentation_platform/public/model_provider.h"

namespace segmentation_platform {

namespace {

using proto::SegmentId;

base::flat_set<SegmentId> GetAllSegmentIds(
    const std::vector<std::unique_ptr<Config>>& configs) {
  base::flat_set<SegmentId> all_segment_ids;
  for (const auto& config : configs) {
    for (const auto& segment_id : config->segments)
      all_segment_ids.insert(segment_id.first);
  }
  return all_segment_ids;
}

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
      configs_(std::move(init_params->configs)),
      all_segment_ids_(GetAllSegmentIds(configs_)),
      field_trial_register_(std::move(init_params->field_trial_register)),
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
  } else {
    DCHECK(model_provider_factory_ && init_params->db_provider);
    DCHECK(!init_params->storage_dir.empty() && init_params->ukm_data_manager);
    storage_service_ = std::make_unique<StorageService>(
        init_params->storage_dir, init_params->db_provider,
        init_params->task_runner, init_params->clock,
        init_params->ukm_data_manager, all_segment_ids_,
        model_provider_factory_.get(), profile_prefs_);
  }

  // Construct signal processors.
  signal_handler_.Initialize(
      storage_service_.get(), init_params->history_service, all_segment_ids_,
      base::BindRepeating(
          &SegmentationPlatformServiceImpl::OnModelRefreshNeeded,
          weak_ptr_factory_.GetWeakPtr()));

  // TODO(ritikagup@): Move code for recording FieldTrialRegister into separate
  // class when adding support for recording multi class output fields.
  cached_result_provider_ = std::make_unique<CachedResultProvider>(
      init_params->profile_prefs, configs_);

  request_dispatcher_ = std::make_unique<RequestDispatcher>(
      configs_, cached_result_provider_.get());

  for (const auto& config : configs_) {
    segment_selectors_[config->segmentation_key] =
        std::make_unique<SegmentSelectorImpl>(
            storage_service_->segment_info_database(),
            storage_service_->signal_storage_config(),
            init_params->profile_prefs, config.get(),
            field_trial_register_.get(), init_params->clock, platform_options_,
            storage_service_->default_model_manager());
  }

  proxy_ = std::make_unique<ServiceProxyImpl>(
      storage_service_->segment_info_database(),
      storage_service_->default_model_manager(),
      storage_service_->signal_storage_config(), &configs_, platform_options_,
      &segment_selectors_);
  segment_score_provider_ = SegmentScoreProvider::Create(
      storage_service_->segment_info_database(), all_segment_ids_);

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
}

SegmentationPlatformServiceImpl::~SegmentationPlatformServiceImpl() {
  signal_handler_.TearDown();
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

SegmentSelectionResult SegmentationPlatformServiceImpl::GetCachedSegmentResult(
    const std::string& segmentation_key) {
  CHECK(segment_selectors_.find(segmentation_key) != segment_selectors_.end());
  auto& selector = segment_selectors_.at(segmentation_key);
  return selector->GetCachedSegmentResult();
}

void SegmentationPlatformServiceImpl::GetSelectedSegmentOnDemand(
    const std::string& segmentation_key,
    scoped_refptr<InputContext> input_context,
    SegmentSelectionCallback callback) {
  // TODO(shaktisahu): Delete this API after enabling RequestDispatcher.
  if (!storage_init_status_.has_value()) {
    // If the platform isn't fully initialized, cache the input arguments to run
    // later.
    pending_actions_.push_back(base::BindOnce(
        &SegmentationPlatformServiceImpl::GetSelectedSegmentOnDemand,
        weak_ptr_factory_.GetWeakPtr(), segmentation_key,
        std::move(input_context), std::move(callback)));
    return;
  }

  if (!storage_init_status_.value()) {
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce(std::move(callback), SegmentSelectionResult()));
    return;
  }

  CHECK(segment_selectors_.find(segmentation_key) != segment_selectors_.end());
  auto& selector = segment_selectors_.at(segmentation_key);

  // Wrap callback to record metrics.
  auto wrapped_callback = base::BindOnce(
      [](const std::string& segmentation_key, base::Time start_time,
         SegmentSelectionCallback callback,
         const SegmentSelectionResult& result) -> void {
        stats::RecordOnDemandSegmentSelectionDuration(
            segmentation_key, result, base::Time::Now() - start_time);
        std::move(callback).Run(result);
      },
      segmentation_key, base::Time::Now(), std::move(callback));
  selector->GetSelectedSegmentOnDemand(input_context,
                                       std::move(wrapped_callback));
}

void SegmentationPlatformServiceImpl::EnableMetrics(
    bool signal_collection_allowed) {
  signal_handler_.EnableMetrics(signal_collection_allowed);
}

ServiceProxy* SegmentationPlatformServiceImpl::GetServiceProxy() {
  return proxy_.get();
}

bool SegmentationPlatformServiceImpl::IsPlatformInitialized() {
  return storage_init_status_.has_value() && storage_init_status_.value();
}

void SegmentationPlatformServiceImpl::OnDatabaseInitialized(bool success) {
  storage_init_status_ = success;
  OnServiceStatusChanged();

  if (!success) {
    for (const auto& config : configs_) {
      stats::RecordSegmentSelectionFailure(
          *config, stats::SegmentationSelectionFailureReason::kDBInitFailure);
    }
    return;
  }

  segment_score_provider_->Initialize(base::DoNothing());

  signal_handler_.OnSignalListUpdated();

  std::vector<ModelExecutionSchedulerImpl::Observer*> observers;
  for (auto& key_and_selector : segment_selectors_)
    observers.push_back(key_and_selector.second.get());
  observers.push_back(proxy_.get());
  execution_service_.Initialize(
      storage_service_.get(), &signal_handler_, clock_,
      base::BindRepeating(
          &SegmentationPlatformServiceImpl::OnSegmentationModelUpdated,
          weak_ptr_factory_.GetWeakPtr()),
      task_runner_, all_segment_ids_, model_provider_factory_.get(),
      std::move(observers), platform_options_,
      std::move(input_delegate_holder_), &configs_, profile_prefs_);

  proxy_->SetExecutionService(&execution_service_);

  for (auto& selector : segment_selectors_) {
    selector.second->OnPlatformInitialized(&execution_service_);
  }

  std::map<std::string, std::unique_ptr<SegmentResultProvider>>
      result_providers;
  for (const auto& config : configs_) {
    result_providers[config->segmentation_key] = SegmentResultProvider::Create(
        storage_service_->segment_info_database(),
        storage_service_->signal_storage_config(),
        storage_service_->default_model_manager(), &execution_service_, clock_,
        platform_options_.force_refresh_results);
  }

  request_dispatcher_->OnPlatformInitialized(success, &execution_service_,
                                             std::move(result_providers));

  // Run any method calls that were received during initialization.
  while (!pending_actions_.empty()) {
    auto callback = std::move(pending_actions_.front());
    pending_actions_.pop_front();
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, std::move(callback));
  }

  // Run any daily maintenance tasks.
  RunDailyTasks(/*is_startup=*/true);

  init_time_ = clock_->Now();
  base::UmaHistogramMediumTimes(
      "SegmentationPlatform.Init.CreationToInitializationLatency",
      init_time_ - creation_time_);
}

void SegmentationPlatformServiceImpl::OnSegmentationModelUpdated(
    proto::SegmentInfo segment_info) {
  DCHECK(metadata_utils::ValidateSegmentInfoMetadataAndFeatures(segment_info) ==
         metadata_utils::ValidationResult::kValidationSuccess);

  signal_handler_.OnSignalListUpdated();

  execution_service_.OnNewModelInfoReady(segment_info);

  // Update the service status for proxy.
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(&SegmentationPlatformServiceImpl::OnServiceStatusChanged,
                     weak_ptr_factory_.GetWeakPtr()));
}

void SegmentationPlatformServiceImpl::OnModelRefreshNeeded() {
  execution_service_.RefreshModelResults();
}

void SegmentationPlatformServiceImpl::OnServiceStatusChanged() {
  proxy_->OnServiceStatusChanged(storage_init_status_.has_value(),
                                 storage_service_->GetServiceStatus());
}

void SegmentationPlatformServiceImpl::RunDailyTasks(bool is_startup) {
  execution_service_.RunDailyTasks(is_startup);
  storage_service_->ExecuteDatabaseMaintenanceTasks(is_startup);

  base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&SegmentationPlatformServiceImpl::RunDailyTasks,
                     weak_ptr_factory_.GetWeakPtr(), /*is_startup=*/false),
      base::Days(1));
}

// static
void SegmentationPlatformService::RegisterProfilePrefs(
    PrefRegistrySimple* registry) {
  registry->RegisterDictionaryPref(kSegmentationResultPref);
  registry->RegisterStringPref(kSegmentationClientResultPrefs, std::string());
  registry->RegisterTimePref(kSegmentationLastDBCompactionTimePref,
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
