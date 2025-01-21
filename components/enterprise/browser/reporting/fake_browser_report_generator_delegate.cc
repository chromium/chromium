// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/enterprise/browser/reporting/fake_browser_report_generator_delegate.h"

#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include "base/files/file_path.h"
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
class CloudPolicyManager;
}  // namespace policy

namespace enterprise_reporting::test {

FakeProfileReportGeneratorDelegate::~FakeProfileReportGeneratorDelegate() =
    default;

bool FakeProfileReportGeneratorDelegate::Init(const base::FilePath& path) {
  return true;
}

void FakeProfileReportGeneratorDelegate::GetSigninUserInfo(
    enterprise_management::ChromeUserProfileInfo* report) {}

void FakeProfileReportGeneratorDelegate::GetAffiliationInfo(
    enterprise_management::ChromeUserProfileInfo* report) {}

void FakeProfileReportGeneratorDelegate::GetExtensionInfo(
    enterprise_management::ChromeUserProfileInfo* report) {}

void FakeProfileReportGeneratorDelegate::GetExtensionRequest(
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
    std::string_view executable_path)
    : executable_path_(executable_path) {}

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

FakeReportingDelegateFactory::FakeReportingDelegateFactory(
    std::string_view executable_path)
    : executable_path_(executable_path) {}

FakeReportingDelegateFactory::~FakeReportingDelegateFactory() = default;

std::unique_ptr<BrowserReportGenerator::Delegate>
FakeReportingDelegateFactory::GetBrowserReportGeneratorDelegate() const {
  return std::make_unique<test::FakeBrowserReportGeneratorDelegate>(
      executable_path_);
}

std::unique_ptr<ProfileReportGenerator::Delegate>
FakeReportingDelegateFactory::GetProfileReportGeneratorDelegate() const {
  return std::make_unique<FakeProfileReportGeneratorDelegate>();
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

}  // namespace enterprise_reporting::test
