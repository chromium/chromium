// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/enterprise/browser/reporting/browser_launch/browser_launch_event_controller.h"

#include <memory>
#include <utility>

#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/test/gtest_util.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/task_environment.h"
#include "components/enterprise/browser/reporting/browser_launch/browser_launch_event_uploader.h"
#include "components/enterprise/common/proto/synced/browser_events.pb.h"
#include "components/policy/core/common/cloud/cloud_policy_client.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/testing_pref_service.h"
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

  MOCK_METHOD(PrefService*, GetPrefService, (), (const, override));
  MOCK_METHOD(const char*, GetPolicyPrefName, (), (const, override));
  MOCK_METHOD(std::string_view, GetMetricSuffix, (), (const, override));
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
    pref_service_.registry()->RegisterBooleanPref("test_policy_pref", true);

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
    EXPECT_CALL(*uploader_ptr_, GetPrefService())
        .WillRepeatedly(testing::Return(&pref_service_));
    EXPECT_CALL(*uploader_ptr_, GetPolicyPrefName())
        .WillRepeatedly(testing::Return("test_policy_pref"));
    EXPECT_CALL(*uploader_ptr_, GetMetricSuffix())
        .WillRepeatedly(testing::Return("Browser"));
    controller_ = std::make_unique<BrowserLaunchEventController>(
        std::move(collector), std::move(uploader));
  }

 protected:
  base::test::SingleThreadTaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  TestingPrefServiceSimple pref_service_;
  std::unique_ptr<BrowserLaunchEventController> controller_;
  raw_ptr<MockLaunchDataCollector> collector_ptr_;
  raw_ptr<MockBrowserLaunchEventUploader> uploader_ptr_;
};

TEST_F(BrowserLaunchEventControllerTest, SuccessfulUpload) {
  base::HistogramTester histogram_tester;
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

  histogram_tester.ExpectUniqueSample(
      "Enterprise.BrowserLaunchEvent.UploadResult.Browser",
      /*kSuccess*/ 0, 1);
  histogram_tester.ExpectUniqueSample(
      "Enterprise.BrowserLaunchEvent.RetryCount.Browser", 0, 1);
  histogram_tester.ExpectTotalCount(
      "Enterprise.BrowserLaunchEvent.ProcessCreationToUploadLatency.Browser",
      1);
  histogram_tester.ExpectUniqueSample(
      "Enterprise.BrowserLaunchEvent.SwitchCount.Browser", 1, 1);
}

TEST_F(BrowserLaunchEventControllerTest, RetryOnFailure) {
  base::HistogramTester histogram_tester;
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

  histogram_tester.ExpectUniqueSample(
      "Enterprise.BrowserLaunchEvent.UploadResult.Browser",
      /*kSuccess*/ 0, 1);
  histogram_tester.ExpectUniqueSample(
      "Enterprise.BrowserLaunchEvent.RetryCount.Browser", 1, 1);
  histogram_tester.ExpectTotalCount(
      "Enterprise.BrowserLaunchEvent.ProcessCreationToUploadLatency.Browser",
      1);
  histogram_tester.ExpectUniqueSample(
      "Enterprise.BrowserLaunchEvent.SwitchCount.Browser", 1, 1);
}

TEST_F(BrowserLaunchEventControllerTest, MaxRetriesReached) {
  base::HistogramTester histogram_tester;
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

  histogram_tester.ExpectUniqueSample(
      "Enterprise.BrowserLaunchEvent.UploadResult.Browser",
      /*kFailedRetryLimit*/ 1, 1);
  histogram_tester.ExpectTotalCount(
      "Enterprise.BrowserLaunchEvent.RetryCount.Browser", 0);
  histogram_tester.ExpectTotalCount(
      "Enterprise.BrowserLaunchEvent.ProcessCreationToUploadLatency.Browser",
      0);
  histogram_tester.ExpectUniqueSample(
      "Enterprise.BrowserLaunchEvent.SwitchCount.Browser", 1, 1);
}

TEST_F(BrowserLaunchEventControllerTest, NonRetryableFailure) {
  base::HistogramTester histogram_tester;
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

  histogram_tester.ExpectUniqueSample(
      "Enterprise.BrowserLaunchEvent.UploadResult.Browser",
      /*kFailedPermanent*/ 2, 1);
  histogram_tester.ExpectTotalCount(
      "Enterprise.BrowserLaunchEvent.RetryCount.Browser", 0);
  histogram_tester.ExpectTotalCount(
      "Enterprise.BrowserLaunchEvent.ProcessCreationToUploadLatency.Browser",
      0);
  histogram_tester.ExpectUniqueSample(
      "Enterprise.BrowserLaunchEvent.SwitchCount.Browser", 1, 1);
}

TEST_F(BrowserLaunchEventControllerTest, MultipleCallsNoop) {
  EXPECT_CALL(*collector_ptr_, GetEvent()).Times(1);
  EXPECT_CALL(*uploader_ptr_, UploadEvent(_, _)).Times(1);

  controller_->CollectAndUpload();
  controller_->CollectAndUpload();
}

TEST_F(BrowserLaunchEventControllerTest, PolicyDisabledObservesPref) {
  pref_service_.SetBoolean("test_policy_pref", false);

  EXPECT_CALL(*collector_ptr_, GetEvent()).Times(0);
  EXPECT_CALL(*uploader_ptr_, UploadEvent(_, _)).Times(0);

  controller_->CollectAndUpload();

  EXPECT_CALL(*collector_ptr_, GetEvent()).Times(1);
  EXPECT_CALL(*uploader_ptr_, UploadEvent(_, _)).Times(1);

  pref_service_.SetBoolean("test_policy_pref", true);
}

TEST_F(BrowserLaunchEventControllerTest, NotRegisteredFailure) {
  base::HistogramTester histogram_tester;
  EXPECT_CALL(*collector_ptr_, GetEvent()).Times(1);

  EXPECT_CALL(*uploader_ptr_, UploadEvent(_, _))
      .WillOnce([](const auto&,
                   base::OnceCallback<void(policy::CloudPolicyClient::Result)>
                       callback) {
        std::move(callback).Run(policy::CloudPolicyClient::Result(
            policy::CloudPolicyClient::NotRegistered()));
      });

  controller_->CollectAndUpload();

  histogram_tester.ExpectTotalCount(
      "Enterprise.BrowserLaunchEvent.UploadResult.Browser", 0);
  histogram_tester.ExpectTotalCount(
      "Enterprise.BrowserLaunchEvent.RetryCount.Browser", 0);
  histogram_tester.ExpectTotalCount(
      "Enterprise.BrowserLaunchEvent.ProcessCreationToUploadLatency.Browser",
      0);
  histogram_tester.ExpectUniqueSample(
      "Enterprise.BrowserLaunchEvent.SwitchCount.Browser", 1, 1);
}

}  // namespace enterprise_reporting
