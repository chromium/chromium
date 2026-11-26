// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/enterprise/browser/reporting/fake_browser_report_generator_delegate.h"

#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "base/files/file_path.h"
#include "base/functional/callback.h"
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

FakeProfileReportGeneratorDelegate::~FakeProfileReportGeneratorDelegate() =
    default;

bool FakeProfileReportGeneratorDelegate::Init(const base::FilePath& path) {
  return init_result_;
}

void FakeProfileReportGeneratorDelegate::SetInitResult(bool result) {
  init_result_ = result;
}

void FakeProfileReportGeneratorDelegate::GetSigninUserInfo(
    enterprise_management::ChromeUserProfileInfo* report) {}

void FakeProfileReportGeneratorDelegate::GetAffiliationInfo(
    enterprise_management::ChromeUserProfileInfo* report) {
  report->mutable_affiliation()->set_is_affiliated(true);
}

void FakeProfileReportGeneratorDelegate::GetExtensionInfo(
    enterprise_management::ChromeUserProfileInfo* report) {}

void FakeProfileReportGeneratorDelegate::GetExtensionRequest(
    enterprise_management::ChromeUserProfileInfo* report) {}

void FakeProfileReportGeneratorDelegate::GetProfileId(
    enterprise_management::ChromeUserProfileInfo* report) {}

void FakeProfileReportGeneratorDelegate::GetProfileName(
    enterprise_management::ChromeUserProfileInfo* report) {}

std::unique_ptr<policy::PolicyConversionsClient>
FakeProfileReportGeneratorDelegate::MakePolicyConversionsClient(
    bool is_machine_scope) {
  return nullptr;
}

policy::CloudPolicyManager*
FakeProfileReportGeneratorDelegate::GetCloudPolicyManager(
    bool is_machine_scope) {
  return nullptr;
}

FakeBrowserReportGeneratorDelegate::FakeBrowserReportGeneratorDelegate(
    std::string_view executable_path,
    std::optional<enterprise_management::AppBoundEncryptionState>
        app_bound_encryption_state)
    : executable_path_(executable_path),
      app_bound_encryption_state_(app_bound_encryption_state) {}

FakeBrowserReportGeneratorDelegate::~FakeBrowserReportGeneratorDelegate() =
    default;

std::string FakeBrowserReportGeneratorDelegate::GetExecutablePath() {
  return executable_path_;
}

version_info::Channel FakeBrowserReportGeneratorDelegate::GetChannel() {
  return version_info::Channel::STABLE;
}

std::vector<BrowserReportGenerator::ReportedProfileData>
FakeBrowserReportGeneratorDelegate::GetReportedProfiles() {
  return std::vector<BrowserReportGenerator::ReportedProfileData>();
}

bool FakeBrowserReportGeneratorDelegate::IsExtendedStableChannel() {
  return true;
}

void FakeBrowserReportGeneratorDelegate::GenerateBuildStateInfo(
    enterprise_management::BrowserReport* report) {
  return;
}

void FakeBrowserReportGeneratorDelegate::GenerateAppBoundEncryptionInfo(
    std::unique_ptr<enterprise_management::BrowserReport> report,
    BrowserReportGenerator::ReportCallback callback) {
  if (app_bound_encryption_state_.has_value()) {
    report->set_app_bound_encryption_state(*app_bound_encryption_state_);
  }
  BrowserReportGenerator::Delegate::GenerateAppBoundEncryptionInfo(
      std::move(report), std::move(callback));
}

FakeReportingDelegateFactory::FakeReportingDelegateFactory(
    std::string_view executable_path)
    : executable_path_(executable_path), profile_init_result_(true) {}

FakeReportingDelegateFactory::~FakeReportingDelegateFactory() = default;

std::unique_ptr<BrowserReportGenerator::Delegate>
FakeReportingDelegateFactory::GetBrowserReportGeneratorDelegate() const {
  return std::make_unique<test::FakeBrowserReportGeneratorDelegate>(
      executable_path_, app_bound_encryption_state_);
}

std::unique_ptr<ProfileReportGenerator::Delegate>
FakeReportingDelegateFactory::GetProfileReportGeneratorDelegate() const {
  auto delegate = std::make_unique<FakeProfileReportGeneratorDelegate>();
  delegate->SetInitResult(profile_init_result_);
  return delegate;
}

std::unique_ptr<ReportGenerator::Delegate>
FakeReportingDelegateFactory::GetReportGeneratorDelegate() const {
  return nullptr;
}

std::unique_ptr<ReportScheduler::Delegate>
FakeReportingDelegateFactory::GetReportSchedulerDelegate() const {
  return nullptr;
}

std::unique_ptr<RealTimeReportGenerator::Delegate>
FakeReportingDelegateFactory::GetRealTimeReportGeneratorDelegate() const {
  return nullptr;
}

std::unique_ptr<RealTimeReportController::Delegate>
FakeReportingDelegateFactory::GetRealTimeReportControllerDelegate() const {
  return nullptr;
}

void FakeReportingDelegateFactory::SetProfileInitResult(bool result) {
  profile_init_result_ = result;
}

void FakeReportingDelegateFactory::SetAppBoundEncryptionState(
    std::optional<enterprise_management::AppBoundEncryptionState> state) {
  app_bound_encryption_state_ = state;
}

}  // namespace enterprise_reporting::test
