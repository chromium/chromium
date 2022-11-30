// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/enterprise/browser/reporting/report_uploader.h"

#include <utility>

#include "base/test/metrics/histogram_tester.h"
#include "base/test/task_environment.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "components/enterprise/browser/reporting/report_request.h"
#include "components/enterprise/browser/reporting/report_type.h"
#include "components/policy/core/common/cloud/cloud_policy_constants.h"
#include "components/policy/core/common/cloud/mock_cloud_policy_client.h"
#include "device_management_backend.pb.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::_;
using ::testing::Eq;
using ::testing::InSequence;
using ::testing::Property;
using ::testing::WithArgs;

namespace em = enterprise_management;

namespace enterprise_reporting {
namespace {
constexpr const char* kBrowserVersionNames[] = {"name1", "name2"};
constexpr char kResponseMetricsName[] = "Enterprise.CloudReportingResponse";

}  // namespace

class ReportUploaderTest : public ::testing::Test {
 public:
  // Different CloudPolicyClient proxy function will be used in test cases based
  // on the current operation system. They share same retry and error handling
  // behaviors provided by ReportUploader.
#if BUILDFLAG(IS_CHROMEOS_ASH)
#define UploadReportProxy UploadChromeOsUserReportProxy
#else
#define UploadReportProxy UploadChromeDesktopReportProxy
#endif

  ReportUploaderTest()
      : task_environment_(base::test::TaskEnvironment::TimeSource::MOCK_TIME) {
    CreateUploader(0);
  }

  ReportUploaderTest(const ReportUploaderTest&) = delete;
  ReportUploaderTest& operator=(const ReportUploaderTest&) = delete;

  ~ReportUploaderTest() override {}

  void UploadReportAndSetExpectation(
      int number_of_request,
      ReportUploader::ReportStatus expected_status) {
    DCHECK_LE(number_of_request, 2)
        << "Please update kBrowserVersionNames above.";
    ReportRequestQueue requests;
    for (int i = 0; i < number_of_request; i++) {
      auto request = std::make_unique<ReportRequest>(GetReportType());
      em::BrowserReport* browser_report;
      switch (GetReportType()) {
        case ReportType::kFull:
        case ReportType::kBrowserVersion:
          browser_report =
              request->GetDeviceReportRequest().mutable_browser_report();
          break;
        case ReportType::kProfileReport:
          browser_report =
              request->GetChromeProfileReportRequest().mutable_browser_report();
          break;
      }
      browser_report->set_browser_version(kBrowserVersionNames[i]);
      requests.push(std::move(request));
    }
    has_responded_ = false;
    uploader_->SetRequestAndUpload(
        GetReportType(), std::move(requests),
        base::BindOnce(&ReportUploaderTest::OnReportUploaded,
                       base::Unretained(this), expected_status));
  }

  virtual ReportType GetReportType() { return ReportType::kFull; }

  void OnReportUploaded(ReportUploader::ReportStatus expected_status,
                        ReportUploader::ReportStatus actuall_status) {
    EXPECT_EQ(expected_status, actuall_status);
    has_responded_ = true;
  }

  void CreateUploader(int retry_count) {
    uploader_ = std::make_unique<ReportUploader>(&client_, retry_count);
  }

  // Forwards to send next request and get response.
  void RunNextTask() {
    task_environment_.FastForwardBy(
        task_environment_.NextMainThreadPendingTaskDelay());
  }

  // Verifies the retried is delayed properly.
  void VerifyRequestDelay(int delay_seconds) {
    if (delay_seconds == 0) {
      EXPECT_EQ(base::TimeDelta(),
                task_environment_.NextMainThreadPendingTaskDelay());
      return;
    }
    EXPECT_GE(base::Seconds(delay_seconds),
              task_environment_.NextMainThreadPendingTaskDelay());
    EXPECT_LE(base::Seconds(static_cast<int>(delay_seconds * 0.9)),
              task_environment_.NextMainThreadPendingTaskDelay());
  }

  base::test::TaskEnvironment task_environment_;

