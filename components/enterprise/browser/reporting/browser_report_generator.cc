// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/enterprise/browser/reporting/browser_report_generator.h"

#include <utility>

#include "base/notreached.h"
#include "base/version.h"
#include "build/chromeos_buildflags.h"
#include "components/enterprise/browser/reporting/report_util.h"
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
  GenerateBasicInfo(report.get(), report_type);

  if (report_type != ReportType::kProfileReport) {
    GenerateProfileInfo(report.get());
  }
  std::move(callback).Run(std::move(report));
}

void BrowserReportGenerator::GenerateProfileInfo(em::BrowserReport* report) {
  for (auto entry : delegate_->GetReportedProfiles()) {
    em::ChromeUserProfileInfo* profile =
        report->add_chrome_user_profile_infos();
    profile->set_id(entry.id);
    profile->set_name(entry.name);
    profile->set_is_detail_available(false);
  }
}

void BrowserReportGenerator::GenerateBasicInfo(em::BrowserReport* report,
                                               ReportType report_type) {
  // Chrome OS user session report doesn't include version and channel
  // information.
#if BUILDFLAG(IS_CHROMEOS_ASH)
  bool contains_version_and_channel = report_type == ReportType::kProfileReport;
#else
  bool contains_version_and_channel = true;
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

  if (contains_version_and_channel) {
    report->set_browser_version(std::string(version_info::GetVersionNumber()));
    report->set_channel(policy::ConvertToProtoChannel(delegate_->GetChannel()));
    if (delegate_->IsExtendedStableChannel())
      report->set_is_extended_stable_channel(true);
    delegate_->GenerateBuildStateInfo(report);
  }

  switch (report_type) {
    case ReportType::kFull:
      report->set_executable_path(delegate_->GetExecutablePath());
      break;
    case ReportType::kProfileReport:
      report->set_executable_path(
          ObfuscateFilePath(delegate_->GetExecutablePath()));
      break;
    case ReportType::kBrowserVersion:
      NOTREACHED_IN_MIGRATION();
      break;
  }
}

}  // namespace enterprise_reporting
