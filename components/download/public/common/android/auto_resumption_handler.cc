// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/download/public/common/android/auto_resumption_handler.h"

#include <memory>
#include <utility>
#include <vector>

#include "base/feature_list.h"
#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/metrics/field_trial_params.h"
#include "base/strings/string_number_conversions.h"
#include "base/task/single_thread_task_runner.h"
#include "base/time/clock.h"
#include "base/time/time.h"
#include "components/download/public/common/download_features.h"
#include "components/download/public/common/download_item.h"
#include "components/download/public/common/download_utils.h"
#include "components/download/public/task/task_scheduler.h"
#include "services/network/public/cpp/network_connection_tracker.h"
#include "url/gurl.h"

namespace download {
namespace {

static download::AutoResumptionHandler* g_auto_resumption_handler = nullptr;

// The delay to wait for after a chrome restart before resuming all pending
// downloads so that tab loading doesn't get impacted.
const base::TimeDelta kAutoResumeStartupDelay = base::Seconds(10);

// The interval at which various download updates are grouped together for
// computing the params for the task scheduler.
const base::TimeDelta kBatchDownloadUpdatesInterval = base::Seconds(1);

// The delay to wait for before immediately retrying a download after it got
// interrupted due to network reasons.
const base::TimeDelta kDownloadImmediateRetryDelay = base::Seconds(1);

// Any downloads started before this interval will be ignored. User scheduled
// download will not be affected.
const base::TimeDelta kAutoResumptionExpireInterval = base::Days(7);

// The task type to use for scheduling a task.
const download::DownloadTaskType kResumptionTaskType =
    download::DownloadTaskType::DOWNLOAD_AUTO_RESUMPTION_TASK;

const download::DownloadTaskType kUnmeteredDownloadsTaskType =
    download::DownloadTaskType::DOWNLOAD_AUTO_RESUMPTION_UNMETERED_TASK;

const download::DownloadTaskType kAnyNetworkDownloadsTaskType =
    download::DownloadTaskType::DOWNLOAD_AUTO_RESUMPTION_ANY_NETWORK_TASK;

// The window start time after which the system should fire the task.
const int64_t kWindowStartTimeSeconds = 0;

// The window end time before which the system should fire the task.
const int64_t kWindowEndTimeSeconds = 24 * 60 * 60;

bool IsConnected(network::mojom::ConnectionType type) {
  switch (type) {
    case network::mojom::ConnectionType::CONNECTION_UNKNOWN:
    case network::mojom::ConnectionType::CONNECTION_NONE:
    case network::mojom::ConnectionType::CONNECTION_BLUETOOTH:
      return false;
    default:
      return true;
  }
}

}  // namespace

AutoResumptionHandler::Config::Config()
    : auto_resumption_size_limit(0),
      is_auto_resumption_enabled_in_native(false) {}

// static
void AutoResumptionHandler::Create(
    std::unique_ptr<download::NetworkStatusListener> network_listener,
    std::unique_ptr<download::TaskManager> task_manager,
    std::unique_ptr<Config> config,
    base::Clock* clock) {
  DCHECK(!g_auto_resumption_handler);
  g_auto_resumption_handler = new AutoResumptionHandler(
      std::move(network_listener), std::move(task_manager), std::move(config),
      clock);
}

// static
AutoResumptionHandler* AutoResumptionHandler::Get() {
  return g_auto_resumption_handler;
}

AutoResumptionHandler::AutoResumptionHandler(
    std::unique_ptr<download::NetworkStatusListener> network_listener,
    std::unique_ptr<download::TaskManager> task_manager,
    std::unique_ptr<Config> config,
    base::Clock* clock)
    : network_listener_(std::move(network_listener)),
      task_manager_(std::move(task_manager)),
      config_(std::move(config)),
      clock_(clock) {
  network_listener_->Start(this);
}

AutoResumptionHandler::~AutoResumptionHandler() {
  network_listener_->Stop();
}

void AutoResumptionHandler::SetResumableDownloads(
    const std::vector<raw_ptr<download::DownloadItem, VectorExperimental>>&
        downloads) {
  resumable_downloads_.clear();
  for (download::DownloadItem* download : downloads) {
    if (!IsAutoResumableDownload(download))
      continue;
    resumable_downloads_.insert(std::make_pair(download->GetGuid(), download));
    download->RemoveObserver(this);
    download->AddObserver(this);
  }

  base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&AutoResumptionHandler::ResumePendingDownloads,
                     weak_factory_.GetWeakPtr()),
      kAutoResumeStartupDelay);
}

