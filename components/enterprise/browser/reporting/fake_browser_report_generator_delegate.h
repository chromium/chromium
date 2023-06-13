// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ENTERPRISE_BROWSER_REPORTING_FAKE_BROWSER_REPORT_GENERATOR_DELEGATE_H_
#define COMPONENTS_ENTERPRISE_BROWSER_REPORTING_FAKE_BROWSER_REPORT_GENERATOR_DELEGATE_H_

#include <memory>
#include <string>

#include "base/files/file_path.h"
#include "base/strings/string_piece.h"
#include "components/enterprise/browser/reporting/browser_report_generator.h"
#include "components/enterprise/browser/reporting/real_time_report_controller.h"
#include "components/enterprise/browser/reporting/report_util.h"
#include "components/enterprise/browser/reporting/reporting_delegate_factory.h"
#include "components/version_info/channel.h"

namespace enterprise_management {
class ChromeUserProfileInfo;
class BrowserReport;
}  // namespace enterprise_management

namespace policy {
class PolicyConversionsClient;
class MachineLevelUserCloudPolicyManager;
}  // namespace policy

namespace enterprise_reporting::test {

class FakeProfileReportGeneratorDelegate
    : public ProfileReportGenerator::Delegate {
 public:
  ~FakeProfileReportGeneratorDelegate() override;

  bool Init(const base::FilePath& path) override;

  void GetSigninUserInfo(
      enterprise_management::ChromeUserProfileInfo* report) override;

  void GetExtensionInfo(
      enterprise_management::ChromeUserProfileInfo* report) override;

  void GetExtensionRequest(
      enterprise_management::ChromeUserProfileInfo* report) override;

  std::unique_ptr<policy::PolicyConversionsClient> MakePolicyConversionsClient()
      override;

  policy::MachineLevelUserCloudPolicyManager* GetCloudPolicyManager() override;
};

class FakeBrowserReportGeneratorDelegate
    : public BrowserReportGenerator::Delegate {
 public:
  explicit FakeBrowserReportGeneratorDelegate(
      base::StringPiece executable_path);
  ~FakeBrowserReportGeneratorDelegate() override;

  std::string GetExecutablePath() override;

  version_info::Channel GetChannel() override;

  std::vector<BrowserReportGenerator::ReportedProfileData> GetReportedProfiles()
      override;

  bool IsExtendedStableChannel() override;

  void GenerateBuildStateInfo(
      enterprise_management::BrowserReport* report) override;

 private:
  const std::string executable_path_;
};

class FakeReportingDelegateFactory : public ReportingDelegateFactory {
 public:
  explicit FakeReportingDelegateFactory(base::StringPiece executable_path);

  ~FakeReportingDelegateFactory() override;

  std::unique_ptr<BrowserReportGenerator::Delegate>
  GetBrowserReportGeneratorDelegate() override;

  std::unique_ptr<ProfileReportGenerator::Delegate>
  GetProfileReportGeneratorDelegate() override;

  std::unique_ptr<ReportGenerator::Delegate> GetReportGeneratorDelegate()
      override;

  std::unique_ptr<ReportScheduler::Delegate> GetReportSchedulerDelegate()
      override;

  std::unique_ptr<RealTimeReportGenerator::Delegate>
  GetRealTimeReportGeneratorDelegate() override;

  std::unique_ptr<RealTimeReportController::Delegate>
  GetRealTimeReportControllerDelegate() override;

 private:
  const std::string executable_path_;
};

}  // namespace enterprise_reporting::test

#endif  // COMPONENTS_ENTERPRISE_BROWSER_REPORTING_FAKE_BROWSER_REPORT_GENERATOR_DELEGATE_H_
