// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/download/public/common/auto_resumption_handler.h"

#include <vector>

#include "base/bind.h"
#include "base/metrics/field_trial_params.h"
#include "base/strings/string_number_conversions.h"
#include "base/threading/thread_task_runner_handle.h"
#include "components/download/public/task/task_scheduler.h"
#include "url/gurl.h"

namespace {

static download::AutoResumptionHandler* g_auto_resumption_handler = nullptr;

// The delay to wait for after a chrome restart before resuming all pending
// downloads so that tab loading doesn't get impacted.
const base::TimeDelta kAutoResumeStartupDelay =
    base::TimeDelta::FromSeconds(10);

// The interval at which various download updates are grouped together for
// computing the params for the task scheduler.
const base::TimeDelta kBatchDownloadUpdatesInterval =
    base::TimeDelta::FromSeconds(1);

// The delay to wait for before immediately retrying a download after it got
// interrupted due to network reasons.
const base::TimeDelta kDownloadImmediateRetryDelay =
    base::TimeDelta::FromSeconds(1);

// The task type to use for scheduling a task.
const download::DownloadTaskType kResumptionTaskType =
    download::DownloadTaskType::DOWNLOAD_AUTO_RESUMPTION_TASK;

// The window start time after which the system should fire the task.
const int64_t kWindowStartTimeSeconds = 0;

// The window end time before which the system should fire the task.
const int64_t kWindowEndTimeSeconds = 24 * 60 * 60;

bool IsMetered(network::mojom::ConnectionType type) {
  switch (type) {
    case network::mojom::ConnectionType::CONNECTION_2G:
    case network::mojom::ConnectionType::CONNECTION_3G:
    case network::mojom::ConnectionType::CONNECTION_4G:
      return true;
    case network::mojom::ConnectionType::CONNECTION_ETHERNET:
    case network::mojom::ConnectionType::CONNECTION_WIFI:
    case network::mojom::ConnectionType::CONNECTION_UNKNOWN:
    case network::mojom::ConnectionType::CONNECTION_NONE:
    case network::mojom::ConnectionType::CONNECTION_BLUETOOTH:
      return false;
  }
  NOTREACHED();
  return false;
}

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

namespace download {

AutoResumptionHandler::Config::Config()
    : auto_resumption_size_limit(0),
      is_auto_resumption_enabled_in_native(false) {}

// static
void AutoResumptionHandler::Create(
    std::unique_ptr<download::NetworkStatusListener> network_listener,
    std::unique_ptr<download::TaskManager> task_manager,
    std::unique_ptr<Config> config) {
  DCHECK(!g_auto_resumption_handler);
  g_auto_resumption_handler = new AutoResumptionHandler(
      std::move(network_listener), std::move(task_manager), std::move(config));
}

// static
AutoResumptionHandler* AutoResumptionHandler::Get() {
  return g_auto_resumption_handler;
}

AutoResumptionHandler::AutoResumptionHandler(
    std::unique_ptr<download::NetworkStatusListener> network_listener,
    std::unique_ptr<download::TaskManager> task_manager,
    std::unique_ptr<Config> config)
    : network_listener_(std::move(network_listener)),
      task_manager_(std::move(task_manager)),
      config_(std::move(config)) {
  network_listener_->Start(this);
}

AutoResumptionHandler::~AutoResumptionHandler() {
  network_listener_->Stop();
}

void AutoResumptionHandler::SetResumableDownloads(
    const std::vector<download::DownloadItem*>& downloads) {
  resumable_downloads_.clear();
  for (auto* download : downloads) {
    if (!IsAutoResumableDownload(download))
      continue;
    resumable_downloads_.insert(std::make_pair(download->GetGuid(), download));
    download->RemoveObserver(this);
    download->AddObserver(this);
  }

  base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&AutoResumptionHandler::ResumePendingDownloads,
                     weak_factory_.GetWeakPtr()),
      kAutoResumeStartupDelay);
}

bool AutoResumptionHandler::IsActiveNetworkMetered() const {
  return IsMetered(network_listener_->GetConnectionType());
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
      IsAutoResumableDownload(item) && SatisfiesNetworkRequirements(item)) {
    downloads_to_retry_.insert(item);
    base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
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
  RecomputeTaskParams();
}

void AutoResumptionHandler::OnDownloadDestroyed(download::DownloadItem* item) {
  resumable_downloads_.erase(item->GetGuid());
  downloads_to_retry_.erase(item);
}

