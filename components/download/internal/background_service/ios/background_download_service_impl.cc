// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/download/internal/background_service/ios/background_download_service_impl.h"

#include <utility>

#include "base/logging.h"
#include "base/notreached.h"
#include "base/numerics/safe_conversions.h"
#include "base/threading/thread_task_runner_handle.h"
#include "components/download/internal/background_service/client_set.h"
#include "components/download/internal/background_service/config.h"
#include "components/download/internal/background_service/entry.h"
#include "components/download/internal/background_service/file_monitor.h"
#include "components/download/internal/background_service/ios/background_download_task_helper.h"
#include "components/download/internal/background_service/ios/entry_utils.h"
#include "components/download/internal/background_service/stats.h"
#include "components/download/public/background_service/client.h"
#include "components/download/public/background_service/download_metadata.h"
#include "components/download/public/background_service/download_params.h"

namespace download {
namespace {

// Interval to throttle the download update that results in a database update.
const base::TimeDelta kUpdateInterval = base::TimeDelta::FromSeconds(5);

void InvokeStartCallback(const std::string& guid,
                         DownloadClient client,
                         DownloadParams::StartResult result,
                         DownloadParams::StartCallback callback) {
  stats::LogStartDownloadResult(client, result);
  if (callback) {
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), guid, result));
  }
}

}  // namespace

BackgroundDownloadServiceImpl::BackgroundDownloadServiceImpl(
    std::unique_ptr<ClientSet> clients,
    std::unique_ptr<Model> model,
    std::unique_ptr<BackgroundDownloadTaskHelper> download_helper,
    std::unique_ptr<FileMonitor> file_monitor,
    const base::FilePath& download_dir,
    base::Clock* clock)
    : config_(std::make_unique<Configuration>()),
      service_config_(config_.get()),
      clients_(std::move(clients)),
      model_(std::move(model)),
      download_helper_(std::move(download_helper)),
      file_monitor_(std::move(file_monitor)),
      clock_(clock),
      download_dir_(download_dir) {
  model_->Initialize(this);
}

BackgroundDownloadServiceImpl::~BackgroundDownloadServiceImpl() = default;

const ServiceConfig& BackgroundDownloadServiceImpl::GetConfig() {
  NOTREACHED() << " This function is not supported on iOS.";
  return service_config_;
}

void BackgroundDownloadServiceImpl::OnStartScheduledTask(
    DownloadTaskType task_type,
    TaskFinishedCallback callback) {
  NOTREACHED() << " This function is not supported on iOS.";
}

bool BackgroundDownloadServiceImpl::OnStopScheduledTask(
    DownloadTaskType task_type) {
  NOTREACHED() << " This function is not supported on iOS.";
  return true;
}

BackgroundDownloadService::ServiceStatus
BackgroundDownloadServiceImpl::GetStatus() {
  if (!init_success_.has_value())
    return BackgroundDownloadService::ServiceStatus::STARTING_UP;
  return init_success_.value()
             ? BackgroundDownloadService::ServiceStatus::READY
             : BackgroundDownloadService::ServiceStatus::UNAVAILABLE;
}

void BackgroundDownloadServiceImpl::StartDownload(
    DownloadParams download_params) {
  // TODO(xingliu): Refactor non-iOS download service to share cached api
  // functionality.
  if (GetStatus() != BackgroundDownloadService::ServiceStatus::READY) {
    LOG(ERROR) << "Background download service is not intialized successfully.";
    InvokeStartCallback(download_params.guid, download_params.client,
                        DownloadParams::StartResult::INTERNAL_ERROR,
                        std::move(download_params.callback));
    return;
  }

  if (start_callbacks_.find(download_params.guid) != start_callbacks_.end() ||
      model_->Get(download_params.guid) != nullptr) {
    InvokeStartCallback(download_params.guid, download_params.client,
                        DownloadParams::StartResult::UNEXPECTED_GUID,
                        std::move(download_params.callback));
    return;
  }

  DCHECK(!download_params.guid.empty());
  start_callbacks_.emplace(download_params.guid,
                           std::move(download_params.callback));
  Entry entry(download_params);
  entry.target_file_path = download_dir_.AppendASCII(download_params.guid);
  entry.create_time = clock_->Now();
  entry.state = Entry::State::ACTIVE;
  model_->Add(entry);
}

void BackgroundDownloadServiceImpl::PauseDownload(const std::string& guid) {
  NOTREACHED() << " This function is not supported on iOS.";
}

void BackgroundDownloadServiceImpl::ResumeDownload(const std::string& guid) {
  NOTREACHED() << " This function is not supported on iOS.";
}
void BackgroundDownloadServiceImpl::CancelDownload(const std::string& guid) {
  NOTREACHED() << " This function is not supported on iOS.";
}
void BackgroundDownloadServiceImpl::ChangeDownloadCriteria(
    const std::string& guid,
    const SchedulingParams& params) {
  NOTREACHED() << " This function is not supported on iOS.";
}

Logger* BackgroundDownloadServiceImpl::GetLogger() {
  NOTIMPLEMENTED();
  return nullptr;
}

void BackgroundDownloadServiceImpl::OnModelReady(bool success) {
  if (!success) {
    init_success_ = false;
    stats::LogStartUpResult(false, stats::StartUpResult::FAILURE_REASON_MODEL);
    NotifyServiceUnavailable();
    return;
  }

  // Clean up expired entries or entries without a client, disregard whether
  // they are completed.
  std::set<std::string> entries_to_remove;
  for (Entry* entry : model_->PeekEntries()) {
    download::Client* client = clients_->GetClient(entry->client);
    // TODO(xingliu): Ask client whether we can delete the file?
    if (!client || base::Time::Now() - entry->create_time >
                       config_->file_keep_alive_time) {
      entries_to_remove.insert(entry->guid);
    }
  }

  for (const auto& guid : entries_to_remove)
    model_->Remove(guid);

  file_monitor_->Initialize(
      base::BindOnce(&BackgroundDownloadServiceImpl::OnFileMonitorInitialized,
                     weak_ptr_factory_.GetWeakPtr()));
}

