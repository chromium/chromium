// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DOWNLOAD_INTERNAL_BACKGROUND_SERVICE_INIT_AWARE_BACKGROUND_DOWNLOAD_SERVICE_H_
#define COMPONENTS_DOWNLOAD_INTERNAL_BACKGROUND_SERVICE_INIT_AWARE_BACKGROUND_DOWNLOAD_SERVICE_H_

#include <map>
#include <memory>
#include <string>

#include "base/containers/circular_deque.h"
#include "base/memory/weak_ptr.h"
#include "components/download/public/background_service/background_download_service.h"

namespace download {

class InitializableBackgroundDownloadService;
class Logger;

struct DownloadParams;
struct SchedulingParams;

// The internal implementation of the BackgroundDownloadService.
class InitAwareBackgroundDownloadService : public BackgroundDownloadService {
 public:
  explicit InitAwareBackgroundDownloadService(
      std::unique_ptr<InitializableBackgroundDownloadService> service);

  InitAwareBackgroundDownloadService(
      const InitAwareBackgroundDownloadService&) = delete;
  InitAwareBackgroundDownloadService& operator=(
      const InitAwareBackgroundDownloadService&) = delete;

  ~InitAwareBackgroundDownloadService() override;

  // BackgroundDownloadService implementation.
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
  Logger* GetLogger() override;

 private:
  void OnServiceInitialized();

  std::unique_ptr<InitializableBackgroundDownloadService> service_;

  base::circular_deque<base::OnceClosure> pending_actions_;
  std::map<DownloadTaskType, base::OnceClosure> pending_tasks_;
  bool startup_completed_;

  base::WeakPtrFactory<InitAwareBackgroundDownloadService> weak_ptr_factory_{
      this};
};

}  // namespace download

#endif  // COMPONENTS_DOWNLOAD_INTERNAL_BACKGROUND_SERVICE_INIT_AWARE_BACKGROUND_DOWNLOAD_SERVICE_H_
