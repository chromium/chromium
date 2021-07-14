// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DOWNLOAD_INTERNAL_BACKGROUND_SERVICE_IOS_BACKGROUND_DOWNLOAD_SERVICE_IMPL_H_
#define COMPONENTS_DOWNLOAD_INTERNAL_BACKGROUND_SERVICE_IOS_BACKGROUND_DOWNLOAD_SERVICE_IMPL_H_

#include <map>
#include <memory>
#include <string>

#include "base/memory/weak_ptr.h"
#include "components/download/internal/background_service/model_impl.h"
#include "components/download/internal/background_service/service_config_impl.h"
#include "components/download/public/background_service/background_download_service.h"
#include "components/download/public/task/download_task_types.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace download {

class BackgroundDownloadTaskHelper;
class ClientSet;
class Model;
struct Configuration;
struct DownloadParams;
struct SchedulingParams;

class BackgroundDownloadServiceImpl : public BackgroundDownloadService,
                                      public Model::Client {
 public:
  BackgroundDownloadServiceImpl(
      std::unique_ptr<ClientSet> clients,
      std::unique_ptr<Model> model,
      std::unique_ptr<BackgroundDownloadTaskHelper> download_helper);
  ~BackgroundDownloadServiceImpl() override;

 private:
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

  void OnDownloadFinished(DownloadClient download_client,
                          const std::string& guid,
                          bool success,
                          const base::FilePath& file_path);

  void OnDownloadUpdated(DownloadClient download_client,
                         const std::string& guid,
                         int64_t bytes_downloaded);

  std::unique_ptr<Configuration> config_;
  ServiceConfigImpl service_config_;
  std::unique_ptr<ClientSet> clients_;
  std::unique_ptr<Model> model_;
  std::unique_ptr<BackgroundDownloadTaskHelper> download_helper_;
  absl::optional<bool> init_success_;
  std::map<std::string, DownloadParams::StartCallback> start_callbacks_;

  base::WeakPtrFactory<BackgroundDownloadServiceImpl> weak_ptr_factory_{this};
};

}  // namespace download

#endif  // COMPONENTS_DOWNLOAD_INTERNAL_BACKGROUND_SERVICE_IOS_BACKGROUND_DOWNLOAD_SERVICE_IMPL_H_
