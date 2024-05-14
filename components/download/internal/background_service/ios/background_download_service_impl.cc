// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/download/internal/background_service/ios/background_download_service_impl.h"

#include <utility>
#include <vector>

#include "base/logging.h"
#include "base/notreached.h"
#include "base/numerics/safe_conversions.h"
#include "base/task/single_thread_task_runner.h"
#include "components/download/internal/background_service/client_set.h"
#include "components/download/internal/background_service/config.h"
#include "components/download/internal/background_service/entry.h"
#include "components/download/internal/background_service/file_monitor.h"
#include "components/download/internal/background_service/ios/background_download_task_helper.h"
#include "components/download/internal/background_service/ios/entry_utils.h"
#include "components/download/internal/background_service/log_sink.h"
#include "components/download/internal/background_service/stats.h"
#include "components/download/public/background_service/client.h"
#include "components/download/public/background_service/download_metadata.h"
#include "components/download/public/background_service/download_params.h"
#include "components/download/public/background_service/logger.h"

namespace download {
namespace {

// Interval to throttle the download update that results in a database update.
const base::TimeDelta kUpdateInterval = base::Seconds(5);

}  // namespace

BackgroundDownloadServiceImpl::BackgroundDownloadServiceImpl(
    std::unique_ptr<ClientSet> clients,
    std::unique_ptr<Model> model,
    std::unique_ptr<BackgroundDownloadTaskHelper> download_helper,
    std::unique_ptr<FileMonitor> file_monitor,
    const base::FilePath& download_dir,
    std::unique_ptr<Logger> logger,
    LogSink* log_sink,
    base::Clock* clock)
    : config_(std::make_unique<Configuration>()),
      service_config_(config_.get()),
      clients_(std::move(clients)),
      model_(std::move(model)),
      download_helper_(std::move(download_helper)),
      file_monitor_(std::move(file_monitor)),
      logger_(std::move(logger)),
      log_sink_(log_sink),
      clock_(clock),
      download_dir_(download_dir) {
  // iOS doesn't use driver interface, mark it ready.
  startup_status_.driver_ok = true;
}

BackgroundDownloadServiceImpl::~BackgroundDownloadServiceImpl() = default;

void BackgroundDownloadServiceImpl::Initialize(base::OnceClosure callback) {
  init_callback_ = std::move(callback);
  model_->Initialize(this);
}

const ServiceConfig& BackgroundDownloadServiceImpl::GetConfig() {
  NOTREACHED_IN_MIGRATION() << " This function is not supported on iOS.";
  return service_config_;
}

void BackgroundDownloadServiceImpl::OnStartScheduledTask(
    DownloadTaskType task_type,
    TaskFinishedCallback callback) {
  NOTREACHED_IN_MIGRATION() << " This function is not supported on iOS.";
}

bool BackgroundDownloadServiceImpl::OnStopScheduledTask(
    DownloadTaskType task_type) {
  NOTREACHED_IN_MIGRATION() << " This function is not supported on iOS.";
  return true;
}

BackgroundDownloadService::ServiceStatus
BackgroundDownloadServiceImpl::GetStatus() {
  if (startup_status_.Failed())
    return BackgroundDownloadService::ServiceStatus::UNAVAILABLE;
  return startup_status_.Complete()
             ? BackgroundDownloadService::ServiceStatus::READY
             : BackgroundDownloadService::ServiceStatus::STARTING_UP;
}

void BackgroundDownloadServiceImpl::StartDownload(
    DownloadParams download_params) {
  if (GetStatus() != BackgroundDownloadService::ServiceStatus::READY) {
    LOG(ERROR) << "Background download service is not intialized successfully.";
    InvokeStartCallback(download_params.client, download_params.guid,
                        DownloadParams::StartResult::INTERNAL_ERROR,
                        std::move(download_params.callback));
    return;
  }

  if (start_callbacks_.find(download_params.guid) != start_callbacks_.end() ||
      model_->Get(download_params.guid) != nullptr) {
    InvokeStartCallback(download_params.client, download_params.guid,
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
  entry.custom_data = std::move(download_params.custom_data);

  model_->Add(entry);
}

void BackgroundDownloadServiceImpl::PauseDownload(const std::string& guid) {
  NOTREACHED_IN_MIGRATION() << " This function is not supported on iOS.";
}

void BackgroundDownloadServiceImpl::ResumeDownload(const std::string& guid) {
  NOTREACHED_IN_MIGRATION() << " This function is not supported on iOS.";
}
void BackgroundDownloadServiceImpl::CancelDownload(const std::string& guid) {
  cancelled_downloads_.emplace(guid);
}
void BackgroundDownloadServiceImpl::ChangeDownloadCriteria(
    const std::string& guid,
    const SchedulingParams& params) {
  NOTREACHED_IN_MIGRATION() << " This function is not supported on iOS.";
}

Logger* BackgroundDownloadServiceImpl::GetLogger() {
  return logger_.get();
}

void BackgroundDownloadServiceImpl::HandleEventsForBackgroundURLSession(
    base::OnceClosure completion_handler) {
  download_helper_->HandleEventsForBackgroundURLSession(
      std::move(completion_handler));
}

void BackgroundDownloadServiceImpl::OnModelReady(bool success) {
  startup_status_.model_ok = success;

  if (!success) {
    DCHECK(startup_status_.Failed());
    stats::LogStartUpResult(false, stats::StartUpResult::FAILURE_REASON_MODEL);
    NotifyServiceUnavailable();
    return;
  }

  PruneDbRecords();
  file_monitor_->Initialize(
      base::BindOnce(&BackgroundDownloadServiceImpl::OnFileMonitorInitialized,
                     weak_ptr_factory_.GetWeakPtr()));
}

void BackgroundDownloadServiceImpl::PruneDbRecords() {
  // Clean up expired entries or entries without a client, disregard whether
  // they are completed.
  std::set<std::string> entries_to_remove;
  for (Entry* entry : model_->PeekEntries()) {
    download::Client* client = clients_->GetClient(entry->client);
    // TODO(xingliu): Ask client whether we can delete the file?
    if (!client ||
        clock_->Now() - entry->create_time > config_->file_keep_alive_time) {
      entries_to_remove.insert(entry->guid);
    }

    // On iOS, we don't implement any resumption mechanism, so unfinished
    // downloads should be deleted.
    if (entry->state != Entry::State::COMPLETE) {
      entries_to_remove.insert(entry->guid);
    }
  }
  for (const auto& guid : entries_to_remove)
    model_->Remove(guid);
}

void BackgroundDownloadServiceImpl::OnFileMonitorInitialized(bool success) {
  if (!success) {
    startup_status_.file_monitor_ok = false;
    DCHECK(startup_status_.Complete());
    DCHECK(startup_status_.Failed());
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
  startup_status_.file_monitor_ok = true;
  DCHECK(startup_status_.Ok());
  log_sink_->OnServiceStatusChanged();
  stats::LogStartUpResult(false, stats::StartUpResult::SUCCESS);
  if (init_callback_)
    std::move(init_callback_).Run();

  // Report download metadata to clients.
  auto metadata_map = util::MapEntriesToMetadataForClients(
      clients_->GetRegisteredClients(), model_->PeekEntries());
  for (DownloadClient client_id : clients_->GetRegisteredClients()) {
    clients_->GetClient(client_id)->OnServiceInitialized(
        /*state_lost=*/false, metadata_map[client_id]);
  }

  log_sink_->OnServiceDownloadsAvailable();
}

void BackgroundDownloadServiceImpl::NotifyServiceUnavailable() {
  for (DownloadClient client_id : clients_->GetRegisteredClients())
    clients_->GetClient(client_id)->OnServiceUnavailable();

  log_sink_->OnServiceStatusChanged();
}

void BackgroundDownloadServiceImpl::InvokeStartCallback(
    DownloadClient client,
    const std::string& guid,
    DownloadParams::StartResult result,
    DownloadParams::StartCallback callback) {
  log_sink_->OnServiceRequestMade(client, guid, result);
  stats::LogStartDownloadResult(client, result);
  if (callback) {
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), guid, result));
  }
}

void BackgroundDownloadServiceImpl::OnModelHardRecoverComplete(bool success) {}

void BackgroundDownloadServiceImpl::OnItemAdded(bool success,
                                                DownloadClient client,
                                                const std::string& guid) {
  DownloadParams::StartCallback callback = std::move(start_callbacks_[guid]);
  start_callbacks_.erase(guid);
  if (!success) {
    InvokeStartCallback(client, guid,
                        DownloadParams::StartResult::INTERNAL_ERROR,
                        std::move(callback));
    return;
  }

  Entry* entry = model_->Get(guid);
  DCHECK(entry);

  InvokeStartCallback(client, guid, DownloadParams::StartResult::ACCEPTED,
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

Controller::State BackgroundDownloadServiceImpl::GetControllerState() {
  switch (GetStatus()) {
    case ServiceStatus::STARTING_UP:
      return Controller::State::INITIALIZING;
    case ServiceStatus::READY:
      return Controller::State::READY;
    case ServiceStatus::UNAVAILABLE:
      return Controller::State::UNAVAILABLE;
  }
}

const StartupStatus& BackgroundDownloadServiceImpl::GetStartupStatus() {
  return startup_status_;
}

LogSource::EntryDetailsList
BackgroundDownloadServiceImpl::GetServiceDownloads() {
  EntryDetailsList list;
  auto entries = model_->PeekEntries();
  for (download::Entry* entry : entries) {
    list.push_back(std::make_pair(entry, std::nullopt));
  }
  return list;
}

std::optional<LogSource::EntryDetails>
BackgroundDownloadServiceImpl::GetServiceDownload(const std::string& guid) {
  auto* entry = model_->Get(guid);

  return std::optional<LogSource::EntryDetails>(
      std::make_pair(entry, std::nullopt));
}

void BackgroundDownloadServiceImpl::OnDownloadFinished(
    DownloadClient download_client,
    const std::string& guid,
    bool success,
    const base::FilePath& file_path,
    int64_t file_size) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (cancelled_downloads_.find(guid) != cancelled_downloads_.end()) {
    cancelled_downloads_.erase(guid);
    return;
  }

  download::Client* client = clients_->GetClient(download_client);
  if (!client)
    return;

  // TODO(xingliu): Plumb more details from platform api for failure reasons and
  // bytes downloaded.
  Entry* entry = model_->Get(guid);
  if (!success) {
    stats::LogDownloadCompletion(download_client, CompletionType::FAIL,
                                 file_size);
    if (entry) {
      log_sink_->OnServiceDownloadFailed(CompletionType::UNKNOWN, *entry);
      model_->Remove(guid);
    }
    client->OnDownloadFailed(guid, CompletionInfo(),
                             download::Client::FailureReason::UNKNOWN);
    return;
  }

  if (!entry)
    return;

  entry->bytes_downloaded = base::saturated_cast<uint64_t>(file_size);
  entry->completion_time = clock_->Now();
  entry->state = Entry::State::COMPLETE;
  model_->Update(*entry);
  log_sink_->OnServiceDownloadChanged(guid);
  stats::LogDownloadCompletion(download_client, CompletionType::SUCCEED,
                               file_size);

  CompletionInfo completion_info;
  completion_info.path = file_path;
  completion_info.custom_data = entry->custom_data;
  client->OnDownloadSucceeded(guid, completion_info);
}

void BackgroundDownloadServiceImpl::OnDownloadUpdated(
    DownloadClient download_client,
    const std::string& guid,
    int64_t bytes_downloaded) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (cancelled_downloads_.find(guid) != cancelled_downloads_.end()) {
    return;
  }

  uint64_t bytes_count = base::saturated_cast<uint64_t>(bytes_downloaded);
  MaybeUpdateProgress(guid, bytes_count);

  log_sink_->OnServiceDownloadChanged(guid);
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
