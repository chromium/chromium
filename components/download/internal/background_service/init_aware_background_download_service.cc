// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/download/internal/background_service/init_aware_background_download_service.h"

#include <utility>

#include "base/functional/bind.h"
#include "base/strings/string_util.h"
#include "components/download/internal/background_service/initializable_background_download_service.h"
#include "components/download/internal/background_service/stats.h"

namespace download {

InitAwareBackgroundDownloadService::InitAwareBackgroundDownloadService(
    std::unique_ptr<InitializableBackgroundDownloadService> service)
    : service_(std::move(service)), startup_completed_(false) {
  service_->Initialize(
      base::BindOnce(&InitAwareBackgroundDownloadService::OnServiceInitialized,
                     weak_ptr_factory_.GetWeakPtr()));
}

InitAwareBackgroundDownloadService::~InitAwareBackgroundDownloadService() =
    default;

const ServiceConfig& InitAwareBackgroundDownloadService::GetConfig() {
  return service_->GetConfig();
}

void InitAwareBackgroundDownloadService::OnStartScheduledTask(
    DownloadTaskType task_type,
    TaskFinishedCallback callback) {
  if (startup_completed_) {
    service_->OnStartScheduledTask(task_type, std::move(callback));
    return;
  }

  pending_tasks_[task_type] = base::BindOnce(
      &InitializableBackgroundDownloadService::OnStartScheduledTask,
      base::Unretained(service_.get()), task_type, std::move(callback));
}

bool InitAwareBackgroundDownloadService::OnStopScheduledTask(
    DownloadTaskType task_type) {
  if (startup_completed_) {
    return service_->OnStopScheduledTask(task_type);
  }

  auto iter = pending_tasks_.find(task_type);
  if (iter != pending_tasks_.end()) {
    // We still need to run the callback in order to properly cleanup and notify
    // the system by running the respective task finished callbacks.
    std::move(iter->second).Run();
    pending_tasks_.erase(iter);
  }

  return true;
}

BackgroundDownloadService::ServiceStatus
InitAwareBackgroundDownloadService::GetStatus() {
  return service_->GetStatus();
}

void InitAwareBackgroundDownloadService::StartDownload(
    DownloadParams download_params) {
  stats::LogServiceApiAction(download_params.client,
                             stats::ServiceApiAction::START_DOWNLOAD);
  if (startup_completed_) {
    service_->StartDownload(std::move(download_params));
  } else {
    pending_actions_.push_back(base::BindOnce(
        &InitializableBackgroundDownloadService::StartDownload,
        base::Unretained(service_.get()), std::move(download_params)));
  }
}

void InitAwareBackgroundDownloadService::PauseDownload(
    const std::string& guid) {
  if (startup_completed_) {
    service_->PauseDownload(guid);
  } else {
    pending_actions_.push_back(
        base::BindOnce(&InitializableBackgroundDownloadService::PauseDownload,
                       base::Unretained(service_.get()), guid));
  }
}

void InitAwareBackgroundDownloadService::ResumeDownload(
    const std::string& guid) {
  if (startup_completed_) {
    service_->ResumeDownload(guid);
  } else {
    pending_actions_.push_back(
        base::BindOnce(&InitializableBackgroundDownloadService::ResumeDownload,
                       base::Unretained(service_.get()), guid));
  }
}

void InitAwareBackgroundDownloadService::CancelDownload(
    const std::string& guid) {
  if (startup_completed_) {
    service_->CancelDownload(guid);
  } else {
    pending_actions_.push_back(
        base::BindOnce(&InitializableBackgroundDownloadService::CancelDownload,
                       base::Unretained(service_.get()), guid));
  }
}

void InitAwareBackgroundDownloadService::ChangeDownloadCriteria(
    const std::string& guid,
    const SchedulingParams& params) {
  if (startup_completed_) {
    service_->ChangeDownloadCriteria(guid, params);
  } else {
    pending_actions_.push_back(base::BindOnce(
        &InitializableBackgroundDownloadService::ChangeDownloadCriteria,
        base::Unretained(service_.get()), guid, params));
  }
}

Logger* InitAwareBackgroundDownloadService::GetLogger() {
  return service_->GetLogger();
}

void InitAwareBackgroundDownloadService::OnServiceInitialized() {
  while (!pending_actions_.empty()) {
    auto callback = std::move(pending_actions_.front());
    pending_actions_.pop_front();
    std::move(callback).Run();
  }

  while (!pending_tasks_.empty()) {
    auto iter = pending_tasks_.begin();
    std::move(iter->second).Run();
    pending_tasks_.erase(iter);
  }

  startup_completed_ = true;
}

}  // namespace download
