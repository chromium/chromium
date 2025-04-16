// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/enterprise/browser/reporting/chrome_profile_request_generator.h"

#include <vector>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/test/test_future.h"
#include "components/enterprise/browser/reporting/fake_browser_report_generator_delegate.h"
#include "components/enterprise/browser/reporting/report_generation_config.h"
#include "components/enterprise/browser/reporting/report_util.h"
#include "components/enterprise/browser/reporting/reporting_delegate_factory.h"
#include "components/policy/core/common/cloud/cloud_policy_util.h"
#include "components/policy/proto/device_management_backend.pb.h"
#include "components/version_info/channel.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace enterprise_reporting {

namespace em = enterprise_reporting;

namespace {

const base::FilePath::CharType kProfilePath[] =
    FILE_PATH_LITERAL("profile-path");
constexpr char kBrowserExePath[] = "browser-path";

void VerifyFullReportNoSecuritySignals(const ReportRequestQueue& requests) {
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

}  // namespace

class ChromeProfileRequestGeneratorTest : public ::testing::Test {
 protected:
  ChromeProfileRequestGeneratorTest()
      : generator_(base::FilePath(kProfilePath),
                   &delegate_factory_) {}

  test::FakeReportingDelegateFactory delegate_factory_{kBrowserExePath};
  ChromeProfileRequestGenerator generator_;
};

TEST_F(ChromeProfileRequestGeneratorTest, GenerateFullReportNoSecuritySignals) {
  base::test::TestFuture<ReportRequestQueue> test_future;
  generator_.Generate(ReportGenerationConfig(ReportType::kProfileReport,
                                             SecuritySignalsMode::kNoSignals,
                                             /*use_cookies=*/false),
                      test_future.GetCallback());

  VerifyFullReportNoSecuritySignals(test_future.Get());
}

}  // namespace enterprise_reporting
