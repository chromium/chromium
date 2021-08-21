// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/enterprise/browser/reporting/browser_report_generator.h"

#include <utility>

#include "base/version.h"
#include "build/chromeos_buildflags.h"
#include "components/enterprise/browser/reporting/reporting_delegate_factory.h"
#include "components/policy/core/common/cloud/cloud_policy_util.h"
#include "components/policy/proto/device_management_backend.pb.h"
#include "components/version_info/version_info.h"

namespace em = ::enterprise_management;

namespace enterprise_reporting {

BrowserReportGenerator::BrowserReportGenerator(
    ReportingDelegateFactory* delegate_factory)
    : delegate_(delegate_factory->GetBrowserReportGeneratorDelegate()) {}

BrowserReportGenerator::~BrowserReportGenerator() = default;

void BrowserReportGenerator::Generate(ReportType report_type,
                                      ReportCallback callback) {
  auto report = std::make_unique<em::BrowserReport>();
  GenerateProfileInfo(report_type, report.get());
  delegate_->OnProfileInfoGenerated(report_type);

  if (report_type == ReportType::kExtensionRequest) {
    report->set_executable_path(delegate_->GetExecutablePath());
    std::move(callback).Run(std::move(report));
    return;
  }
  GenerateBasicInfo(report.get());

  // std::move is required here because the function completes the report
  // asynchronously.
  delegate_->GeneratePluginsIfNeeded(std::move(callback), std::move(report));
}

void BrowserReportGenerator::GenerateProfileInfo(ReportType report_type,
                                                 em::BrowserReport* report) {
  for (auto entry : delegate_->GetReportedProfiles(report_type)) {
    em::ChromeUserProfileInfo* profile =
        report->add_chrome_user_profile_infos();
    profile->set_id(entry.id);
    profile->set_name(entry.name);
    profile->set_is_detail_available(false);
  }
}

void BrowserReportGenerator::GenerateBasicInfo(em::BrowserReport* report) {
#if !BUILDFLAG(IS_CHROMEOS_ASH)
  report->set_browser_version(version_info::GetVersionNumber());
  report->set_channel(policy::ConvertToProtoChannel(delegate_->GetChannel()));
  if (delegate_->IsExtendedStableChannel())
    report->set_is_extended_stable_channel(true);
  delegate_->GenerateBuildStateInfo(report);
#endif

  report->set_executable_path(delegate_->GetExecutablePath());
}

}  // namespace enterprise_reporting