void AutoResumptionHandler::ResumeDownloadImmediately() {
  for (auto* download : std::move(downloads_to_retry_)) {
    if (SatisfiesNetworkRequirements(download))
      download->Resume(false);
    else
      RecomputeTaskParams();
  }
  downloads_to_retry_.clear();
}

void AutoResumptionHandler::OnStartScheduledTask(
    download::TaskFinishedCallback callback) {
  task_manager_->OnStartScheduledTask(kResumptionTaskType, std::move(callback));
  ResumePendingDownloads();
}

bool AutoResumptionHandler::OnStopScheduledTask() {
  task_manager_->OnStopScheduledTask(kResumptionTaskType);
  RescheduleTaskIfNecessary();
  return false;
}

void AutoResumptionHandler::RecomputeTaskParams() {
  if (recompute_task_params_scheduled_)
    return;

  recompute_task_params_scheduled_ = true;
  base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&AutoResumptionHandler::RescheduleTaskIfNecessary,
                     weak_factory_.GetWeakPtr()),
      kBatchDownloadUpdatesInterval);
}

void AutoResumptionHandler::RescheduleTaskIfNecessary() {
  if (!config_->is_auto_resumption_enabled_in_native)
    return;

  recompute_task_params_scheduled_ = false;

  bool has_resumable_downloads = false;
  bool has_actionable_downloads = false;
  bool can_download_on_metered = false;
  for (auto iter = resumable_downloads_.begin();
       iter != resumable_downloads_.end(); ++iter) {
    download::DownloadItem* download = iter->second;
    if (!IsAutoResumableDownload(download))
      continue;

    has_resumable_downloads = true;
    has_actionable_downloads |= SatisfiesNetworkRequirements(download);
    can_download_on_metered |= download->AllowMetered();
    if (can_download_on_metered)
      break;
  }

  if (!has_actionable_downloads)
    task_manager_->NotifyTaskFinished(kResumptionTaskType, false);

  if (!has_resumable_downloads) {
    task_manager_->UnscheduleTask(kResumptionTaskType);
    return;
  }

  download::TaskManager::TaskParams task_params;
  task_params.require_unmetered_network = !can_download_on_metered;
  task_params.window_start_time_seconds = kWindowStartTimeSeconds;
  task_params.window_end_time_seconds = kWindowEndTimeSeconds;
  task_manager_->ScheduleTask(kResumptionTaskType, task_params);
}

void AutoResumptionHandler::ResumePendingDownloads() {
  if (!config_->is_auto_resumption_enabled_in_native)
    return;

  for (auto iter = resumable_downloads_.begin();
       iter != resumable_downloads_.end(); ++iter) {
    download::DownloadItem* download = iter->second;
    if (!IsAutoResumableDownload(download))
      continue;

    if (SatisfiesNetworkRequirements(download))
      download->Resume(false);
  }
}

bool AutoResumptionHandler::SatisfiesNetworkRequirements(
    download::DownloadItem* download) {
  if (!IsConnected(network_listener_->GetConnectionType()))
    return false;

  return download->AllowMetered() || !IsActiveNetworkMetered();
}

bool AutoResumptionHandler::IsAutoResumableDownload(
    download::DownloadItem* item) {
  if (!item || item->IsDangerous())
    return false;

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
      NOTREACHED();
  }

  return false;
}

// static
bool AutoResumptionHandler::IsInterruptedDownloadAutoResumable(
    download::DownloadItem* download_item,
    int auto_resumption_size_limit) {
  DCHECK_EQ(download::DownloadItem::INTERRUPTED, download_item->GetState());
  if (download_item->IsDangerous())
    return false;

  if (!download_item->GetURL().SchemeIsHTTPOrHTTPS())
    return false;

  if (download_item->GetBytesWasted() > auto_resumption_size_limit)
    return false;

  int interrupt_reason = download_item->GetLastReason();
  DCHECK_NE(interrupt_reason, download::DOWNLOAD_INTERRUPT_REASON_NONE);
  return interrupt_reason ==
             download::DOWNLOAD_INTERRUPT_REASON_NETWORK_TIMEOUT ||
         interrupt_reason ==
             download::DOWNLOAD_INTERRUPT_REASON_NETWORK_FAILED ||
         interrupt_reason ==
             download::DOWNLOAD_INTERRUPT_REASON_NETWORK_DISCONNECTED ||
         interrupt_reason == download::DOWNLOAD_INTERRUPT_REASON_CRASH;
}

}  // namespace download