  std::unique_ptr<ReportUploader> uploader_;
  ::testing::StrictMock<policy::MockCloudPolicyClient> client_;
  bool has_responded_ = false;
  base::HistogramTester histogram_tester_;
};

class ReportUploaderTestWithTransientError
    : public ReportUploaderTest,
      public ::testing::WithParamInterface<policy::DeviceManagementStatus> {};

class ReportUploaderTestWithReportType
    : public ReportUploaderTest,
      public ::testing::WithParamInterface<ReportType> {
 public:
  ReportType GetReportType() override { return GetParam(); }
};

TEST_F(ReportUploaderTest, PersistentError) {
  CreateUploader(/* retry_count = */ 1);
  EXPECT_CALL(client_, UploadReportProxy(_, _))
      .WillOnce(WithArgs<1>(policy::ScheduleStatusCallback(false)));
  client_.SetStatus(policy::DM_STATUS_SERVICE_DEVICE_NOT_FOUND);
  UploadReportAndSetExpectation(/*number_of_request=*/2,
                                ReportUploader::kPersistentError);
  RunNextTask();
  EXPECT_TRUE(has_responded_);
  histogram_tester_.ExpectUniqueSample(
      kResponseMetricsName, ReportResponseMetricsStatus::kOtherError, 1);
  ::testing::Mock::VerifyAndClearExpectations(&client_);
}

TEST_F(ReportUploaderTest, RequestTooBigError) {
  CreateUploader(/* *retry_count = */ 2);
  EXPECT_CALL(client_, UploadReportProxy(_, _))
      .Times(2)
      .WillOnce(WithArgs<1>(policy::ScheduleStatusCallback(false)))
      .WillOnce(WithArgs<1>(policy::ScheduleStatusCallback(false)));
  client_.SetStatus(policy::DM_STATUS_REQUEST_TOO_LARGE);
  UploadReportAndSetExpectation(/*number_of_request=*/2,
                                ReportUploader::kSuccess);
  RunNextTask();
  EXPECT_TRUE(has_responded_);
  histogram_tester_.ExpectUniqueSample(
      kResponseMetricsName, ReportResponseMetricsStatus::kRequestTooLargeError,
      2);
  ::testing::Mock::VerifyAndClearExpectations(&client_);
}

TEST_F(ReportUploaderTest, RetryAndSuccess) {
  EXPECT_CALL(client_, UploadReportProxy(_, _))
      .Times(2)
      .WillOnce(WithArgs<1>(policy::ScheduleStatusCallback(false)))
      .WillOnce(WithArgs<1>(policy::ScheduleStatusCallback(true)));
  CreateUploader(/* retry_count = */ 1);
  client_.SetStatus(policy::DM_STATUS_TEMPORARY_UNAVAILABLE);
  UploadReportAndSetExpectation(/*number_of_request=*/1,
                                ReportUploader::kSuccess);
  RunNextTask();

  // No response, request is retried.
  EXPECT_FALSE(has_responded_);
  RunNextTask();
  EXPECT_TRUE(has_responded_);
  ::testing::Mock::VerifyAndClearExpectations(&client_);
  histogram_tester_.ExpectTotalCount(kResponseMetricsName, 2);
  histogram_tester_.ExpectBucketCount(kResponseMetricsName,
                                      ReportResponseMetricsStatus::kSuccess, 1);
  histogram_tester_.ExpectBucketCount(
      kResponseMetricsName, ReportResponseMetricsStatus::kTemporaryServerError,
      1);
}

TEST_F(ReportUploaderTest, RetryAndFailedWithPersistentError) {
  EXPECT_CALL(client_, UploadReportProxy(_, _))
      .Times(2)
      .WillRepeatedly(WithArgs<1>(policy::ScheduleStatusCallback(false)));
  CreateUploader(/* retry_count = */ 1);
  client_.SetStatus(policy::DM_STATUS_TEMPORARY_UNAVAILABLE);
  UploadReportAndSetExpectation(/*number_of_request=*/1,
                                ReportUploader::kPersistentError);
  RunNextTask();

  histogram_tester_.ExpectUniqueSample(
      kResponseMetricsName, ReportResponseMetricsStatus::kTemporaryServerError,
      1);

  // No response, request is retried.
  EXPECT_FALSE(has_responded_);
  // Error is changed.
  client_.SetStatus(policy::DM_STATUS_SERVICE_DEVICE_NOT_FOUND);
  RunNextTask();
  EXPECT_TRUE(has_responded_);
  ::testing::Mock::VerifyAndClearExpectations(&client_);
  histogram_tester_.ExpectTotalCount(kResponseMetricsName, 2);
  histogram_tester_.ExpectBucketCount(
      kResponseMetricsName, ReportResponseMetricsStatus::kOtherError, 1);
}

TEST_F(ReportUploaderTest, RetryAndFailedWithTransientError) {
  EXPECT_CALL(client_, UploadReportProxy(_, _))
      .Times(2)
      .WillRepeatedly(WithArgs<1>(policy::ScheduleStatusCallback(false)));
  CreateUploader(/* retry_count = */ 1);
  client_.SetStatus(policy::DM_STATUS_TEMPORARY_UNAVAILABLE);
  UploadReportAndSetExpectation(/*number_of_request=*/1,
                                ReportUploader::kTransientError);
  RunNextTask();

  histogram_tester_.ExpectUniqueSample(
      kResponseMetricsName, ReportResponseMetricsStatus::kTemporaryServerError,
      1);

  // No response, request is retried.
  EXPECT_FALSE(has_responded_);
  RunNextTask();
  EXPECT_TRUE(has_responded_);
  histogram_tester_.ExpectUniqueSample(
      kResponseMetricsName, ReportResponseMetricsStatus::kTemporaryServerError,
      2);
  ::testing::Mock::VerifyAndClearExpectations(&client_);
}

TEST_F(ReportUploaderTest, MultipleReports) {
  {
    InSequence s;
    // First report
    EXPECT_CALL(
        client_,
        UploadReportProxy(
            Property(&ReportRequest::DeviceReportRequestProto::browser_report,
                     Property(&em::BrowserReport::browser_version,
                              Eq(kBrowserVersionNames[0]))),
            _))
        .Times(3)
        .WillOnce(WithArgs<1>(policy::ScheduleStatusCallback(false)))
        .WillOnce(WithArgs<1>(policy::ScheduleStatusCallback(false)))
        .WillOnce(WithArgs<1>(policy::ScheduleStatusCallback(true)));
    // Second report
    EXPECT_CALL(
        client_,
        UploadReportProxy(
            Property(&ReportRequest::DeviceReportRequestProto::browser_report,
                     Property(&em::BrowserReport::browser_version,
                              Eq(kBrowserVersionNames[1]))),
            _))
        .Times(2)
        .WillOnce(WithArgs<1>(policy::ScheduleStatusCallback(false)))
        .WillOnce(WithArgs<1>(policy::ScheduleStatusCallback(false)));
  }
  CreateUploader(/* retry_count = */ 2);
  client_.SetStatus(policy::DM_STATUS_TEMPORARY_UNAVAILABLE);
  UploadReportAndSetExpectation(/*number_of_request=*/2,
                                ReportUploader::kTransientError);

  // The first request has no delay.
  VerifyRequestDelay(0);
  RunNextTask();

  // The first retry is delayed between 54 to 60 seconds.
  VerifyRequestDelay(60);
  RunNextTask();

  // The second retry is delayed between 108 to 120 seconds.
  VerifyRequestDelay(120);
  RunNextTask();

  // Request is succeeded, send the next request And its first retry is delayed
  // between 108 to 120 seconds because there were 2 failures.
  VerifyRequestDelay(120);
  RunNextTask();

  // And we failed again, reach maximum retries count.
  EXPECT_TRUE(has_responded_);

  ::testing::Mock::VerifyAndClearExpectations(&client_);
}

// Verified three DM server error that is transient.
TEST_P(ReportUploaderTestWithTransientError, WithoutRetry) {
  EXPECT_CALL(client_, UploadReportProxy(_, _))
      .WillOnce(WithArgs<1>(policy::ScheduleStatusCallback(false)));
  client_.SetStatus(GetParam());
  UploadReportAndSetExpectation(/*number_of_request=*/2,
                                ReportUploader::kTransientError);
  task_environment_.FastForwardBy(base::TimeDelta());
  EXPECT_TRUE(has_responded_);
  ::testing::Mock::VerifyAndClearExpectations(&client_);
}

INSTANTIATE_TEST_SUITE_P(
    All,
    ReportUploaderTestWithTransientError,
    ::testing::Values(policy::DM_STATUS_REQUEST_FAILED,
                      policy::DM_STATUS_TEMPORARY_UNAVAILABLE,
                      policy::DM_STATUS_SERVICE_TOO_MANY_REQUESTS));

TEST_P(ReportUploaderTestWithReportType, Success) {
  switch (GetReportType()) {
    case ReportType::kFull:
    case ReportType::kBrowserVersion:
      EXPECT_CALL(client_, UploadReportProxy(_, _))
          .WillOnce(WithArgs<1>(policy::ScheduleStatusCallback(true)));
      break;
    case ReportType::kProfileReport:
      EXPECT_CALL(client_, UploadChromeProfileReportProxy(_, _))
          .WillOnce(WithArgs<1>(policy::ScheduleStatusCallback(true)));
      break;
  }

  UploadReportAndSetExpectation(/*number_of_request=*/1,
                                ReportUploader::kSuccess);
  RunNextTask();
  EXPECT_TRUE(has_responded_);
  histogram_tester_.ExpectUniqueSample(
      kResponseMetricsName, ReportResponseMetricsStatus::kSuccess, 1);
  ::testing::Mock::VerifyAndClearExpectations(&client_);
}

INSTANTIATE_TEST_SUITE_P(All,
                         ReportUploaderTestWithReportType,
                         ::testing::Values(ReportType::kFull,
                                           ReportType::kBrowserVersion,
                                           ReportType::kProfileReport));

}  // namespace enterprise_reporting
