// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/enterprise/browser/reporting/fake_browser_report_generator_delegate.h"

#include <memory>
#include <string>
#include <vector>

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

FakeProfileReportGeneratorDelegate::~FakeProfileReportGeneratorDelegate() =
    default;

bool FakeProfileReportGeneratorDelegate::Init(const base::FilePath& path) {
  return true;
}
void FakeProfileReportGeneratorDelegate::GetSigninUserInfo(
    enterprise_management::ChromeUserProfileInfo* report) {}

void FakeProfileReportGeneratorDelegate::GetExtensionInfo(
    enterprise_management::ChromeUserProfileInfo* report) {}

void FakeProfileReportGeneratorDelegate::GetExtensionRequest(
    enterprise_management::ChromeUserProfileInfo* report) {}

std::unique_ptr<policy::PolicyConversionsClient>
FakeProfileReportGeneratorDelegate::MakePolicyConversionsClient() {
  return nullptr;
}

policy::MachineLevelUserCloudPolicyManager*
FakeProfileReportGeneratorDelegate::GetCloudPolicyManager() {
  return nullptr;
}

FakeBrowserReportGeneratorDelegate::FakeBrowserReportGeneratorDelegate(
    base::StringPiece executable_path)
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
    base::StringPiece executable_path)
    : executable_path_(executable_path) {}

FakeReportingDelegateFactory::~FakeReportingDelegateFactory() = default;

std::unique_ptr<BrowserReportGenerator::Delegate>
FakeReportingDelegateFactory::GetBrowserReportGeneratorDelegate() {
  return std::make_unique<test::FakeBrowserReportGeneratorDelegate>(
      executable_path_);
}

std::unique_ptr<ProfileReportGenerator::Delegate>
FakeReportingDelegateFactory::GetProfileReportGeneratorDelegate() {
  return std::make_unique<FakeProfileReportGeneratorDelegate>();
}

std::unique_ptr<ReportGenerator::Delegate>
FakeReportingDelegateFactory::GetReportGeneratorDelegate() {
  return nullptr;
}

std::unique_ptr<ReportScheduler::Delegate>
FakeReportingDelegateFactory::GetReportSchedulerDelegate() {
  return nullptr;
}

std::unique_ptr<RealTimeReportGenerator::Delegate>
FakeReportingDelegateFactory::GetRealTimeReportGeneratorDelegate() {
  return nullptr;
}

std::unique_ptr<RealTimeReportController::Delegate>
FakeReportingDelegateFactory::GetRealTimeReportControllerDelegate() {
  return nullptr;
}

}  // namespace enterprise_reporting::test
