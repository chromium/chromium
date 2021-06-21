// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/download/internal/background_service/background_download_service_impl.h"

#include <utility>

#include "base/bind.h"
#include "base/strings/string_util.h"
#include "components/download/internal/background_service/controller.h"
#include "components/download/internal/background_service/logger_impl.h"
#include "components/download/internal/background_service/startup_status.h"
#include "components/download/internal/background_service/stats.h"

namespace download {

BackgroundDownloadServiceImpl::BackgroundDownloadServiceImpl(
    std::unique_ptr<Configuration> config,
    std::unique_ptr<Logger> logger,
    std::unique_ptr<Controller> controller)
    : config_(std::move(config)),
      logger_(std::move(logger)),
      controller_(std::move(controller)),
      service_config_(config_.get()),
      startup_completed_(false) {
  controller_->Initialize(
      base::BindOnce(&BackgroundDownloadServiceImpl::OnControllerInitialized,
                     weak_ptr_factory_.GetWeakPtr()));
}

BackgroundDownloadServiceImpl::~BackgroundDownloadServiceImpl() = default;

const ServiceConfig& BackgroundDownloadServiceImpl::GetConfig() {
  return service_config_;
}

void BackgroundDownloadServiceImpl::OnStartScheduledTask(
    DownloadTaskType task_type,
    TaskFinishedCallback callback) {
  if (startup_completed_) {
    controller_->OnStartScheduledTask(task_type, std::move(callback));
    return;
  }

  pending_tasks_[task_type] = base::BindOnce(
      &Controller::OnStartScheduledTask, base::Unretained(controller_.get()),
      task_type, std::move(callback));
}

bool BackgroundDownloadServiceImpl::OnStopScheduledTask(
    DownloadTaskType task_type) {
  if (startup_completed_) {
    return controller_->OnStopScheduledTask(task_type);
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
BackgroundDownloadServiceImpl::GetStatus() {
  switch (controller_->GetState()) {
    case Controller::State::CREATED:       // Intentional fallthrough.
    case Controller::State::INITIALIZING:  // Intentional fallthrough.
    case Controller::State::RECOVERING:
      return BackgroundDownloadService::ServiceStatus::STARTING_UP;
    case Controller::State::READY:
      return BackgroundDownloadService::ServiceStatus::READY;
    case Controller::State::UNAVAILABLE:  // Intentional fallthrough.
    default:
      return BackgroundDownloadService::ServiceStatus::UNAVAILABLE;
  }
}

void BackgroundDownloadServiceImpl::StartDownload(
    DownloadParams download_params) {
  stats::LogServiceApiAction(download_params.client,
                             stats::ServiceApiAction::START_DOWNLOAD);
  if (startup_completed_) {
    controller_->StartDownload(std::move(download_params));
  } else {
    pending_actions_.push_back(base::BindOnce(
        &Controller::StartDownload, base::Unretained(controller_.get()),
        std::move(download_params)));
  }
}

void BackgroundDownloadServiceImpl::PauseDownload(const std::string& guid) {
  if (startup_completed_) {
    controller_->PauseDownload(guid);
  } else {
    pending_actions_.push_back(base::BindOnce(
        &Controller::PauseDownload, base::Unretained(controller_.get()), guid));
  }
}

void BackgroundDownloadServiceImpl::ResumeDownload(const std::string& guid) {
  if (startup_completed_) {
    controller_->ResumeDownload(guid);
  } else {
    pending_actions_.push_back(
        base::BindOnce(&Controller::ResumeDownload,
                       base::Unretained(controller_.get()), guid));
  }
}

void BackgroundDownloadServiceImpl::CancelDownload(const std::string& guid) {
  if (startup_completed_) {
    controller_->CancelDownload(guid);
  } else {
    pending_actions_.push_back(
        base::BindOnce(&Controller::CancelDownload,
                       base::Unretained(controller_.get()), guid));
  }
}

void BackgroundDownloadServiceImpl::ChangeDownloadCriteria(
    const std::string& guid,
    const SchedulingParams& params) {
  if (startup_completed_) {
    controller_->ChangeDownloadCriteria(guid, params);
  } else {
    pending_actions_.push_back(
        base::BindOnce(&Controller::ChangeDownloadCriteria,
                       base::Unretained(controller_.get()), guid, params));
  }
}

Logger* BackgroundDownloadServiceImpl::GetLogger() {
  return logger_.get();
}

void BackgroundDownloadServiceImpl::OnControllerInitialized() {
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
