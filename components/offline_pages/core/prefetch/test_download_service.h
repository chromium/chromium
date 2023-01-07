// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OFFLINE_PAGES_CORE_PREFETCH_TEST_DOWNLOAD_SERVICE_H_
#define COMPONENTS_OFFLINE_PAGES_CORE_PREFETCH_TEST_DOWNLOAD_SERVICE_H_

#include <string>

#include "base/files/scoped_temp_dir.h"
#include "base/memory/raw_ptr.h"
#include "components/download/public/background_service/background_download_service.h"
#include "components/download/public/background_service/client.h"
#include "components/download/public/background_service/download_params.h"
#include "components/offline_pages/core/prefetch/test_download_client.h"

namespace offline_pages {

// Implementation of BackgroundDownloadService used for testing.
class TestDownloadService : public download::BackgroundDownloadService {
 public:
  TestDownloadService();

  TestDownloadService(const TestDownloadService&) = delete;
  TestDownloadService& operator=(const TestDownloadService&) = delete;

  ~TestDownloadService() override;

  // BackgroundDownloadService implementation.
  const download::ServiceConfig& GetConfig() override;
  void OnStartScheduledTask(download::DownloadTaskType task_type,
                            download::TaskFinishedCallback callback) override;
  bool OnStopScheduledTask(download::DownloadTaskType task_type) override;
  BackgroundDownloadService::ServiceStatus GetStatus() override;
  void StartDownload(download::DownloadParams download_params) override;
  void PauseDownload(const std::string& guid) override;
  void ResumeDownload(const std::string& guid) override;
  void CancelDownload(const std::string& guid) override;
  void ChangeDownloadCriteria(
      const std::string& guid,
      const download::SchedulingParams& params) override;
  download::Logger* GetLogger() override;

  void SetClient(TestDownloadClient* client) { client_ = client; }
  // Sets the content saved for downloads requested through StartDownload.
  void SetTestFileData(const std::string& data);

 private:
  void FinishDownload(const std::string& guid);

  base::ScopedTempDir download_dir_;
  raw_ptr<TestDownloadClient> client_ = nullptr;
  int next_file_id_ = 0;
  std::string test_file_data_;
};

}  // namespace offline_pages

#endif  // COMPONENTS_OFFLINE_PAGES_CORE_PREFETCH_TEST_DOWNLOAD_SERVICE_H_
