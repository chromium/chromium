// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/download/internal/background_service/ios/background_download_service_impl.h"

#include <utility>

#include "base/logging.h"
#include "base/notreached.h"
#include "base/threading/thread_task_runner_handle.h"
#include "components/download/internal/background_service/client_set.h"
#include "components/download/internal/background_service/config.h"
#include "components/download/internal/background_service/entry.h"
#include "components/download/internal/background_service/ios/background_download_task_helper.h"
#include "components/download/internal/background_service/ios/entry_utils.h"
#include "components/download/public/background_service/client.h"
#include "components/download/public/background_service/download_metadata.h"
#include "components/download/public/background_service/download_params.h"

namespace download {
namespace {

void InvokeStartCallback(const std::string& guid,
                         DownloadParams::StartResult result,
                         DownloadParams::StartCallback callback) {
  if (callback) {
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), guid, result));
  }
}

}  // namespace

BackgroundDownloadServiceImpl::BackgroundDownloadServiceImpl(
    std::unique_ptr<ClientSet> clients,
    std::unique_ptr<Model> model,
    std::unique_ptr<BackgroundDownloadTaskHelper> download_helper)
    : config_(std::make_unique<Configuration>()),
      service_config_(config_.get()),
      clients_(std::move(clients)),
      model_(std::move(model)),
      download_helper_(std::move(download_helper)) {
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
    InvokeStartCallback(download_params.guid,
                        DownloadParams::StartResult::INTERNAL_ERROR,
                        std::move(download_params.callback));
    return;
  }

  if (start_callbacks_.find(download_params.guid) != start_callbacks_.end() ||
      model_->Get(download_params.guid) != nullptr) {
    InvokeStartCallback(download_params.guid,
                        DownloadParams::StartResult::UNEXPECTED_GUID,
                        std::move(download_params.callback));
    return;
  }

  start_callbacks_.emplace(download_params.guid,
                           std::move(download_params.callback));
  model_->Add(Entry(download_params));
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
  init_success_ = success;
  if (!success) {
    // Report service failure to clients.
    for (DownloadClient client_id : clients_->GetRegisteredClients())
      clients_->GetClient(client_id)->OnServiceUnavailable();
    return;
  }

  // Report download metadata to clients.
  auto metadata_map = util::MapEntriesToMetadataForClients(
      clients_->GetRegisteredClients(), model_->PeekEntries());
  for (DownloadClient client_id : clients_->GetRegisteredClients())
    clients_->GetClient(client_id)->OnServiceInitialized(
        /*state_lost=*/false, metadata_map[client_id]);
}

void BackgroundDownloadServiceImpl::OnModelHardRecoverComplete(bool success) {}

void BackgroundDownloadServiceImpl::OnItemAdded(bool success,
                                                DownloadClient client,
                                                const std::string& guid) {
  DownloadParams::StartCallback callback = std::move(start_callbacks_[guid]);
  start_callbacks_.erase(guid);
  if (!success) {
    InvokeStartCallback(guid, DownloadParams::StartResult::INTERNAL_ERROR,
                        std::move(callback));
    return;
  }

  Entry* entry = model_->Get(guid);
  DCHECK(entry);

  InvokeStartCallback(guid, DownloadParams::StartResult::ACCEPTED,
                      std::move(callback));
  download_helper_->StartDownload(
      entry->guid, entry->request_params, entry->scheduling_params,
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
    const base::FilePath& file_path) {
  download::Client* client = clients_->GetClient(download_client);
  if (!client)
    return;

  // TODO(xingliu): Plumb more details from platform api for failure reasons and
  // bytes downloaded.
  if (!success) {
    model_->Remove(guid);
    client->OnDownloadFailed(guid, CompletionInfo(),
                             download::Client::FailureReason::UNKNOWN);

    return;
  }
  CompletionInfo completion_info;
  completion_info.path = file_path;
  client->OnDownloadSucceeded(guid, completion_info);
}

void BackgroundDownloadServiceImpl::OnDownloadUpdated(
    DownloadClient download_client,
    const std::string& guid,
    int64_t bytes_downloaded) {
  download::Client* client = clients_->GetClient(download_client);
  if (!client)
    return;

  client->OnDownloadUpdated(guid, /*bytes_uploaded*/ 0u,
                            static_cast<uint64_t>(bytes_downloaded));
}

}  // namespace download
