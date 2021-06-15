// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/download/internal/background_service/ios/download_service_impl.h"

#include <utility>

#include "base/notreached.h"
#include "components/download/internal/background_service/config.h"
#include "components/download/public/background_service/download_params.h"

namespace download {

DownloadServiceImpl::DownloadServiceImpl()
    : config_(std::make_unique<Configuration>()),
      service_config_(config_.get()) {}

DownloadServiceImpl::~DownloadServiceImpl() = default;

const ServiceConfig& DownloadServiceImpl::GetConfig() {
  NOTREACHED() << " This function is not supported on iOS.";
  return service_config_;
}
void DownloadServiceImpl::OnStartScheduledTask(DownloadTaskType task_type,
                                               TaskFinishedCallback callback) {
  NOTREACHED() << " This function is not supported on iOS.";
}

bool DownloadServiceImpl::OnStopScheduledTask(DownloadTaskType task_type) {
  NOTREACHED() << " This function is not supported on iOS.";
  return true;
}

DownloadService::ServiceStatus DownloadServiceImpl::GetStatus() {
  NOTIMPLEMENTED();
  return DownloadService::ServiceStatus::UNAVAILABLE;
}

void DownloadServiceImpl::StartDownload(DownloadParams download_params) {
  NOTIMPLEMENTED();
}

void DownloadServiceImpl::PauseDownload(const std::string& guid) {
  NOTIMPLEMENTED();
}
void DownloadServiceImpl::ResumeDownload(const std::string& guid) {
  NOTIMPLEMENTED();
}
void DownloadServiceImpl::CancelDownload(const std::string& guid) {
  NOTIMPLEMENTED();
}
void DownloadServiceImpl::ChangeDownloadCriteria(
    const std::string& guid,
    const SchedulingParams& params) {
  NOTIMPLEMENTED();
}

Logger* DownloadServiceImpl::GetLogger() {
  NOTIMPLEMENTED();
  return nullptr;
}

}  // namespace download