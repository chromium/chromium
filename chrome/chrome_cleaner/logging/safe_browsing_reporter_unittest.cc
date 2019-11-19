// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/chrome_cleaner/logging/safe_browsing_reporter.h"

#include <memory>
#include <string>
#include <utility>

#include "base/bind.h"
#include "base/run_loop.h"
#include "base/test/task_environment.h"
#include "base/test/test_simple_task_runner.h"
#include "chrome/chrome_cleaner/http/mock_http_agent_factory.h"
#include "chrome/chrome_cleaner/logging/test_utils.h"
#include "components/chrome_cleaner/public/constants/result_codes.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chrome_cleaner {

namespace {
const char kSerializedReport[] = "I'm a serialized report!";
const char kTestSafeBrowsingUrl[] = "https://sb.google.com/yay";

void NoSleep(base::TimeDelta) {}
}  // namespace

using ::testing::_;
using ::testing::Return;

class SafeBrowsingReporterTest : public testing::Test {
 public:
  // SafeBrowsingReporter::OnResultCallback:
  void OnReportUploadResult(base::OnceClosure run_loop_quit,
                            SafeBrowsingReporter::Result result,
                            const std::string& serialized_report,
                            std::unique_ptr<ChromeFoilResponse> response) {
    result_ = result;
    report_upload_result_called_ = true;
    if (response.get())
      response->SerializeToString(&response_string_);
    std::move(run_loop_quit).Run();
  }

  void SetUp() override {
    SafeBrowsingReporter::SetHttpAgentFactoryForTesting(
        http_agent_factory_.get());
    SafeBrowsingReporter::SetSleepCallbackForTesting(
        base::BindRepeating(&NoSleep));
    SafeBrowsingReporter::SetNetworkCheckerForTesting(&network_checker_);
  }

  void TearDown() override {
    SafeBrowsingReporter::SetNetworkCheckerForTesting(nullptr);
    SafeBrowsingReporter::SetHttpAgentFactoryForTesting(nullptr);
  }

 protected:
  SafeBrowsingReporterTest()
      : task_environment_(base::test::TaskEnvironment::MainThreadType::UI) {}

  // Uploads a report and waits for OnReportUploadResult to be called.
  void DoUploadReport(const std::string& serialized_report) {
    base::RunLoop run_loop;
    SafeBrowsingReporter::UploadReport(
        base::BindRepeating(&SafeBrowsingReporterTest::OnReportUploadResult,
                            base::Unretained(this), run_loop.QuitClosure()),
        kTestSafeBrowsingUrl, serialized_report, TRAFFIC_ANNOTATION_FOR_TESTS);
    run_loop.Run();
  }

  // Needed for the current task runner to be available.
  base::test::TaskEnvironment task_environment_;

  // |result_| and |response_string_| are set in |OnReportUploadResult| and used
  // to confirm that the upload succeeded or failed appropriately.
  SafeBrowsingReporter::Result result_{
      SafeBrowsingReporter::Result::UPLOAD_INTERNAL_ERROR};
  std::string response_string_;

  MockHttpAgentConfig config_;
  std::unique_ptr<HttpAgentFactory> http_agent_factory_{
      std::make_unique<MockHttpAgentFactory>(&config_)};
  MockNetworkChecker network_checker_;

  // Confirm the execution of |OnReportUploadResult| and that |result_| and
  // |response_string_| are valid.
  bool report_upload_result_called_{false};

 private:
  DISALLOW_COPY_AND_ASSIGN(SafeBrowsingReporterTest);
};

TEST_F(SafeBrowsingReporterTest, Success) {
  MockHttpAgentConfig::Calls calls(HttpStatus::kOk);

  ChromeFoilResponse response;
  response.set_token("Token");
  std::string response_string;
  response.SerializeToString(&response_string);
  calls.read_data_result = response_string;

  config_.AddCalls(calls);

  DoUploadReport(kSerializedReport);

  ASSERT_GT(config_.num_request_data(), 0u);
  EXPECT_EQ(kSerializedReport, config_.request_data(0).body);

  EXPECT_EQ(SafeBrowsingReporter::Result::UPLOAD_SUCCESS, result_);
  EXPECT_EQ(response_string_, response_string);

  EXPECT_TRUE(report_upload_result_called_);
}

TEST_F(SafeBrowsingReporterTest, Failure) {
  MockHttpAgentConfig::Calls calls(HttpStatus::kOk);
  calls.request_succeeds = false;
  // Request fails on all tries. No retry without log lines.
  config_.AddCalls(calls);
  config_.AddCalls(calls);
  config_.AddCalls(calls);

  DoUploadReport(kSerializedReport);

  EXPECT_EQ(SafeBrowsingReporter::Result::UPLOAD_REQUEST_FAILED, result_);
  EXPECT_TRUE(response_string_.empty());
  EXPECT_EQ(config_.num_request_data(), 3UL);

  EXPECT_TRUE(report_upload_result_called_);
}

TEST_F(SafeBrowsingReporterTest, RetryOnlyOnceWhenFailing) {
  MockHttpAgentConfig::Calls calls_failed(HttpStatus::kNotFound);
  // Fail on every try with the same error.
  config_.AddCalls(calls_failed);
  config_.AddCalls(calls_failed);
  config_.AddCalls(calls_failed);

  DoUploadReport(kSerializedReport);

  EXPECT_EQ(SafeBrowsingReporter::Result::UPLOAD_REQUEST_FAILED, result_);
  EXPECT_TRUE(report_upload_result_called_);

  ASSERT_EQ(3u, config_.num_request_data());

  // Expect all requests to have the same contents.
  EXPECT_EQ(config_.request_data(0).body, config_.request_data(1).body);
  EXPECT_EQ(config_.request_data(0).body, config_.request_data(2).body);
}

TEST_F(SafeBrowsingReporterTest, WaitForSafeBrowsing) {
  MockHttpAgentConfig::Calls calls(HttpStatus::kOk);

  ChromeFoilResponse response;
  response.set_token("Token");
  std::string response_string;
  response.SerializeToString(&response_string);
  calls.read_data_result = response_string;

  config_.AddCalls(calls);

  // Safe Browsing is initially not reachable, but waiting for it result in
  // it becoming reachable. Everything should succeed.
  network_checker_.SetIsSafeBrowsingReachableResult(false);
  network_checker_.SetWaitForSafeBrowsingResult(true);

  DoUploadReport(kSerializedReport);

  ASSERT_GT(config_.num_request_data(), 0u);
  EXPECT_EQ(kSerializedReport, config_.request_data(0).body);

  EXPECT_EQ(SafeBrowsingReporter::Result::UPLOAD_SUCCESS, result_);
  EXPECT_EQ(response_string_, response_string);

  EXPECT_TRUE(report_upload_result_called_);
}

TEST_F(SafeBrowsingReporterTest, NoNetwork) {
  MockHttpAgentConfig::Calls calls(HttpStatus::kOk);

  config_.AddCalls(calls);

  // Safe Browsing never becomes reachable, and WaitForSafeBrowsing times
  // out. UPLOAD_NO_NETWORK should be the result code, and the callback should
  // have run anyway.
  network_checker_.SetIsSafeBrowsingReachableResult(false);
  network_checker_.SetWaitForSafeBrowsingResult(false);

  DoUploadReport(kSerializedReport);

  ASSERT_EQ(config_.num_request_data(), 0u);
  EXPECT_EQ(SafeBrowsingReporter::Result::UPLOAD_NO_NETWORK, result_);
  EXPECT_TRUE(report_upload_result_called_);
}

}  // namespace chrome_cleaner
