// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/enterprise/browser/reporting/chrome_profile_request_generator.h"

#include <vector>

#include "base/bind.h"
#include "base/callback.h"
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
constexpr char kProfileName[] = "profile-name";
constexpr char kBrowserExePath[] = "browser-path";

class FakeBrowserReportGeneratorDelegate
    : public BrowserReportGenerator::Delegate {
 public:
  ~FakeBrowserReportGeneratorDelegate() override = default;

  std::string GetExecutablePath() override { return kBrowserExePath; }
  version_info::Channel GetChannel() override {
    return version_info::Channel::STABLE;
  }
  std::vector<BrowserReportGenerator::ReportedProfileData> GetReportedProfiles()
      override {
    return std::vector<BrowserReportGenerator::ReportedProfileData>();
  }
  bool IsExtendedStableChannel() override { return true; }
  void GenerateBuildStateInfo(
      enterprise_management::BrowserReport* report) override {
    return;
  }
  void GeneratePluginsIfNeeded(
      BrowserReportGenerator::ReportCallback callback,
      std::unique_ptr<enterprise_management::BrowserReport> report) override {
    std::move(callback).Run(std::move(report));
  }
};

class FakeProfileReportGeneratorDelegate
    : public ProfileReportGenerator::Delegate {
 public:
  ~FakeProfileReportGeneratorDelegate() override = default;

  bool Init(const base::FilePath& path) override { return true; }
  void GetSigninUserInfo(
      enterprise_management::ChromeUserProfileInfo* report) override {}
  void GetExtensionInfo(
      enterprise_management::ChromeUserProfileInfo* report) override {}
  void GetExtensionRequest(
      enterprise_management::ChromeUserProfileInfo* report) override {}

  std::unique_ptr<policy::PolicyConversionsClient> MakePolicyConversionsClient()
      override {
    return nullptr;
  }
  policy::MachineLevelUserCloudPolicyManager* GetCloudPolicyManager() override {
    return nullptr;
  }
};

class FakeReportingDelegateFactory : public ReportingDelegateFactory {
 public:
  ~FakeReportingDelegateFactory() override = default;

  std::unique_ptr<BrowserReportGenerator::Delegate>
  GetBrowserReportGeneratorDelegate() override {
    return std::make_unique<FakeBrowserReportGeneratorDelegate>();
  }

  std::unique_ptr<ProfileReportGenerator::Delegate>
  GetProfileReportGeneratorDelegate() override {
    return std::make_unique<FakeProfileReportGeneratorDelegate>();
  }

  std::unique_ptr<ReportGenerator::Delegate> GetReportGeneratorDelegate()
      override {
    return nullptr;
  }

  std::unique_ptr<ReportScheduler::Delegate> GetReportSchedulerDelegate()
      override {
    return nullptr;
  }

  std::unique_ptr<RealTimeReportGenerator::Delegate>
  GetRealTimeReportGeneratorDelegate() override {
    return nullptr;
  }
};

}  // namespace

class ChromeProfileRequestGeneratorTest : public ::testing::Test {
 public:
  ChromeProfileRequestGeneratorTest()
      : generator_(base::FilePath(kProfilePath),
                   kProfileName,
                   &delegate_factory_) {}
  void Verify(ReportRequestQueue requests) {
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
    EXPECT_EQ(kProfileName, profile_report.name());
    EXPECT_TRUE(profile_report.is_detail_available());
  }
  void GenerateAndVerify() {
    generator_.Generate(base::BindOnce(
        &ChromeProfileRequestGeneratorTest::Verify, base::Unretained(this)));
  }

 private:
  FakeReportingDelegateFactory delegate_factory_;
  ChromeProfileRequestGenerator generator_;
};

TEST_F(ChromeProfileRequestGeneratorTest, Generate) {
  GenerateAndVerify();
}

}  // namespace enterprise_reporting
