// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OFFLINE_PAGES_CORE_PREFETCH_TEST_DOWNLOAD_SERVICE_H_
#define COMPONENTS_OFFLINE_PAGES_CORE_PREFETCH_TEST_DOWNLOAD_SERVICE_H_

#include <list>
#include <string>

#include "base/files/scoped_temp_dir.h"
#include "base/optional.h"
#include "components/download/public/background_service/client.h"
#include "components/download/public/background_service/download_params.h"
#include "components/download/public/background_service/download_service.h"
#include "components/offline_pages/core/prefetch/test_download_client.h"

namespace offline_pages {

// Implementation of DownloadService used for testing.
class TestDownloadService : public download::DownloadService {
 public:
  TestDownloadService();
  ~TestDownloadService() override;

  // DownloadService implementation.
  const download::ServiceConfig& GetConfig() override;
  void OnStartScheduledTask(download::DownloadTaskType task_type,
                            download::TaskFinishedCallback callback) override;
  bool OnStopScheduledTask(download::DownloadTaskType task_type) override;
  DownloadService::ServiceStatus GetStatus() override;
  void StartDownload(const download::DownloadParams& download_params) override;
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
  TestDownloadClient* client_ = nullptr;
  int next_file_id_ = 0;
  std::string test_file_data_;
  DISALLOW_COPY_AND_ASSIGN(TestDownloadService);
};

}  // namespace offline_pages

#endif  // COMPONENTS_OFFLINE_PAGES_CORE_PREFETCH_TEST_DOWNLOAD_SERVICE_H_
