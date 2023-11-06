// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/download/internal/background_service/init_aware_background_download_service.h"

#include <memory>

#include "base/memory/raw_ptr.h"
#include "base/strings/string_util.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/test_simple_task_runner.h"
#include "components/download/internal/background_service/stats.h"
#include "components/download/internal/background_service/test/download_params_utils.h"
#include "components/download/internal/background_service/test/mock_controller.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::_;
using testing::Return;

namespace download {
namespace {

class InitAwareBackgroundDownloadServiceTest : public testing::Test {
 public:
  InitAwareBackgroundDownloadServiceTest()
      : controller_(nullptr),
        task_runner_(new base::TestSimpleTaskRunner),
        current_default_handle_(task_runner_) {}

  InitAwareBackgroundDownloadServiceTest(
      const InitAwareBackgroundDownloadServiceTest&) = delete;
  InitAwareBackgroundDownloadServiceTest& operator=(
      const InitAwareBackgroundDownloadServiceTest&) = delete;

  ~InitAwareBackgroundDownloadServiceTest() override = default;

  void SetUp() override {
    auto controller = std::make_unique<test::MockController>();
    controller_ = controller.get();
    service_ = std::make_unique<InitAwareBackgroundDownloadService>(
        std::move(controller));
  }

  void TearDown() override {
    controller_ = nullptr;
  }

 protected:
  raw_ptr<test::MockController> controller_;
  std::unique_ptr<InitAwareBackgroundDownloadService> service_;
  scoped_refptr<base::TestSimpleTaskRunner> task_runner_;
  base::SingleThreadTaskRunner::CurrentDefaultHandle current_default_handle_;
};

}  // namespace

TEST_F(InitAwareBackgroundDownloadServiceTest, TestGetStatus) {
  StartupStatus startup_status;
  EXPECT_CALL(*controller_, GetStatus())
      .WillOnce(Return(BackgroundDownloadService::ServiceStatus::STARTING_UP))
      .WillOnce(Return(BackgroundDownloadService::ServiceStatus::READY))
      .WillOnce(Return(BackgroundDownloadService::ServiceStatus::UNAVAILABLE));

  EXPECT_EQ(BackgroundDownloadService::ServiceStatus::STARTING_UP,
            service_->GetStatus());
  EXPECT_EQ(BackgroundDownloadService::ServiceStatus::READY,
            service_->GetStatus());
  EXPECT_EQ(BackgroundDownloadService::ServiceStatus::UNAVAILABLE,
            service_->GetStatus());
}

TEST_F(InitAwareBackgroundDownloadServiceTest, TestApiPassThrough) {
  DownloadParams params = test::BuildBasicDownloadParams();
  auto guid = params.guid;
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

    service_->StartDownload(std::move(params));

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
  service_->PauseDownload(guid);
  service_->ResumeDownload(guid);
  service_->CancelDownload(guid);
  service_->ChangeDownloadCriteria(guid, scheduling_params);
  task_runner_->RunUntilIdle();

  testing::Sequence seq1;
  EXPECT_CALL(*controller_, StartDownload(_)).Times(1).InSequence(seq1);
  EXPECT_CALL(*controller_, PauseDownload(guid)).Times(1).InSequence(seq1);
  EXPECT_CALL(*controller_, ResumeDownload(guid)).Times(1).InSequence(seq1);
  EXPECT_CALL(*controller_, CancelDownload(guid)).Times(1).InSequence(seq1);
  EXPECT_CALL(*controller_, ChangeDownloadCriteria(guid, _))
      .Times(1)
      .InSequence(seq1);

  controller_->TriggerInitCompleted();
  task_runner_->RunUntilIdle();

  EXPECT_CALL(*controller_, PauseDownload(guid)).Times(1).InSequence(seq1);
  EXPECT_CALL(*controller_, ResumeDownload(guid)).Times(1).InSequence(seq1);
  service_->PauseDownload(guid);
  service_->ResumeDownload(guid);
  task_runner_->RunUntilIdle();
}

}  // namespace download