bool AutoResumptionHandler::IsActiveNetworkMetered() const {
  return network::NetworkConnectionTracker::IsConnectionCellular(
      network_listener_->GetConnectionType());
}

void AutoResumptionHandler::OnNetworkStatusReady(
    network::mojom::ConnectionType type) {
  // TODO(xingliu): The API to check network type on all platforms is async now,
  // that early call to IsActiveNetworkMetered() which queries network type
  // might just return a unknown network type.
}

void AutoResumptionHandler::OnNetworkChanged(
    network::mojom::ConnectionType type) {
  if (!IsConnected(type))
    return;

  ResumePendingDownloads();
}

void AutoResumptionHandler::OnDownloadStarted(download::DownloadItem* item) {
  item->RemoveObserver(this);
  item->AddObserver(this);

  OnDownloadUpdated(item);
}

void AutoResumptionHandler::OnDownloadUpdated(download::DownloadItem* item) {
  if (IsAutoResumableDownload(item))
    resumable_downloads_[item->GetGuid()] = item;
  else
    resumable_downloads_.erase(item->GetGuid());

  if (item->GetState() == download::DownloadItem::INTERRUPTED &&
      IsAutoResumableDownload(item) && ShouldResumeNow(item)) {
    downloads_to_retry_.insert(item);
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
        FROM_HERE,
        base::BindOnce(&AutoResumptionHandler::ResumeDownloadImmediately,
                       weak_factory_.GetWeakPtr()),
        kDownloadImmediateRetryDelay);
    return;
  }
  RecomputeTaskParams();
}

void AutoResumptionHandler::OnDownloadRemoved(download::DownloadItem* item) {
  resumable_downloads_.erase(item->GetGuid());
  downloads_to_retry_.erase(item);
  RecomputeTaskParams();
}

void AutoResumptionHandler::OnDownloadDestroyed(download::DownloadItem* item) {
  resumable_downloads_.erase(item->GetGuid());
  downloads_to_retry_.erase(item);
}

void AutoResumptionHandler::ResumeDownloadImmediately() {
  if (!config_->is_auto_resumption_enabled_in_native)
    return;

  for (download::DownloadItem* download : std::move(downloads_to_retry_)) {
    if (ShouldResumeNow(download))
      download->Resume(false);
    else
      RecomputeTaskParams();
  }
  downloads_to_retry_.clear();
}

void AutoResumptionHandler::OnStartScheduledTask(
    DownloadTaskType type,
    download::TaskFinishedCallback callback) {
  task_manager_->OnStartScheduledTask(type, std::move(callback));
  ResumePendingDownloads();
}

bool AutoResumptionHandler::OnStopScheduledTask(DownloadTaskType type) {
  task_manager_->OnStopScheduledTask(type);
  RescheduleTaskIfNecessary();
  return false;
}

void AutoResumptionHandler::RecomputeTaskParams() {
  if (recompute_task_params_scheduled_)
    return;

  recompute_task_params_scheduled_ = true;
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&AutoResumptionHandler::RescheduleTaskIfNecessary,
                     weak_factory_.GetWeakPtr()),
      kBatchDownloadUpdatesInterval);
}

// Go through all the downloads.
// 1- If there is no immediately resumable downloads, finish the task
// 2- If there are resumable downloads, schedule a task
// 3- If there are no resumable downloads, unschedule the task.
// At any point either a task is running or is scheduled but not both, which is
// handled by TaskManager.
void AutoResumptionHandler::RescheduleTaskIfNecessary() {
  if (!config_->is_auto_resumption_enabled_in_native)
    return;

  recompute_task_params_scheduled_ = false;

  if (base::FeatureList::IsEnabled(features::kDownloadsMigrateToJobsAPI)) {
    RescheduleTaskIfNecessaryForTaskType(kUnmeteredDownloadsTaskType);
    RescheduleTaskIfNecessaryForTaskType(kAnyNetworkDownloadsTaskType);
    return;
  }

  DownloadTaskType task_type = kResumptionTaskType;

  bool has_resumable_downloads = false;
  bool has_actionable_downloads = false;
  bool can_download_on_metered = false;

  for (auto& pair : resumable_downloads_) {
    download::DownloadItem* download = pair.second;
    if (!IsAutoResumableDownload(download))
      continue;

    has_resumable_downloads = true;
    has_actionable_downloads |= ShouldResumeNow(download);
    can_download_on_metered |= download->AllowMetered();
  }

  if (!has_actionable_downloads) {
    task_manager_->NotifyTaskFinished(task_type, false);
  }

  if (!has_resumable_downloads) {
    task_manager_->UnscheduleTask(task_type);
    return;
  }

  download::TaskManager::TaskParams task_params;
  task_params.require_unmetered_network = !can_download_on_metered;
  task_params.window_start_time_seconds = kWindowStartTimeSeconds;
  task_params.window_end_time_seconds = kWindowEndTimeSeconds;
  task_manager_->ScheduleTask(task_type, task_params);
}

