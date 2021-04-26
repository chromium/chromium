// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DOWNLOAD_PUBLIC_BACKGROUND_SERVICE_TEST_TEST_DOWNLOAD_SERVICE_H_
#define COMPONENTS_DOWNLOAD_PUBLIC_BACKGROUND_SERVICE_TEST_TEST_DOWNLOAD_SERVICE_H_

#include <list>
#include <memory>
#include <string>

#include "base/optional.h"
#include "components/download/public/background_service/client.h"
#include "components/download/public/background_service/download_params.h"
#include "components/download/public/background_service/download_service.h"

namespace download {

struct CompletionInfo;

namespace test {

// Implementation of DownloadService used for testing.
class TestDownloadService : public DownloadService {
 public:
  TestDownloadService();
  ~TestDownloadService() override;

  // DownloadService implementation.
  const ServiceConfig& GetConfig() override;
  void OnStartScheduledTask(DownloadTaskType task_type,
                            TaskFinishedCallback callback) override;
  bool OnStopScheduledTask(DownloadTaskType task_type) override;
  DownloadService::ServiceStatus GetStatus() override;
  void StartDownload(const DownloadParams& download_params) override;
  void PauseDownload(const std::string& guid) override;
  void ResumeDownload(const std::string& guid) override;
  void CancelDownload(const std::string& guid) override;
  void ChangeDownloadCriteria(const std::string& guid,
                              const SchedulingParams& params) override;
  Logger* GetLogger() override;

  base::Optional<DownloadParams> GetDownload(const std::string& guid) const;

  // Set failed_download_id and fail_at_start.
  void SetFailedDownload(const std::string& failed_download_id,
                         bool fail_at_start);

  void SetIsReady(bool is_ready);

  void SetHash256(const std::string& hash256);

  void set_client(Client* client) { client_ = client; }

 private:
  // Begin the download, raising success/failure events.
  void ProcessDownload();

  // Notify the observer a download has succeeded.
  void OnDownloadSucceeded(const std::string& guid,
                           const CompletionInfo& completion_info);

  // Notify the observer a download has failed.
  void OnDownloadFailed(const std::string& guid,
                        const CompletionInfo& completion_info,
                        Client::FailureReason failure_reason);

  std::unique_ptr<ServiceConfig> service_config_;
  std::unique_ptr<Logger> logger_;

  bool is_ready_;
  std::string hash256_;
  std::string failed_download_id_;
  bool fail_at_start_;
  uint64_t file_size_;

  Client* client_;

  std::list<DownloadParams> downloads_;

  DISALLOW_COPY_AND_ASSIGN(TestDownloadService);
};

}  // namespace test
}  // namespace download

#endif  // COMPONENTS_DOWNLOAD_PUBLIC_BACKGROUND_SERVICE_TEST_TEST_DOWNLOAD_SERVICE_H_
