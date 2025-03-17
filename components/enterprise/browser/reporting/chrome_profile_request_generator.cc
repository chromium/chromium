// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/enterprise/browser/reporting/chrome_profile_request_generator.h"

#include <utility>
#include <variant>

#include "base/barrier_callback.h"
#include "base/check.h"
#include "base/functional/callback.h"
#include "components/enterprise/browser/reporting/os_report_generator.h"
#include "components/enterprise/browser/reporting/report_type.h"
#include "components/enterprise/browser/reporting/reporting_delegate_factory.h"

namespace enterprise_reporting {

namespace em = enterprise_management;

namespace {

void ParseReports(
    base::OnceCallback<void(std::unique_ptr<em::BrowserReport>,
                            std::unique_ptr<em::ChromeUserProfileInfo>)>
        callback,
    std::vector<std::variant<std::unique_ptr<em::BrowserReport>,
                             std::unique_ptr<em::ChromeUserProfileInfo>>>
        reports) {
  std::unique_ptr<em::BrowserReport> browser_report;
  std::unique_ptr<enterprise_management::ChromeUserProfileInfo> profile_report;
  for (auto& variant : reports) {
    if (std::holds_alternative<std::unique_ptr<em::BrowserReport>>(variant)) {
      browser_report =
          std::move(std::get<std::unique_ptr<em::BrowserReport>>(variant));
    } else {
      profile_report = std::move(
          std::get<std::unique_ptr<em::ChromeUserProfileInfo>>(variant));
    }
  }

  std::move(callback).Run(std::move(browser_report), std::move(profile_report));
}

}  // namespace

ChromeProfileRequestGenerator::ChromeProfileRequestGenerator(
    const base::FilePath& profile_path,
    ReportingDelegateFactory* delegate_factory)
    : profile_path_(profile_path),
      browser_report_generator_(delegate_factory),
      profile_report_generator_(delegate_factory) {
  profile_report_generator_.set_is_machine_scope(false);
}

ChromeProfileRequestGenerator::~ChromeProfileRequestGenerator() = default;

void ChromeProfileRequestGenerator::Generate(ReportCallback callback) {
  auto request = std::make_unique<ReportRequest>(ReportType::kProfileReport);
  request->GetChromeProfileReportRequest().set_allocated_os_report(
      GetOSReport().release());

  auto barrier_callback = base::BarrierCallback<
      std::variant<std::unique_ptr<em::BrowserReport>,
                   std::unique_ptr<em::ChromeUserProfileInfo>>>(
      2, base::BindOnce(
             ParseReports,
             base::BindOnce(&ChromeProfileRequestGenerator::OnReportsReady,
                            weak_ptr_factory_.GetWeakPtr(), std::move(request),
                            std::move(callback))));

  browser_report_generator_.Generate(
      ReportType::kProfileReport,
      base::BindOnce(
          [](std::unique_ptr<em::BrowserReport> browser_report)
              -> std::variant<std::unique_ptr<em::BrowserReport>,
                              std::unique_ptr<em::ChromeUserProfileInfo>> {
            return std::move(browser_report);
          })
          .Then(barrier_callback));

  profile_report_generator_.MaybeGenerate(
      profile_path_, ReportType::kProfileReport,
      base::BindOnce(
          [](std::unique_ptr<em::ChromeUserProfileInfo> profile_report)
              -> std::variant<std::unique_ptr<em::BrowserReport>,
                              std::unique_ptr<em::ChromeUserProfileInfo>> {
            return std::move(profile_report);
          })
          .Then(barrier_callback));
}

void ChromeProfileRequestGenerator::ToggleExtensionReport(
    ProfileReportGenerator::ExtensionsEnabledCallback callback) {
  profile_report_generator_.SetExtensionsEnabledCallback(std::move(callback));
}

void ChromeProfileRequestGenerator::OnReportsReady(
    std::unique_ptr<ReportRequest> request,
    ReportCallback callback,
    std::unique_ptr<em::BrowserReport> browser_report,
    std::unique_ptr<em::ChromeUserProfileInfo> profile_report) {
  browser_report->add_chrome_user_profile_infos()->Swap(profile_report.get());

  request->GetChromeProfileReportRequest().set_allocated_browser_report(
      browser_report.release());
  ReportRequestQueue requests;
  requests.push(std::move(request));
  std::move(callback).Run(std::move(requests));
}

}  // namespace enterprise_reporting
