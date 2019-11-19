// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DOWNLOAD_INTERNAL_BACKGROUND_SERVICE_DOWNLOAD_SERVICE_IMPL_H_
#define COMPONENTS_DOWNLOAD_INTERNAL_BACKGROUND_SERVICE_DOWNLOAD_SERVICE_IMPL_H_

#include <map>
#include <memory>
#include <string>

#include "base/containers/circular_deque.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "components/download/internal/background_service/config.h"
#include "components/download/internal/background_service/service_config_impl.h"
#include "components/download/public/background_service/download_service.h"

namespace download {

class Controller;
class Logger;

struct DownloadParams;
struct SchedulingParams;

// The internal implementation of the DownloadService.
class DownloadServiceImpl : public DownloadService {
 public:
  DownloadServiceImpl(std::unique_ptr<Configuration> config,
                      std::unique_ptr<Logger> logger,
                      std::unique_ptr<Controller> controller);
  ~DownloadServiceImpl() override;

  // DownloadService implementation.
  const ServiceConfig& GetConfig() override;
  void OnStartScheduledTask(DownloadTaskType task_type,
                            TaskFinishedCallback callback) override;
  bool OnStopScheduledTask(DownloadTaskType task_type) override;
  ServiceStatus GetStatus() override;
  void StartDownload(const DownloadParams& download_params) override;
  void PauseDownload(const std::string& guid) override;
  void ResumeDownload(const std::string& guid) override;
  void CancelDownload(const std::string& guid) override;
  void ChangeDownloadCriteria(const std::string& guid,
                              const SchedulingParams& params) override;
  Logger* GetLogger() override;

 private:
  void OnControllerInitialized();

  // config_ needs to be destructed after controller_ and service_config_ which
  // hold onto references to it.
  std::unique_ptr<Configuration> config_;

  std::unique_ptr<Logger> logger_;
  std::unique_ptr<Controller> controller_;
  ServiceConfigImpl service_config_;

  base::circular_deque<base::OnceClosure> pending_actions_;
  std::map<DownloadTaskType, base::OnceClosure> pending_tasks_;
  bool startup_completed_;

  base::WeakPtrFactory<DownloadServiceImpl> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(DownloadServiceImpl);
};

}  // namespace download

#endif  // COMPONENTS_DOWNLOAD_INTERNAL_BACKGROUND_SERVICE_DOWNLOAD_SERVICE_IMPL_H_
