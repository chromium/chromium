// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DOWNLOAD_PUBLIC_BACKGROUND_SERVICE_TEST_MOCK_DOWNLOAD_SERVICE_H_
#define COMPONENTS_DOWNLOAD_PUBLIC_BACKGROUND_SERVICE_TEST_MOCK_DOWNLOAD_SERVICE_H_

#include <string>

#include "components/download/public/background_service/background_download_service.h"
#include "components/download/public/background_service/download_params.h"
#include "components/download/public/background_service/service_config.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace download {
namespace test {

class MockDownloadService : public BackgroundDownloadService {
 public:
  MockDownloadService();

  MockDownloadService(const MockDownloadService&) = delete;
  MockDownloadService& operator=(const MockDownloadService&) = delete;

  ~MockDownloadService() override;

  // BackgroundDownloadService implementation.
  MOCK_METHOD0(GetConfig, const ServiceConfig&());
  MOCK_METHOD2(OnStartScheduledTask,
               void(DownloadTaskType task_type, TaskFinishedCallback callback));
  MOCK_METHOD1(OnStopScheduledTask, bool(DownloadTaskType task_type));
  MOCK_METHOD0(GetStatus, ServiceStatus());

  void StartDownload(DownloadParams download_params) override {
    // Redirect as gmock can't handle move-only types.
    StartDownload_(download_params);
  }

  MOCK_METHOD1(StartDownload_, void(DownloadParams& download_params));
  MOCK_METHOD1(PauseDownload, void(const std::string& guid));
  MOCK_METHOD1(ResumeDownload, void(const std::string& guid));
  MOCK_METHOD1(CancelDownload, void(const std::string& guid));
  MOCK_METHOD2(ChangeDownloadCriteria,
               void(const std::string& guid, const SchedulingParams& params));
  MOCK_METHOD0(GetLogger, Logger*());
};

}  // namespace test
}  // namespace download

#endif  // COMPONENTS_DOWNLOAD_PUBLIC_BACKGROUND_SERVICE_TEST_MOCK_DOWNLOAD_SERVICE_H_
