// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ENTERPRISE_BROWSER_REPORTING_FAKE_BROWSER_REPORT_GENERATOR_DELEGATE_H_
#define COMPONENTS_ENTERPRISE_BROWSER_REPORTING_FAKE_BROWSER_REPORT_GENERATOR_DELEGATE_H_

#include <memory>
#include <optional>
#include <string>
#include <string_view>

#include "base/files/file_path.h"
#include "components/enterprise/browser/reporting/browser_report_generator.h"
#include "components/enterprise/browser/reporting/real_time_report_controller.h"
#include "components/enterprise/browser/reporting/report_util.h"
#include "components/enterprise/browser/reporting/reporting_delegate_factory.h"
#include "components/policy/proto/device_management_backend.pb.h"
#include "components/version_info/channel.h"

namespace policy {
class PolicyConversionsClient;
class CloudPolicyManager;
}  // namespace policy

namespace enterprise_reporting::test {

class FakeProfileReportGeneratorDelegate
    : public ProfileReportGenerator::Delegate {
 public:
  ~FakeProfileReportGeneratorDelegate() override;

  bool Init(const base::FilePath& path) override;

  void GetSigninUserInfo(
      enterprise_management::ChromeUserProfileInfo* report) override;

  void GetAffiliationInfo(
      enterprise_management::ChromeUserProfileInfo* report) override;

  void GetExtensionInfo(
      enterprise_management::ChromeUserProfileInfo* report) override;

  void GetExtensionRequest(
      enterprise_management::ChromeUserProfileInfo* report) override;

  void GetProfileId(
      enterprise_management::ChromeUserProfileInfo* report) override;

  void GetProfileName(
      enterprise_management::ChromeUserProfileInfo* report) override;

  std::unique_ptr<policy::PolicyConversionsClient> MakePolicyConversionsClient(
      bool is_machine_scope) override;

  policy::CloudPolicyManager* GetCloudPolicyManager(
      bool is_machine_scope) override;

  void SetInitResult(bool result);

 private:
  bool init_result_ = false;
};

class FakeBrowserReportGeneratorDelegate
    : public BrowserReportGenerator::Delegate {
 public:
  explicit FakeBrowserReportGeneratorDelegate(
      std::string_view executable_path,
      std::optional<enterprise_management::AppBoundEncryptionState>
          app_bound_encryption_state = std::nullopt);
  ~FakeBrowserReportGeneratorDelegate() override;

  std::string GetExecutablePath() override;

  version_info::Channel GetChannel() override;

  std::vector<BrowserReportGenerator::ReportedProfileData> GetReportedProfiles()
      override;

  bool IsExtendedStableChannel() override;

  void GenerateBuildStateInfo(
      enterprise_management::BrowserReport* report) override;

  void GenerateAppBoundEncryptionInfo(
      std::unique_ptr<enterprise_management::BrowserReport> report,
      BrowserReportGenerator::ReportCallback callback) override;

 private:
  const std::string executable_path_;
  const std::optional<enterprise_management::AppBoundEncryptionState>
      app_bound_encryption_state_;
};

class FakeReportingDelegateFactory : public ReportingDelegateFactory {
 public:
  explicit FakeReportingDelegateFactory(std::string_view executable_path);

  ~FakeReportingDelegateFactory() override;

  std::unique_ptr<BrowserReportGenerator::Delegate>
  GetBrowserReportGeneratorDelegate() const override;

  std::unique_ptr<ProfileReportGenerator::Delegate>
  GetProfileReportGeneratorDelegate() const override;

  std::unique_ptr<ReportGenerator::Delegate> GetReportGeneratorDelegate()
      const override;

  std::unique_ptr<ReportScheduler::Delegate> GetReportSchedulerDelegate()
      const override;

  std::unique_ptr<RealTimeReportGenerator::Delegate>
  GetRealTimeReportGeneratorDelegate() const override;

  std::unique_ptr<RealTimeReportController::Delegate>
  GetRealTimeReportControllerDelegate() const override;

  void SetProfileInitResult(bool result);

  void SetAppBoundEncryptionState(
      std::optional<enterprise_management::AppBoundEncryptionState> state);

 private:
  const std::string executable_path_;
  bool profile_init_result_ = false;
  std::optional<enterprise_management::AppBoundEncryptionState>
      app_bound_encryption_state_;
};

}  // namespace enterprise_reporting::test

#endif  // COMPONENTS_ENTERPRISE_BROWSER_REPORTING_FAKE_BROWSER_REPORT_GENERATOR_DELEGATE_H_
