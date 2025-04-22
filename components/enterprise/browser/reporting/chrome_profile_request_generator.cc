// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/enterprise/browser/reporting/chrome_profile_request_generator.h"

#include <utility>
#include <variant>

#include "base/barrier_callback.h"
#include "base/check.h"
#include "base/functional/callback.h"
#include "build/build_config.h"
#include "components/device_signals/core/browser/signals_aggregator.h"
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
    ReportingDelegateFactory* delegate_factory,
    device_signals::SignalsAggregator* signals_aggregator)
    : profile_path_(profile_path),
      browser_report_generator_(delegate_factory),
      profile_report_generator_(delegate_factory),
      signals_aggregator_(signals_aggregator) {
  profile_report_generator_.set_is_machine_scope(false);
}

ChromeProfileRequestGenerator::~ChromeProfileRequestGenerator() = default;

void ChromeProfileRequestGenerator::Generate(
    ReportGenerationConfig generation_config,
    ReportCallback callback) {
  if (generation_config.report_type != ReportType::kProfileReport) {
    auto empty_request =
        std::make_unique<ReportRequest>(generation_config.report_type);
    return OnRequestReady(std::move(empty_request), std::move(callback));
  }

  auto request = std::make_unique<ReportRequest>(ReportType::kProfileReport);
  if (generation_config.security_signals_mode ==
      SecuritySignalsMode::kSignalsOnly) {
    return OnBaseReportsReady(std::move(request), std::move(callback),
                              generation_config,
                              std::make_unique<em::BrowserReport>(),
                              std::make_unique<em::ChromeUserProfileInfo>());
  }

  request->GetChromeProfileReportRequest().set_allocated_os_report(
      GetOSReport().release());

  auto barrier_callback = base::BarrierCallback<
      std::variant<std::unique_ptr<em::BrowserReport>,
                   std::unique_ptr<em::ChromeUserProfileInfo>>>(
      2, base::BindOnce(
             ParseReports,
             base::BindOnce(&ChromeProfileRequestGenerator::OnBaseReportsReady,
                            weak_ptr_factory_.GetWeakPtr(), std::move(request),
                            std::move(callback), generation_config)));

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

void ChromeProfileRequestGenerator::OnBaseReportsReady(
    std::unique_ptr<ReportRequest> request,
    ReportCallback callback,
    ReportGenerationConfig generation_config,
    std::unique_ptr<em::BrowserReport> browser_report,
    std::unique_ptr<em::ChromeUserProfileInfo> profile_report) {
  if (generation_config.security_signals_mode ==
          SecuritySignalsMode::kNoSignals ||
      !signals_aggregator_) {
    browser_report->add_chrome_user_profile_infos()->Swap(profile_report.get());

    request->GetChromeProfileReportRequest().set_allocated_browser_report(
        browser_report.release());

    return OnRequestReady(std::move(request), std::move(callback));
  }

  // Start signals collection process.
  device_signals::SignalsAggregationRequest signals_request;
  signals_request.signal_names.emplace(device_signals::SignalName::kOsSignals);
  signals_request.signal_names.emplace(
      device_signals::SignalName::kBrowserContextSignals);
#if BUILDFLAG(IS_WIN)
  signals_request.signal_names.emplace(device_signals::SignalName::kAntiVirus);
  signals_request.signal_names.emplace(device_signals::SignalName::kHotfixes);
#endif  // BUILDFLAG(IS_WIN)
  signals_request.trigger = device_signals::Trigger::kSignalsReport;

  signals_aggregator_->GetSignals(
      signals_request,
      base::BindOnce(
          &ChromeProfileRequestGenerator::OnAggregatedSignalsReceived,
          weak_ptr_factory_.GetWeakPtr(), std::move(request),
          std::move(callback), std::move(browser_report),
          std::move(profile_report)));
}

void ChromeProfileRequestGenerator::OnAggregatedSignalsReceived(
    std::unique_ptr<ReportRequest> request,
    ReportCallback callback,
    std::unique_ptr<em::BrowserReport> browser_report,
    std::unique_ptr<em::ChromeUserProfileInfo> profile_report,
    device_signals::SignalsAggregationResponse response) {
  // TODO(402486793): Parse signals from `response` and add them to the reports
  // in `request`.
  auto device_identifier = std::make_unique<em::BrowserDeviceIdentifier>();
  request->GetChromeProfileReportRequest()
      .set_allocated_browser_device_identifier(device_identifier.release());

  if (!request->GetChromeProfileReportRequest().has_os_report()) {
    auto os_report = std::make_unique<em::OSReport>();
    request->GetChromeProfileReportRequest().set_allocated_os_report(
        os_report.release());
  }

  auto profile_signals_report = std::make_unique<em::ProfileSignalsReport>();
  profile_report->set_allocated_profile_signals_report(
      profile_signals_report.release());
  browser_report->add_chrome_user_profile_infos()->Swap(profile_report.get());

  request->GetChromeProfileReportRequest().set_allocated_browser_report(
      browser_report.release());

  OnRequestReady(std::move(request), std::move(callback));
}

void ChromeProfileRequestGenerator::OnRequestReady(
    std::unique_ptr<ReportRequest> request,
    ReportCallback callback) {
  ReportRequestQueue requests;
  requests.push(std::move(request));
  std::move(callback).Run(std::move(requests));
}

}  // namespace enterprise_reporting
