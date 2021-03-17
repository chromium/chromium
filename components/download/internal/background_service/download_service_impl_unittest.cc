// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/download/internal/background_service/download_service_impl.h"

#include <memory>

#include "base/macros.h"
#include "base/strings/string_util.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/test_simple_task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "components/download/internal/background_service/startup_status.h"
#include "components/download/internal/background_service/stats.h"
#include "components/download/internal/background_service/test/download_params_utils.h"
#include "components/download/internal/background_service/test/mock_controller.h"
#include "components/download/public/background_service/test/empty_logger.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::_;
using testing::Return;

namespace download {
namespace {

class DownloadServiceImplTest : public testing::Test {
 public:
  DownloadServiceImplTest()
      : controller_(nullptr),
        task_runner_(new base::TestSimpleTaskRunner),
        handle_(task_runner_) {}
  ~DownloadServiceImplTest() override = default;

  void SetUp() override {
    auto config = std::make_unique<Configuration>();
    auto logger = std::make_unique<test::EmptyLogger>();
    auto controller = std::make_unique<test::MockController>();
    controller_ = controller.get();
    service_ = std::make_unique<DownloadServiceImpl>(
        std::move(config), std::move(logger), std::move(controller));
  }

 protected:
  test::MockController* controller_;
  std::unique_ptr<DownloadServiceImpl> service_;
  scoped_refptr<base::TestSimpleTaskRunner> task_runner_;
  base::ThreadTaskRunnerHandle handle_;

  DISALLOW_COPY_AND_ASSIGN(DownloadServiceImplTest);
};

}  // namespace

TEST_F(DownloadServiceImplTest, TestGetStatus) {
  StartupStatus startup_status;
  EXPECT_CALL(*controller_, GetState())
      .WillOnce(Return(Controller::State::INITIALIZING))
      .WillOnce(Return(Controller::State::READY))
      .WillOnce(Return(Controller::State::UNAVAILABLE));

  EXPECT_EQ(DownloadService::ServiceStatus::STARTING_UP, service_->GetStatus());
  EXPECT_EQ(DownloadService::ServiceStatus::READY, service_->GetStatus());
  EXPECT_EQ(DownloadService::ServiceStatus::UNAVAILABLE, service_->GetStatus());
}

TEST_F(DownloadServiceImplTest, TestApiPassThrough) {
  DownloadParams params = test::BuildBasicDownloadParams();
  SchedulingParams scheduling_params;
  scheduling_params.priority = SchedulingParams::Priority::UI;

  EXPECT_CALL(*controller_, GetOwnerOfDownload(_))
      .WillRepeatedly(Return(DownloadClient::TEST));

  EXPECT_CALL(*controller_, StartDownload(_)).Times(0);
  EXPECT_CALL(*controller_, PauseDownload(params.guid)).Times(0);
  EXPECT_CALL(*controller_, ResumeDownload(params.guid)).Times(0);
  EXPECT_CALL(*controller_, CancelDownload(params.guid)).Times(0);
  EXPECT_CALL(*controller_, ChangeDownloadCriteria(params.guid, _)).Times(0);

  {
    base::HistogramTester histogram_tester;

    service_->StartDownload(params);

    histogram_tester.ExpectBucketCount(
        "Download.Service.Request.ClientAction",
        static_cast<base::HistogramBase::Sample>(
            stats::ServiceApiAction::START_DOWNLOAD),
        1);
    histogram_tester.ExpectBucketCount(
        "Download.Service.Request.ClientAction.__Test__",
        static_cast<base::HistogramBase::Sample>(
            stats::ServiceApiAction::START_DOWNLOAD),
        1);
    histogram_tester.ExpectTotalCount("Download.Service.Request.ClientAction",
                                      1);
    histogram_tester.ExpectTotalCount(
        "Download.Service.Request.ClientAction.__Test__", 1);
  }
  service_->PauseDownload(params.guid);
  service_->ResumeDownload(params.guid);
  service_->CancelDownload(params.guid);
  service_->ChangeDownloadCriteria(params.guid, scheduling_params);
  task_runner_->RunUntilIdle();

  testing::Sequence seq1;
  EXPECT_CALL(*controller_, StartDownload(_)).Times(1).InSequence(seq1);
  EXPECT_CALL(*controller_, PauseDownload(params.guid))
      .Times(1)
      .InSequence(seq1);
  EXPECT_CALL(*controller_, ResumeDownload(params.guid))
      .Times(1)
      .InSequence(seq1);
  EXPECT_CALL(*controller_, CancelDownload(params.guid))
      .Times(1)
      .InSequence(seq1);
  EXPECT_CALL(*controller_, ChangeDownloadCriteria(params.guid, _))
      .Times(1)
      .InSequence(seq1);

  controller_->TriggerInitCompleted();
  task_runner_->RunUntilIdle();

  EXPECT_CALL(*controller_, PauseDownload(params.guid))
      .Times(1)
      .InSequence(seq1);
  EXPECT_CALL(*controller_, ResumeDownload(params.guid))
      .Times(1)
      .InSequence(seq1);
  service_->PauseDownload(params.guid);
  service_->ResumeDownload(params.guid);
  task_runner_->RunUntilIdle();
}

}  // namespace download
