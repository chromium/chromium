// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DOWNLOAD_INTERNAL_BACKGROUND_SERVICE_IOS_DOWNLOAD_SERVICE_IMPL_H_
#define COMPONENTS_DOWNLOAD_INTERNAL_BACKGROUND_SERVICE_IOS_DOWNLOAD_SERVICE_IMPL_H_

#include <memory>
#include <string>

#include "components/download/internal/background_service/service_config_impl.h"
#include "components/download/public/background_service/download_service.h"
#include "components/download/public/task/download_task_types.h"

namespace download {

struct Configuration;
struct DownloadParams;
struct SchedulingParams;

class DownloadServiceImpl : public DownloadService {
 public:
  DownloadServiceImpl();
  ~DownloadServiceImpl() override;

 private:
  // DownloadService implementation.
  const ServiceConfig& GetConfig() override;
  void OnStartScheduledTask(DownloadTaskType task_type,
                            TaskFinishedCallback callback) override;
  bool OnStopScheduledTask(DownloadTaskType task_type) override;
  ServiceStatus GetStatus() override;
  void StartDownload(DownloadParams download_params) override;
  void PauseDownload(const std::string& guid) override;
  void ResumeDownload(const std::string& guid) override;
  void CancelDownload(const std::string& guid) override;
  void ChangeDownloadCriteria(const std::string& guid,
                              const SchedulingParams& params) override;

  std::unique_ptr<Configuration> config_;
  ServiceConfigImpl service_config_;
};

}  // namespace download

#endif  // COMPONENTS_DOWNLOAD_INTERNAL_BACKGROUND_SERVICE_IOS_DOWNLOAD_SERVICE_IMPL_H_