void BackgroundDownloadServiceImpl::OnFileMonitorInitialized(bool success) {
  if (!success) {
    init_success_ = false;
    stats::LogStartUpResult(false,
                            stats::StartUpResult::FAILURE_REASON_FILE_MONITOR);
    NotifyServiceUnavailable();
    return;
  }

  // Clean up the download file directory on a background thread.
  file_monitor_->DeleteUnknownFiles(
      model_->PeekEntries(), {},
      base::BindOnce(&BackgroundDownloadServiceImpl::OnFilesPruned,
                     weak_ptr_factory_.GetWeakPtr()));
}

void BackgroundDownloadServiceImpl::OnFilesPruned() {
  // Initialization is done.
  init_success_ = true;
  stats::LogStartUpResult(false, stats::StartUpResult::SUCCESS);

  // Report download metadata to clients.
  auto metadata_map = util::MapEntriesToMetadataForClients(
      clients_->GetRegisteredClients(), model_->PeekEntries());
  for (DownloadClient client_id : clients_->GetRegisteredClients()) {
    clients_->GetClient(client_id)->OnServiceInitialized(
        /*state_lost=*/false, metadata_map[client_id]);
  }
}

void BackgroundDownloadServiceImpl::NotifyServiceUnavailable() {
  for (DownloadClient client_id : clients_->GetRegisteredClients())
    clients_->GetClient(client_id)->OnServiceUnavailable();
}

void BackgroundDownloadServiceImpl::OnModelHardRecoverComplete(bool success) {}

void BackgroundDownloadServiceImpl::OnItemAdded(bool success,
                                                DownloadClient client,
                                                const std::string& guid) {
  DownloadParams::StartCallback callback = std::move(start_callbacks_[guid]);
  start_callbacks_.erase(guid);
  if (!success) {
    InvokeStartCallback(guid, client,
                        DownloadParams::StartResult::INTERNAL_ERROR,
                        std::move(callback));
    return;
  }

  Entry* entry = model_->Get(guid);
  DCHECK(entry);

  InvokeStartCallback(guid, client, DownloadParams::StartResult::ACCEPTED,
                      std::move(callback));
  download_helper_->StartDownload(
      entry->guid, entry->target_file_path, entry->request_params,
      entry->scheduling_params,
      base::BindOnce(&BackgroundDownloadServiceImpl::OnDownloadFinished,
                     weak_ptr_factory_.GetWeakPtr(), entry->client,
                     entry->guid),
      base::BindRepeating(&BackgroundDownloadServiceImpl::OnDownloadUpdated,
                          weak_ptr_factory_.GetWeakPtr(), entry->client,
                          entry->guid));
}

void BackgroundDownloadServiceImpl::OnItemUpdated(bool success,
                                                  DownloadClient client,
                                                  const std::string& guid) {}

void BackgroundDownloadServiceImpl::OnItemRemoved(bool success,
                                                  DownloadClient client,
                                                  const std::string& guid) {}

void BackgroundDownloadServiceImpl::OnDownloadFinished(
    DownloadClient download_client,
    const std::string& guid,
    bool success,
    const base::FilePath& file_path,
    int64_t file_size) {
  download::Client* client = clients_->GetClient(download_client);
  if (!client)
    return;

  // TODO(xingliu): Plumb more details from platform api for failure reasons and
  // bytes downloaded.
  if (!success) {
    stats::LogDownloadCompletion(CompletionType::FAIL, file_size);
    model_->Remove(guid);
    client->OnDownloadFailed(guid, CompletionInfo(),
                             download::Client::FailureReason::UNKNOWN);

    return;
  }

  Entry* entry = model_->Get(guid);
  if (!entry)
    return;

  entry->bytes_downloaded = base::saturated_cast<uint64_t>(file_size);
  entry->completion_time = clock_->Now();
  entry->state = Entry::State::COMPLETE;
  model_->Update(*entry);

  stats::LogDownloadCompletion(CompletionType::SUCCEED, file_size);

  CompletionInfo completion_info;
  completion_info.path = file_path;
  client->OnDownloadSucceeded(guid, completion_info);
}

void BackgroundDownloadServiceImpl::OnDownloadUpdated(
    DownloadClient download_client,
    const std::string& guid,
    int64_t bytes_downloaded) {
  uint64_t bytes_count = base::saturated_cast<uint64_t>(bytes_downloaded);
  MaybeUpdateProgress(guid, bytes_count);

  download::Client* client = clients_->GetClient(download_client);
  if (!client)
    return;

  client->OnDownloadUpdated(guid, /*bytes_uploaded*/ 0u, bytes_count);
}

void BackgroundDownloadServiceImpl::MaybeUpdateProgress(
    const std::string& guid,
    uint64_t bytes_downloaded) {
  // Throttle the model update frequency.
  if (clock_->Now() - update_time_ < kUpdateInterval)
    return;

  update_time_ = clock_->Now();
  Entry* entry = model_->Get(guid);
  DCHECK_GE(bytes_downloaded, 0u);
  entry->bytes_downloaded = bytes_downloaded;
  model_->Update(*entry);
}

}  // namespace download
