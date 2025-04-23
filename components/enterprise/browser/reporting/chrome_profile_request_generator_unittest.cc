// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/enterprise/browser/reporting/chrome_profile_request_generator.h"

#include <vector>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/test/test_future.h"
#include "components/device_signals/core/browser/mock_signals_aggregator.h"
#include "components/enterprise/browser/reporting/fake_browser_report_generator_delegate.h"
#include "components/enterprise/browser/reporting/report_generation_config.h"
#include "components/enterprise/browser/reporting/report_util.h"
#include "components/enterprise/browser/reporting/reporting_delegate_factory.h"
#include "components/policy/core/common/cloud/cloud_policy_util.h"
#include "components/policy/proto/device_management_backend.pb.h"
#include "components/version_info/channel.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::_;
using testing::Invoke;
using testing::StrictMock;

namespace enterprise_reporting {

namespace em = enterprise_reporting;

namespace {

const base::FilePath::CharType kProfilePath[] =
    FILE_PATH_LITERAL("profile-path");
constexpr char kBrowserExePath[] = "browser-path";

void VerifyReportContent(const ReportRequestQueue& requests) {
  ASSERT_EQ(1u, requests.size());
  ReportRequest* request = requests.front().get();
  ASSERT_TRUE(request);
  ASSERT_TRUE(request->GetChromeProfileReportRequest().has_os_report());
  auto os_report = request->GetChromeProfileReportRequest().os_report();
  EXPECT_EQ(policy::GetOSPlatform(), os_report.name());
  EXPECT_EQ(policy::GetOSArchitecture(), os_report.arch());
  EXPECT_EQ(policy::GetOSVersion(), os_report.version());

  ASSERT_TRUE(request->GetChromeProfileReportRequest().has_browser_report());
  auto browser_report =
      request->GetChromeProfileReportRequest().browser_report();
  EXPECT_EQ(ObfuscateFilePath(kBrowserExePath),
            browser_report.executable_path());
  EXPECT_TRUE(browser_report.is_extended_stable_channel());

  ASSERT_EQ(1, browser_report.chrome_user_profile_infos_size());
  auto profile_report = browser_report.chrome_user_profile_infos(0);
  EXPECT_EQ(ObfuscateFilePath(base::FilePath(kProfilePath).AsUTF8Unsafe()),
            profile_report.id());
  EXPECT_TRUE(profile_report.is_detail_available());
}

device_signals::SignalsAggregationRequest CreateExpectedRequest() {
  device_signals::SignalsAggregationRequest request;
  request.signal_names.emplace(device_signals::SignalName::kOsSignals);
  request.signal_names.emplace(
      device_signals::SignalName::kBrowserContextSignals);
#if BUILDFLAG(IS_WIN)
  request.signal_names.emplace(device_signals::SignalName::kAntiVirus);
  request.signal_names.emplace(device_signals::SignalName::kHotfixes);
#endif  // BUILDFLAG(IS_WIN)
  request.trigger = device_signals::Trigger::kSignalsReport;

  return request;
}

device_signals::SignalsAggregationResponse CreateFilledResponse() {
  // TODO(402486793): Create customized collected value so we can confirm if
  // collected signals correctly override original report values in some cases.
  device_signals::SignalsAggregationResponse response;
  return response;
}

}  // namespace

class ChromeProfileRequestGeneratorTest : public ::testing::Test {
 protected:
  ChromeProfileRequestGeneratorTest()
      : generator_(base::FilePath(kProfilePath),
                   &delegate_factory_,
                   &mock_aggregator_) {}

  test::FakeReportingDelegateFactory delegate_factory_{kBrowserExePath};
  ChromeProfileRequestGenerator generator_;
  StrictMock<device_signals::MockSignalsAggregator> mock_aggregator_;
};

TEST_F(ChromeProfileRequestGeneratorTest, GenerateFullReportNoSecuritySignals) {
  base::test::TestFuture<ReportRequestQueue> test_future;
  generator_.Generate(ReportGenerationConfig(ReportType::kProfileReport,
                                             SecuritySignalsMode::kNoSignals,
                                             /*use_cookies=*/false),
                      test_future.GetCallback());

  EXPECT_CALL(mock_aggregator_, GetSignals(_, _)).Times(0);

  VerifyReportContent(test_future.Get());
}

TEST_F(ChromeProfileRequestGeneratorTest,
       GenerateFullReportWithSecuritySignals) {
  EXPECT_CALL(mock_aggregator_, GetSignals(CreateExpectedRequest(), _))
      .WillOnce(
          Invoke([](const device_signals::SignalsAggregationRequest& request,
                    base::OnceCallback<void(
                        device_signals::SignalsAggregationResponse)> callback) {
            std::move(callback).Run(CreateFilledResponse());
          }));

  base::test::TestFuture<ReportRequestQueue> test_future;
  generator_.Generate(
      ReportGenerationConfig(ReportType::kProfileReport,
                             SecuritySignalsMode::kSignalsAttached,
                             /*use_cookies=*/false),
      test_future.GetCallback());

  // TODO(402486793): Modify and use `VerifyReportContent` to verify the report
  // content.
}

TEST_F(ChromeProfileRequestGeneratorTest, GenerateSecuritySignalsOnlyReport) {
  EXPECT_CALL(mock_aggregator_, GetSignals(CreateExpectedRequest(), _))
      .WillOnce(
          Invoke([](const device_signals::SignalsAggregationRequest& request,
                    base::OnceCallback<void(
                        device_signals::SignalsAggregationResponse)> callback) {
            std::move(callback).Run(CreateFilledResponse());
          }));
  base::test::TestFuture<ReportRequestQueue> test_future;
  generator_.Generate(ReportGenerationConfig(ReportType::kProfileReport,
                                             SecuritySignalsMode::kSignalsOnly,
                                             /*use_cookies=*/false),
                      test_future.GetCallback());
  // TODO(402486793): Modify and use `VerifyReportContent` to verify the report
  // content.
}

TEST_F(ChromeProfileRequestGeneratorTest, IncorrectReportType) {
  base::test::TestFuture<ReportRequestQueue> test_future;
  generator_.Generate(
      ReportGenerationConfig(ReportType::kFull, SecuritySignalsMode::kNoSignals,
                             /*use_cookies=*/false),
      test_future.GetCallback());

  EXPECT_CALL(mock_aggregator_, GetSignals(_, _)).Times(0);

  const ReportRequestQueue& requests = test_future.Get();

  // When the wrong report type is provided, generator should still return the
  // correct request, but with empty content.
  ASSERT_EQ(1u, requests.size());
  ReportRequest* request = requests.front().get();
  ASSERT_TRUE(request);
  ASSERT_FALSE(request->GetDeviceReportRequest().has_browser_report());
}

}  // namespace enterprise_reporting
