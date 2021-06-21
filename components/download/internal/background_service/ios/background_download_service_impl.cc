// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/download/internal/background_service/ios/background_download_service_impl.h"

#include <utility>

#include "base/notreached.h"
#include "components/download/internal/background_service/config.h"
#include "components/download/public/background_service/download_params.h"

namespace download {

BackgroundDownloadServiceImpl::BackgroundDownloadServiceImpl()
    : config_(std::make_unique<Configuration>()),
      service_config_(config_.get()) {}

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
  NOTIMPLEMENTED();
  return BackgroundDownloadService::ServiceStatus::UNAVAILABLE;
}

void BackgroundDownloadServiceImpl::StartDownload(
    DownloadParams download_params) {
  NOTIMPLEMENTED();
}

void BackgroundDownloadServiceImpl::PauseDownload(const std::string& guid) {
  NOTIMPLEMENTED();
}
void BackgroundDownloadServiceImpl::ResumeDownload(const std::string& guid) {
  NOTIMPLEMENTED();
}
void BackgroundDownloadServiceImpl::CancelDownload(const std::string& guid) {
  NOTIMPLEMENTED();
}
void BackgroundDownloadServiceImpl::ChangeDownloadCriteria(
    const std::string& guid,
    const SchedulingParams& params) {
  NOTIMPLEMENTED();
}

Logger* BackgroundDownloadServiceImpl::GetLogger() {
  NOTIMPLEMENTED();
  return nullptr;
}

}  // namespace download