// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/enterprise/browser/reporting/browser_launch/browser_launch_event_controller.h"

#include <memory>
#include <utility>

#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/test/gtest_util.h"
#include "base/test/task_environment.h"
#include "components/enterprise/browser/reporting/browser_launch/browser_launch_event_uploader.h"
#include "components/enterprise/common/proto/synced/browser_events.pb.h"
#include "components/policy/core/common/cloud/cloud_policy_client.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace enterprise_reporting {

namespace {

using ::testing::_;

constexpr int64_t kTestLaunchTime = 12345;
constexpr char kTestSwitch[] = "test-switch";

class MockLaunchDataCollector
    : public BrowserLaunchEventController::LaunchDataCollector {
 public:
  MockLaunchDataCollector() = default;
  ~MockLaunchDataCollector() override = default;

  MOCK_METHOD(::chrome::cros::reporting::proto::BrowserLaunchEvent&&,
              GetEvent,
              (),
              (override));
};

class MockBrowserLaunchEventUploader : public BrowserLaunchEventUploader {
 public:
  MockBrowserLaunchEventUploader() = default;
  ~MockBrowserLaunchEventUploader() override = default;

  MOCK_METHOD(void,
              UploadEvent,
              (const ::chrome::cros::reporting::proto::BrowserLaunchEvent&,
               base::OnceCallback<void(policy::CloudPolicyClient::Result)>),
              (override));
};

}  // namespace

class BrowserLaunchEventControllerTest : public testing::Test {
 public:
  BrowserLaunchEventControllerTest() = default;
  ~BrowserLaunchEventControllerTest() override = default;

  void SetUp() override {
    auto collector = std::make_unique<MockLaunchDataCollector>();
    collector_ptr_ = collector.get();

    ::chrome::cros::reporting::proto::BrowserLaunchEvent event;
    event.set_launch_time_millis(kTestLaunchTime);
    event.add_command_line_switch_keys(kTestSwitch);
    ON_CALL(*collector_ptr_, GetEvent())
        .WillByDefault(
            [event = std::move(event)]() mutable
            -> ::chrome::cros::reporting::proto::BrowserLaunchEvent&& {
              return std::move(event);
            });

    auto uploader = std::make_unique<MockBrowserLaunchEventUploader>();
    uploader_ptr_ = uploader.get();
    controller_ = std::make_unique<BrowserLaunchEventController>(
        std::move(collector), std::move(uploader));
  }

 protected:
  base::test::SingleThreadTaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  std::unique_ptr<BrowserLaunchEventController> controller_;
  raw_ptr<MockLaunchDataCollector> collector_ptr_;
  raw_ptr<MockBrowserLaunchEventUploader> uploader_ptr_;
};

TEST_F(BrowserLaunchEventControllerTest, SuccessfulUpload) {
  EXPECT_CALL(*collector_ptr_, GetEvent()).Times(1);
  EXPECT_CALL(*uploader_ptr_, UploadEvent(_, _))
      .WillOnce(
          [](const ::chrome::cros::reporting::proto::BrowserLaunchEvent& event,
             base::OnceCallback<void(policy::CloudPolicyClient::Result)>
                 callback) {
            EXPECT_EQ(event.launch_time_millis(), kTestLaunchTime);
            EXPECT_EQ(event.command_line_switch_keys(0), kTestSwitch);
            std::move(callback).Run(
                policy::CloudPolicyClient::Result(policy::DM_STATUS_SUCCESS));
          });

  controller_->CollectAndUpload();
}

TEST_F(BrowserLaunchEventControllerTest, RetryOnFailure) {
  EXPECT_CALL(*collector_ptr_, GetEvent()).Times(1);

  EXPECT_CALL(*uploader_ptr_, UploadEvent(_, _))
      .WillOnce([](const auto&,
                   base::OnceCallback<void(policy::CloudPolicyClient::Result)>
                       callback) {
        std::move(callback).Run(policy::CloudPolicyClient::Result(
            policy::DM_STATUS_REQUEST_FAILED));
      })
      .WillOnce([](const auto&,
                   base::OnceCallback<void(policy::CloudPolicyClient::Result)>
                       callback) {
        std::move(callback).Run(
            policy::CloudPolicyClient::Result(policy::DM_STATUS_SUCCESS));
      });

  controller_->CollectAndUpload();
  task_environment_.FastForwardBy(base::Minutes(10));
}

TEST_F(BrowserLaunchEventControllerTest, MaxRetriesReached) {
  EXPECT_CALL(*collector_ptr_, GetEvent()).Times(1);

  EXPECT_CALL(*uploader_ptr_, UploadEvent(_, _))
      .Times(5)
      .WillRepeatedly(
          [](const auto&,
             base::OnceCallback<void(policy::CloudPolicyClient::Result)>
                 callback) {
            std::move(callback).Run(policy::CloudPolicyClient::Result(
                policy::DM_STATUS_REQUEST_FAILED));
          });

  controller_->CollectAndUpload();
  task_environment_.FastForwardBy(base::Minutes(30));
}

TEST_F(BrowserLaunchEventControllerTest, NonRetryableFailure) {
  EXPECT_CALL(*collector_ptr_, GetEvent()).Times(1);

  // We should only see one upload attempt because the error is non-retryable.
  EXPECT_CALL(*uploader_ptr_, UploadEvent(_, _))
      .WillOnce([](const auto&,
                   base::OnceCallback<void(policy::CloudPolicyClient::Result)>
                       callback) {
        std::move(callback).Run(policy::CloudPolicyClient::Result(
            policy::DM_STATUS_SERVICE_MANAGEMENT_TOKEN_INVALID));
      });

  controller_->CollectAndUpload();

  // Fast forward significantly. No second attempt should happen.
  task_environment_.FastForwardBy(base::Minutes(30));
}

TEST_F(BrowserLaunchEventControllerTest, MultipleCallsTriggerCheck) {
  EXPECT_CALL(*collector_ptr_, GetEvent()).Times(1);
  EXPECT_CALL(*uploader_ptr_, UploadEvent(_, _)).Times(1);

  controller_->CollectAndUpload();
  EXPECT_CHECK_DEATH(controller_->CollectAndUpload());
}

}  // namespace enterprise_reporting
