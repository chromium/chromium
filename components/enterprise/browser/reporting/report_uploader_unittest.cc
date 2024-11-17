// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "components/enterprise/browser/reporting/report_uploader.h"

#include <utility>

#include "base/test/metrics/histogram_tester.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
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

// Returns a function that schedules a callback it is passed as second parameter
// with the given result. Useful to test `UploadReport` function.
auto ScheduleResponse(policy::CloudPolicyClient::Result result) {
  return [result](auto /*report*/, auto callback) {
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), result));
  };
}

}  // namespace

class ReportUploaderTest : public ::testing::Test {
 public:
  // Different CloudPolicyClient functions will be used in test cases based
  // on the current operation system. They share same retry and error handling
  // behaviors provided by ReportUploader.
#if BUILDFLAG(IS_CHROMEOS_ASH)
#define UploadReport UploadChromeOsUserReport
#else
#define UploadReport UploadChromeDesktopReport
#endif

  ReportUploaderTest()
      : task_environment_(base::test::TaskEnvironment::TimeSource::MOCK_TIME) {
    CreateUploader(0);
  }

  ReportUploaderTest(const ReportUploaderTest&) = delete;
  ReportUploaderTest& operator=(const ReportUploaderTest&) = delete;

  ~ReportUploaderTest() override = default;

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

// TODO(crbug.com/40483507) This death test does not work on Android.
#if defined(GTEST_HAS_DEATH_TEST) && !BUILDFLAG(IS_ANDROID)
TEST_F(ReportUploaderTest, NotRegisteredCrashes) {
  CreateUploader(/* retry_count = */ 1);
  EXPECT_CALL(client_, UploadReport)
      .WillOnce(ScheduleResponse(policy::CloudPolicyClient::Result(
          policy::CloudPolicyClient::NotRegistered())));
  ReportRequestQueue requests;
  requests.push(std::make_unique<ReportRequest>(GetReportType()));
  base::test::TestFuture<ReportUploader::ReportStatus> future;
  uploader_->SetRequestAndUpload(GetReportType(), std::move(requests),
                                 future.GetCallback());
  ASSERT_DEATH(std::ignore = future.Get(), "");
}
#endif  // defined(GTEST_HAS_DEATH_TEST) && !BUILDFLAG(IS_ANDROID)

TEST_F(ReportUploaderTest, PersistentError) {
  CreateUploader(/* retry_count = */ 1);
  EXPECT_CALL(client_, UploadReport)
      .WillOnce(ScheduleResponse(policy::CloudPolicyClient::Result(
          policy::DM_STATUS_SERVICE_DEVICE_NOT_FOUND)));
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
  EXPECT_CALL(client_, UploadReport)
      .Times(2)
      .WillOnce(ScheduleResponse(policy::CloudPolicyClient::Result(
          policy::DM_STATUS_REQUEST_TOO_LARGE)))
      .WillOnce(ScheduleResponse(policy::CloudPolicyClient::Result(
          policy::DM_STATUS_REQUEST_TOO_LARGE)));
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
  EXPECT_CALL(client_, UploadReport)
      .Times(2)
      .WillOnce(ScheduleResponse(policy::CloudPolicyClient::Result(
          policy::DM_STATUS_TEMPORARY_UNAVAILABLE)))
      .WillOnce(ScheduleResponse(
          policy::CloudPolicyClient::Result(policy::DM_STATUS_SUCCESS)));
  CreateUploader(/* retry_count = */ 1);
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
  EXPECT_CALL(client_, UploadReport)
      .Times(1)
      .WillOnce(ScheduleResponse(policy::CloudPolicyClient::Result(
          policy::DM_STATUS_TEMPORARY_UNAVAILABLE)));
  CreateUploader(/* retry_count = */ 1);
  UploadReportAndSetExpectation(/*number_of_request=*/1,
                                ReportUploader::kPersistentError);
  RunNextTask();

  histogram_tester_.ExpectUniqueSample(
      kResponseMetricsName, ReportResponseMetricsStatus::kTemporaryServerError,
      1);

  // No response, request is retried.
  EXPECT_FALSE(has_responded_);
  // Error is changed.
  EXPECT_CALL(client_, UploadReport)
      .Times(1)
      .WillOnce(ScheduleResponse(policy::CloudPolicyClient::Result(
          policy::DM_STATUS_SERVICE_DEVICE_NOT_FOUND)));
  RunNextTask();
  EXPECT_TRUE(has_responded_);
  ::testing::Mock::VerifyAndClearExpectations(&client_);
  histogram_tester_.ExpectTotalCount(kResponseMetricsName, 2);
  histogram_tester_.ExpectBucketCount(
      kResponseMetricsName, ReportResponseMetricsStatus::kOtherError, 1);
}

TEST_F(ReportUploaderTest, RetryAndFailedWithTransientError) {
  EXPECT_CALL(client_, UploadReport)
      .Times(2)
      .WillRepeatedly(ScheduleResponse(policy::CloudPolicyClient::Result(
          policy::DM_STATUS_TEMPORARY_UNAVAILABLE)));
  CreateUploader(/* retry_count = */ 1);
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
        UploadReport(
            Pointee(Property(
                &ReportRequest::DeviceReportRequestProto::browser_report,
                Property(&em::BrowserReport::browser_version,
                         Eq(kBrowserVersionNames[0])))),
            _))
        .Times(3)
        .WillOnce(ScheduleResponse(policy::CloudPolicyClient::Result(
            policy::DM_STATUS_TEMPORARY_UNAVAILABLE)))
        .WillOnce(ScheduleResponse(policy::CloudPolicyClient::Result(
            policy::DM_STATUS_TEMPORARY_UNAVAILABLE)))
        .WillOnce(ScheduleResponse(
            policy::CloudPolicyClient::Result(policy::DM_STATUS_SUCCESS)));
    // Second report
    EXPECT_CALL(
        client_,
        UploadReport(
            Pointee(Property(
                &ReportRequest::DeviceReportRequestProto::browser_report,
                Property(&em::BrowserReport::browser_version,
                         Eq(kBrowserVersionNames[1])))),
            _))
        .Times(2)
        .WillOnce(ScheduleResponse(policy::CloudPolicyClient::Result(
            policy::DM_STATUS_TEMPORARY_UNAVAILABLE)))
        .WillOnce(ScheduleResponse(policy::CloudPolicyClient::Result(
            policy::DM_STATUS_TEMPORARY_UNAVAILABLE)));
  }
  CreateUploader(/* retry_count = */ 2);
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
  EXPECT_CALL(client_, UploadReport)
      .WillOnce(
          ScheduleResponse(policy::CloudPolicyClient::Result(GetParam())));
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
      EXPECT_CALL(client_, UploadReport)
          .WillOnce(ScheduleResponse(
              policy::CloudPolicyClient::Result(policy::DM_STATUS_SUCCESS)));
      break;
    case ReportType::kProfileReport:
      EXPECT_CALL(client_, UploadChromeProfileReport)
          .WillOnce(ScheduleResponse(
              policy::CloudPolicyClient::Result(policy::DM_STATUS_SUCCESS)));
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
