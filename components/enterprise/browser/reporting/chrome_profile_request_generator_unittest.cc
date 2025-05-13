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

namespace em = enterprise_management;

const char kFakeSignalOsName[] = "os_name_from_signals";
const char kFakeSignalOsVersion[] = "100.0.from_signals";
const char kFakeSignalDisplayName[] = "user_from_signals";
const char kFakeSignalHostname[] = "host_name_from_signals";

const char kFakeSignalMacAddr1[] = "00-11-22-33-44-55-66";
const char kFakeSignalMacAddr2[] = "AA-BB-CC-DD-EE-FF";
const char kFakeSignalMacAddr3[] = "A0-B1-C2-D3-E4-F5";

#if BUILDFLAG(IS_WIN)
const char kFakeSignalAvName[] = "AV_name_from_signals";
const char kFakeSignalAvId[] = "AV_id_from_signals";

const char kFakeFirstHotfix[] = "hotfix_1";
const char kFakeSecondHotfix[] = "hotfix_2";
#endif  // BUILDFLAG(IS_WIN)

namespace {

const base::FilePath::CharType kProfilePath[] =
    FILE_PATH_LITERAL("profile-path");
constexpr char kBrowserExePath[] = "browser-path";

void VerifyReportContent(
    const ReportRequestQueue& requests,
    em::ChromeProfileReportRequest::ReportType expected_report_type) {
  // True if a status report-exclusive field is expected to be filled correctly,
  // status reports with signals also count.
  bool expect_status_report_only_value =
      expected_report_type !=
      em::ChromeProfileReportRequest::PROFILE_SECURITY_SIGNALS;
  bool expect_signals_override_value =
      expected_report_type != em::ChromeProfileReportRequest::PROFILE_REPORT;

  ASSERT_EQ(1u, requests.size());
  ReportRequest* request = requests.front().get();
  ASSERT_TRUE(request);
  EXPECT_EQ(request->GetChromeProfileReportRequest().report_type(),
            expected_report_type);

  EXPECT_EQ(
      request->GetChromeProfileReportRequest().has_browser_device_identifier(),
      expect_signals_override_value);

  if (expect_signals_override_value) {
    auto browser_device_identifier =
        request->GetChromeProfileReportRequest().browser_device_identifier();
    EXPECT_EQ(browser_device_identifier.computer_name(),
              kFakeSignalDisplayName);
    // Test the case when a field value cannot be collected
    EXPECT_EQ(browser_device_identifier.serial_number(), std::string());
    EXPECT_EQ(browser_device_identifier.host_name(), kFakeSignalHostname);
  }

  ASSERT_TRUE(request->GetChromeProfileReportRequest().has_os_report());
  auto os_report = request->GetChromeProfileReportRequest().os_report();
  if (expect_signals_override_value) {
    EXPECT_EQ(os_report.name(), kFakeSignalOsName);
    // If a field does not get overwritten by value collected from device
    // signals, it should use the default OS report provided value.
    EXPECT_EQ(os_report.arch(), policy::GetOSArchitecture());
    EXPECT_EQ(os_report.version(), kFakeSignalOsVersion);
    EXPECT_EQ(os_report.screen_lock_secured(), em::SettingValue::ENABLED);

    EXPECT_EQ(3, os_report.mac_addresses_size());
    EXPECT_EQ(os_report.mac_addresses(0), kFakeSignalMacAddr1);
    EXPECT_EQ(os_report.mac_addresses(1), kFakeSignalMacAddr2);
    EXPECT_EQ(os_report.mac_addresses(2), kFakeSignalMacAddr3);
#if BUILDFLAG(IS_WIN)
    auto av_info = os_report.antivirus_info(0);
    EXPECT_EQ(av_info.display_name(), kFakeSignalAvName);
    EXPECT_EQ(av_info.product_id(), kFakeSignalAvId);
    EXPECT_EQ(av_info.state(), em::AntiVirusProduct::EXPIRED);

    auto first_hotfix = os_report.hotfixes(0);
    EXPECT_EQ(first_hotfix, kFakeFirstHotfix);
    auto second_hotfix = os_report.hotfixes(1);
    EXPECT_EQ(second_hotfix, kFakeSecondHotfix);
#endif  // BUILDFLAG(IS_WIN)
  } else {
    EXPECT_EQ(os_report.name(), policy::GetOSPlatform());
    EXPECT_EQ(os_report.arch(), policy::GetOSArchitecture());
    EXPECT_EQ(os_report.version(), policy::GetOSVersion());

    // Signals report only fields should not be written
    ASSERT_FALSE(os_report.has_device_enrollment_domain());
    ASSERT_FALSE(os_report.has_screen_lock_secured());

    EXPECT_EQ(0, os_report.mac_addresses_size());
#if BUILDFLAG(IS_WIN)
    EXPECT_EQ(0, os_report.antivirus_info_size());
    EXPECT_EQ(0, os_report.hotfixes_size());
#endif  // BUILDFLAG(IS_WIN)
  }

  ASSERT_TRUE(request->GetChromeProfileReportRequest().has_browser_report());
  auto browser_report =
      request->GetChromeProfileReportRequest().browser_report();

  // These fields are only filled if status report is enabled.
  EXPECT_EQ(
      ObfuscateFilePath(kBrowserExePath) == browser_report.executable_path(),
      expect_status_report_only_value);
  EXPECT_EQ(browser_report.is_extended_stable_channel(),
            expect_status_report_only_value);

  EXPECT_EQ(1, browser_report.chrome_user_profile_infos_size());
  auto chrome_user_profile_info = browser_report.chrome_user_profile_infos(0);

  // These fields are only filled if status report is enabled.
  EXPECT_EQ(ObfuscateFilePath(base::FilePath(kProfilePath).AsUTF8Unsafe()) ==
                chrome_user_profile_info.id(),
            expect_status_report_only_value);
  EXPECT_EQ(chrome_user_profile_info.is_detail_available(),
            expect_status_report_only_value);

  // `profile_signals_report` is a signals report only sub-proto.
  EXPECT_EQ(chrome_user_profile_info.has_profile_signals_report(),
            expect_signals_override_value);

  if (expect_signals_override_value) {
    auto profile_signals_report =
        chrome_user_profile_info.profile_signals_report();

    EXPECT_EQ(profile_signals_report.built_in_dns_client_enabled(), true);
    EXPECT_EQ(profile_signals_report.chrome_remote_desktop_app_blocked(),
              false);
    EXPECT_EQ(profile_signals_report.password_protection_warning_trigger(),
              em::ProfileSignalsReport::PHISHING_REUSE);
    // If a value cannot be collected, this report field will be empty.
    EXPECT_EQ(profile_signals_report.profile_enrollment_domain(),
              std::string());
    EXPECT_EQ(profile_signals_report.safe_browsing_protection_level(),
              em::ProfileSignalsReport::STANDARD_PROTECTION);
    EXPECT_EQ(profile_signals_report.site_isolation_enabled(), true);
    EXPECT_EQ(profile_signals_report.realtime_url_check_mode(),
              em::ProfileSignalsReport::ENABLED_MAIN_FRAME);
  }
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
  device_signals::SignalsAggregationResponse response;

  device_signals::OsSignalsResponse os_signals;
  os_signals.operating_system = kFakeSignalOsName;
  os_signals.os_version = kFakeSignalOsVersion;
  os_signals.display_name = kFakeSignalDisplayName;
  os_signals.hostname = kFakeSignalHostname;
  os_signals.screen_lock_secured = device_signals::SettingValue::ENABLED;
  // Test vector field
  os_signals.mac_addresses = {kFakeSignalMacAddr1, kFakeSignalMacAddr2,
                              kFakeSignalMacAddr3};

  response.os_signals_response = os_signals;

#if BUILDFLAG(IS_WIN)
  device_signals::AvProduct av_product;
  av_product.display_name = kFakeSignalAvName;
  av_product.product_id = kFakeSignalAvId;
  av_product.state = device_signals::AvProductState::kExpired;
  device_signals::AntiVirusSignalResponse av_response;
  av_response.av_products.push_back(av_product);
  response.av_signal_response = av_response;

  device_signals::HotfixSignalResponse hotfix_response;
  hotfix_response.hotfixes.push_back({kFakeFirstHotfix});
  hotfix_response.hotfixes.push_back({kFakeSecondHotfix});
  response.hotfix_signal_response = hotfix_response;
#endif  // BUILDFLAG(IS_WIN)

  device_signals::ProfileSignalsResponse profile_signals;
  profile_signals.built_in_dns_client_enabled = true;
  profile_signals.chrome_remote_desktop_app_blocked = false;
  profile_signals.password_protection_warning_trigger =
      safe_browsing::PasswordProtectionTrigger::PHISHING_REUSE;
  // Test that there is no issue if domain can't be collected.
  profile_signals.profile_enrollment_domain = std::nullopt;
  profile_signals.safe_browsing_protection_level =
      safe_browsing::SafeBrowsingState::STANDARD_PROTECTION;
  profile_signals.site_isolation_enabled = true;
  profile_signals.realtime_url_check_mode = enterprise_connectors::
      EnterpriseRealTimeUrlCheckMode::REAL_TIME_CHECK_FOR_MAINFRAME_ENABLED;

  response.profile_signals_response = profile_signals;

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
  EXPECT_CALL(mock_aggregator_, GetSignals(_, _)).Times(0);
  base::test::TestFuture<ReportRequestQueue> test_future;
  generator_.Generate(ReportGenerationConfig(ReportTrigger::kTriggerTimer,
                                             ReportType::kProfileReport,
                                             SecuritySignalsMode::kNoSignals,
                                             /*use_cookies=*/false),
                      test_future.GetCallback());

  VerifyReportContent(test_future.Get(),
                      em::ChromeProfileReportRequest::PROFILE_REPORT);
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
      ReportGenerationConfig(ReportTrigger::kTriggerTimer,
                             ReportType::kProfileReport,
                             SecuritySignalsMode::kSignalsAttached,
                             /*use_cookies=*/false),
      test_future.GetCallback());

