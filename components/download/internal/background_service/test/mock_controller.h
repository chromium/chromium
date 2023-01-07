// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DOWNLOAD_INTERNAL_BACKGROUND_SERVICE_TEST_MOCK_CONTROLLER_H_
#define COMPONENTS_DOWNLOAD_INTERNAL_BACKGROUND_SERVICE_TEST_MOCK_CONTROLLER_H_

#include <string>

#include "components/download/internal/background_service/controller.h"
#include "components/download/internal/background_service/startup_status.h"
#include "components/download/public/background_service/download_params.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace download {
namespace test {

class MockController : public Controller {
 public:
  MockController();

  MockController(const MockController&) = delete;
  MockController& operator=(const MockController&) = delete;

  ~MockController() override;

  // Controller implementation.
  void Initialize(base::OnceClosure callback) override;
  MOCK_METHOD(Controller::State, GetState, (), (override));
  MOCK_METHOD(const ServiceConfig&, GetConfig, (), (override));
  MOCK_METHOD(BackgroundDownloadService::ServiceStatus,
              GetStatus,
              (),
              (override));
  MOCK_METHOD(void, StartDownload, (DownloadParams), (override));
  MOCK_METHOD(void, PauseDownload, (const std::string&), (override));
  MOCK_METHOD(void, ResumeDownload, (const std::string&), (override));
  MOCK_METHOD(void, CancelDownload, (const std::string&), (override));
  MOCK_METHOD(void,
              ChangeDownloadCriteria,
              (const std::string&, const SchedulingParams&),
              (override));
  MOCK_METHOD(DownloadClient,
              GetOwnerOfDownload,
              (const std::string&),
              (override));
  MOCK_METHOD(void,
              OnStartScheduledTask,
              (DownloadTaskType, TaskFinishedCallback),
              (override));
  MOCK_METHOD(bool, OnStopScheduledTask, (DownloadTaskType), (override));
  MOCK_METHOD(Logger*, GetLogger, (), (override));

  void TriggerInitCompleted();

 private:
  base::OnceClosure init_callback_;
};

}  // namespace test
}  // namespace download

#endif  // COMPONENTS_DOWNLOAD_INTERNAL_BACKGROUND_SERVICE_TEST_MOCK_CONTROLLER_H_
