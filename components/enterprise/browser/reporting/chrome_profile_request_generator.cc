// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/enterprise/browser/reporting/chrome_profile_request_generator.h"

#include <utility>

#include "base/functional/callback.h"
#include "components/enterprise/browser/reporting/os_report_generator.h"
#include "components/enterprise/browser/reporting/report_type.h"
#include "components/enterprise/browser/reporting/reporting_delegate_factory.h"

namespace enterprise_reporting {

namespace em = enterprise_management;

ChromeProfileRequestGenerator::ChromeProfileRequestGenerator(
    const base::FilePath& profile_path,
    const std::string& profile_name,
    ReportingDelegateFactory* delegate_factory)
    : profile_path_(profile_path),
      profile_name_(profile_name),
      browser_report_generator_(delegate_factory),
      profile_report_generator_(delegate_factory) {
  profile_report_generator_.set_is_machine_scope(false);
}

ChromeProfileRequestGenerator::~ChromeProfileRequestGenerator() = default;

void ChromeProfileRequestGenerator::Generate(ReportCallback callback) {
  auto request = std::make_unique<ReportRequest>(ReportType::kProfileReport);
  request->GetChromeProfileReportRequest().set_allocated_os_report(
      GetOSReport().release());
  browser_report_generator_.Generate(
      ReportType::kProfileReport,
      base::BindOnce(&ChromeProfileRequestGenerator::OnBrowserReportReady,
                     weak_ptr_factory_.GetWeakPtr(), std::move(request),
                     std::move(callback)));
}

void ChromeProfileRequestGenerator::ToggleExtensionReport(
    ProfileReportGenerator::ExtensionsEnabledCallback callback) {
  profile_report_generator_.SetExtensionsEnabledCallback(std::move(callback));
}

void ChromeProfileRequestGenerator::OnBrowserReportReady(
    std::unique_ptr<ReportRequest> request,
    ReportCallback callback,
    std::unique_ptr<em::BrowserReport> browser_report) {
  auto profile_report = profile_report_generator_.MaybeGenerate(
      profile_path_, profile_name_, ReportType::kProfileReport);

  browser_report->add_chrome_user_profile_infos()->Swap(profile_report.get());

  request->GetChromeProfileReportRequest().set_allocated_browser_report(
      browser_report.release());
  ReportRequestQueue requests;
  requests.push(std::move(request));
  std::move(callback).Run(std::move(requests));
}

}  // namespace enterprise_reporting
