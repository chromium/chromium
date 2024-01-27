// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DOWNLOAD_INTERNAL_BACKGROUND_SERVICE_IOS_BACKGROUND_DOWNLOAD_SERVICE_IMPL_H_
#define COMPONENTS_DOWNLOAD_INTERNAL_BACKGROUND_SERVICE_IOS_BACKGROUND_DOWNLOAD_SERVICE_IMPL_H_

#include <map>
#include <memory>
#include <optional>
#include <set>
#include <string>

#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "base/time/clock.h"
#include "base/time/time.h"
#include "components/download/internal/background_service/initializable_background_download_service.h"
#include "components/download/internal/background_service/log_source.h"
#include "components/download/internal/background_service/model_impl.h"
#include "components/download/internal/background_service/service_config_impl.h"
#include "components/download/internal/background_service/startup_status.h"
#include "components/download/public/task/download_task_types.h"

namespace download {

class BackgroundDownloadTaskHelper;
class ClientSet;
class FileMonitor;
class Logger;
class LogSink;
class Model;
struct Configuration;
struct DownloadParams;
struct SchedulingParams;

class BackgroundDownloadServiceImpl
    : public InitializableBackgroundDownloadService,
      public LogSource,
      public Model::Client {
 public:
  BackgroundDownloadServiceImpl(
      std::unique_ptr<ClientSet> clients,
      std::unique_ptr<Model> model,
      std::unique_ptr<BackgroundDownloadTaskHelper> download_helper,
      std::unique_ptr<FileMonitor> file_monitor,
      const base::FilePath& download_dir,
      std::unique_ptr<Logger> logger,
      LogSink* log_sink,
      base::Clock* clock);
  ~BackgroundDownloadServiceImpl() override;

 private:
  // InitializableBackgroundDownloadService implementation.
  void Initialize(base::OnceClosure callback) override;
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
  void HandleEventsForBackgroundURLSession(
      base::OnceClosure completion_handler) override;

  // Model::Client implementation.
  void OnModelReady(bool success) override;
  void OnModelHardRecoverComplete(bool success) override;
  void OnItemAdded(bool success,
                   DownloadClient client,
                   const std::string& guid) override;
  void OnItemUpdated(bool success,
                     DownloadClient client,
                     const std::string& guid) override;
  void OnItemRemoved(bool success,
                     DownloadClient client,
                     const std::string& guid) override;

  // LogSource implementation.
  Controller::State GetControllerState() override;
  const StartupStatus& GetStartupStatus() override;
  LogSource::EntryDetailsList GetServiceDownloads() override;
  std::optional<LogSource::EntryDetails> GetServiceDownload(
      const std::string& guid) override;

  void PruneDbRecords();
  void OnFileMonitorInitialized(bool success);
  void OnFilesPruned();
  void NotifyServiceUnavailable();
  void InvokeStartCallback(DownloadClient client,
                           const std::string& guid,
                           DownloadParams::StartResult result,
                           DownloadParams::StartCallback callback);
  void OnDownloadFinished(DownloadClient download_client,
                          const std::string& guid,
                          bool success,
                          const base::FilePath& file_path,
                          int64_t file_size);

  void OnDownloadUpdated(DownloadClient download_client,
                         const std::string& guid,
                         int64_t bytes_downloaded);
  void MaybeUpdateProgress(const std::string& guid, uint64_t bytes_downloaded);

  std::unique_ptr<Configuration> config_;
  ServiceConfigImpl service_config_;
  std::unique_ptr<ClientSet> clients_;
  std::unique_ptr<Model> model_;
  std::unique_ptr<BackgroundDownloadTaskHelper> download_helper_;
  std::unique_ptr<FileMonitor> file_monitor_;
  std::unique_ptr<Logger> logger_;
  LogSink* log_sink_;
  StartupStatus startup_status_;
  std::map<std::string, DownloadParams::StartCallback> start_callbacks_;
  base::Time update_time_;
  base::Clock* clock_;
  base::OnceClosure init_callback_;

  // A directory to hold download service files. The files in here will be
  // pruned frequently.
  const base::FilePath download_dir_;
  std::set<std::string> cancelled_downloads_;

  SEQUENCE_CHECKER(sequence_checker_);
  base::WeakPtrFactory<BackgroundDownloadServiceImpl> weak_ptr_factory_{this};
};

}  // namespace download

#endif  // COMPONENTS_DOWNLOAD_INTERNAL_BACKGROUND_SERVICE_IOS_BACKGROUND_DOWNLOAD_SERVICE_IMPL_H_