  VerifyReportContent(
      test_future.Get(),
      em::ChromeProfileReportRequest::PROFILE_REPORT_WITH_SECURITY_SIGNALS);
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
  generator_.Generate(ReportGenerationConfig(ReportTrigger::kTriggerNone,
                                             ReportType::kProfileReport,
                                             SecuritySignalsMode::kSignalsOnly,
                                             /*use_cookies=*/false),
                      test_future.GetCallback());
  VerifyReportContent(test_future.Get(),
                      em::ChromeProfileReportRequest::PROFILE_SECURITY_SIGNALS);
}

TEST_F(ChromeProfileRequestGeneratorTest, IncorrectReportType) {
  EXPECT_CALL(mock_aggregator_, GetSignals(_, _)).Times(0);
  base::test::TestFuture<ReportRequestQueue> test_future;
  generator_.Generate(ReportGenerationConfig(), test_future.GetCallback());

  const ReportRequestQueue& requests = test_future.Get();

  // When the wrong report type is provided, generator should still return the
  // correct request, but with empty content.
  ASSERT_EQ(1u, requests.size());
  ReportRequest* request = requests.front().get();
  ASSERT_TRUE(request);
  ASSERT_FALSE(request->GetDeviceReportRequest().has_browser_report());
}

}  // namespace enterprise_reporting