// Go through all the downloads. Filter out only the ones having matching
// network type. Then the logic is same for both tasks.
// 1- If no download can resume right now, finish the task and schedule later.
// 2- If no download is in a resumable state, we dont need a task. Unschedule if
// there is any.
// At any point either a task is running or is scheduled but not both, which
// is handled by TaskManager.
void AutoResumptionHandler::RescheduleTaskIfNecessaryForTaskType(
    DownloadTaskType task_type) {
  bool has_resumable_downloads = false;
  bool has_actionable_downloads = false;

  bool requires_unmetered = task_type == kUnmeteredDownloadsTaskType;
  for (auto& pair : resumable_downloads_) {
    download::DownloadItem* download = pair.second;
    // Filter out downloads that don't match the network type.
    if (download->AllowMetered() == requires_unmetered) {
      continue;
    }

    if (!IsAutoResumableDownload(download)) {
      continue;
    }

    has_resumable_downloads = true;
    has_actionable_downloads |= ShouldResumeNow(download);
  }

  if (!has_actionable_downloads) {
    // We finish the task without specifying system reschedule since we are
    // scheduling another task below.
    task_manager_->NotifyTaskFinished(task_type, /*needs_reschedule=*/false);
  }

  if (!has_resumable_downloads) {
    task_manager_->UnscheduleTask(task_type);
    return;
  }

  download::TaskManager::TaskParams task_params;
  task_params.require_unmetered_network = requires_unmetered;
  task_params.window_start_time_seconds = kWindowStartTimeSeconds;
  task_params.window_end_time_seconds = kWindowEndTimeSeconds;
  task_manager_->ScheduleTask(task_type, task_params);
}

void AutoResumptionHandler::ResumePendingDownloads() {
  if (!config_->is_auto_resumption_enabled_in_native)
    return;

  int resumed = MaybeResumeDownloads(resumable_downloads_);

  // If we resume nothing, finish the current task and reschedule.
  if (!resumed)
    RecomputeTaskParams();
}

int AutoResumptionHandler::MaybeResumeDownloads(
    std::map<std::string, raw_ptr<DownloadItem, CtnExperimental>> downloads) {
  int resumed = 0;
  for (const auto& pair : downloads) {
    DownloadItem* download = pair.second;
    if (!IsAutoResumableDownload(download))
      continue;

    if (ShouldResumeNow(download)) {
      download->Resume(false);
      resumed++;
    }
  }

  return resumed;
}

bool AutoResumptionHandler::ShouldResumeNow(
    download::DownloadItem* download) const {
  if (!IsConnected(network_listener_->GetConnectionType()))
    return false;

  return download->AllowMetered() || !IsActiveNetworkMetered();
}

bool AutoResumptionHandler::IsAutoResumableDownload(
    download::DownloadItem* item) const {
  if (!item || item->IsDangerous())
    return false;

  // Ignore downloads started a while ago.
  if (clock_->Now() - item->GetStartTime() > kAutoResumptionExpireInterval) {
    return false;
  }

  switch (item->GetState()) {
    case download::DownloadItem::IN_PROGRESS:
      return !item->IsPaused();
    case download::DownloadItem::COMPLETE:
    case download::DownloadItem::CANCELLED:
      return false;
    case download::DownloadItem::INTERRUPTED:
      return !item->IsPaused() &&
             IsInterruptedDownloadAutoResumable(
                 item, config_->auto_resumption_size_limit);
    case download::DownloadItem::MAX_DOWNLOAD_STATE:
      NOTREACHED_IN_MIGRATION();
  }

  return false;
}

}  // namespace download
