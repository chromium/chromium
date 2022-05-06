// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/segmentation_platform/internal/segmentation_platform_service_impl.h"

#include <string>

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/metrics/histogram_functions.h"
#include "base/system/sys_info.h"
#include "base/task/sequenced_task_runner.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/time/clock.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/segmentation_platform/internal/constants.h"
#include "components/segmentation_platform/internal/database/storage_service.h"
#include "components/segmentation_platform/internal/platform_options.h"
#include "components/segmentation_platform/internal/proto/model_prediction.pb.h"
#include "components/segmentation_platform/internal/scheduler/model_execution_scheduler_impl.h"
#include "components/segmentation_platform/internal/selection/segment_score_provider.h"
#include "components/segmentation_platform/internal/selection/segment_selector_impl.h"
#include "components/segmentation_platform/internal/selection/segmentation_result_prefs.h"
#include "components/segmentation_platform/internal/stats.h"
#include "components/segmentation_platform/public/config.h"
#include "components/segmentation_platform/public/field_trial_register.h"
#include "components/segmentation_platform/public/model_provider.h"

using optimization_guide::proto::OptimizationTarget;

namespace segmentation_platform {

namespace {

base::flat_set<OptimizationTarget> GetAllSegmentIds(
    const std::vector<std::unique_ptr<Config>>& configs) {
  base::flat_set<OptimizationTarget> all_segment_ids;
  for (const auto& config : configs) {
    for (const auto& segment_id : config->segment_ids)
      all_segment_ids.insert(segment_id);
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
      clock_(init_params->clock),
      platform_options_(PlatformOptions::CreateDefault()),
      configs_(std::move(init_params->configs)),
      all_segment_ids_(GetAllSegmentIds(configs_)),
      field_trial_register_(std::move(init_params->field_trial_register)),
      profile_prefs_(init_params->profile_prefs),
      creation_time_(clock_->Now()) {
  base::UmaHistogramMediumTimes(
      "SegmentationPlatform.Init.ProcessCreationToServiceCreationLatency",
      base::SysInfo::Uptime());

  DCHECK(task_runner_);
  DCHECK(clock);
  DCHECK(init_params->profile_prefs && init_params->local_state);

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
        model_provider_factory_.get());
  }

  std::vector<OptimizationTarget> segment_id_vec(all_segment_ids_.begin(),
                                                 all_segment_ids_.end());

  // Construct signal processors.
  signal_handler_.Initialize(
      storage_service_.get(), init_params->history_service, segment_id_vec,
      base::BindRepeating(
          &SegmentationPlatformServiceImpl::OnModelRefreshNeeded,
          weak_ptr_factory_.GetWeakPtr()));

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
      storage_service_->signal_storage_config(), &configs_,
      &segment_selectors_);
  segment_score_provider_ =
      SegmentScoreProvider::Create(storage_service_->segment_info_database());

  // Kick off initialization of all databases. Internal operations will be
  // delayed until they are all complete.
  storage_service_->Initialize(
      base::BindOnce(&SegmentationPlatformServiceImpl::OnDatabaseInitialized,
                     weak_ptr_factory_.GetWeakPtr()));
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

SegmentSelectionResult SegmentationPlatformServiceImpl::GetCachedSegmentResult(
    const std::string& segmentation_key) {
  CHECK(segment_selectors_.find(segmentation_key) != segment_selectors_.end());
  auto& selector = segment_selectors_.at(segmentation_key);
  return selector->GetCachedSegmentResult();
}

void SegmentationPlatformServiceImpl::EnableMetrics(
    bool signal_collection_allowed) {
  signal_handler_.EnableMetrics(signal_collection_allowed);
}

ServiceProxy* SegmentationPlatformServiceImpl::GetServiceProxy() {
  return proxy_.get();
}

bool SegmentationPlatformServiceImpl::IsPlatformInitialized() {
  return storage_initialized_;
}

void SegmentationPlatformServiceImpl::OnDatabaseInitialized(bool success) {
  storage_initialized_ = true;
  OnServiceStatusChanged();

  if (!success) {
    for (const auto& config : configs_) {
      stats::RecordSegmentSelectionFailure(
          config->segmentation_key,
          stats::SegmentationSelectionFailureReason::kDBInitFailure);
    }
    return;
  }

  segment_score_provider_->Initialize(base::DoNothing());

  signal_handler_.OnSignalListUpdated();

  std::vector<ModelExecutionSchedulerImpl::Observer*> observers;
  for (auto& key_and_selector : segment_selectors_)
    observers.push_back(key_and_selector.second.get());
  execution_service_.Initialize(
      storage_service_.get(), &signal_handler_, clock_,
      base::BindRepeating(
          &SegmentationPlatformServiceImpl::OnSegmentationModelUpdated,
          weak_ptr_factory_.GetWeakPtr()),
      task_runner_, all_segment_ids_, model_provider_factory_.get(),
      std::move(observers), platform_options_, &configs_, profile_prefs_);

  proxy_->SetExecutionService(&execution_service_);

  for (auto& selector : segment_selectors_) {
    selector.second->OnPlatformInitialized(&execution_service_);
  }

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
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::BindOnce(&SegmentationPlatformServiceImpl::OnServiceStatusChanged,
                     weak_ptr_factory_.GetWeakPtr()));
}

void SegmentationPlatformServiceImpl::OnModelRefreshNeeded() {
  execution_service_.RefreshModelResults();
}

void SegmentationPlatformServiceImpl::OnServiceStatusChanged() {
  proxy_->OnServiceStatusChanged(storage_initialized_,
                                 storage_service_->GetServiceStatus());
}

void SegmentationPlatformServiceImpl::RunDailyTasks(bool is_startup) {
  execution_service_.RunDailyTasks(is_startup);
  storage_service_->ExecuteDatabaseMaintenanceTasks(is_startup);

  base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&SegmentationPlatformServiceImpl::RunDailyTasks,
                     weak_ptr_factory_.GetWeakPtr(), /*is_startup=*/false),
      base::Days(1));
}

// static
void SegmentationPlatformService::RegisterProfilePrefs(
    PrefRegistrySimple* registry) {
  registry->RegisterDictionaryPref(kSegmentationResultPref);
}

// static
void SegmentationPlatformService::RegisterLocalStatePrefs(
    PrefRegistrySimple* registry) {
  registry->RegisterTimePref(kSegmentationUkmMostRecentAllowedTimeKey,
                             base::Time());
  registry->RegisterTimePref(kSegmentationLastCollectionTimePref, base::Time());
}

}  // namespace segmentation_platform
