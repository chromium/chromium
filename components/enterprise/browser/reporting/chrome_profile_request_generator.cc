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
#include "components/enterprise/browser/reporting/report_util.h"
#include "components/enterprise/browser/reporting/reporting_delegate_factory.h"
#include "components/policy/core/common/policy_logger.h"

namespace enterprise_reporting {

namespace em = enterprise_management;

namespace {

em::ChromeProfileReportRequest::ReportType GetReportTypeFromSignalsMode(
    SecuritySignalsMode signals_mode) {
  switch (signals_mode) {
    case SecuritySignalsMode::kNoSignals:
      return em::ChromeProfileReportRequest::PROFILE_REPORT;
    case SecuritySignalsMode::kSignalsAttached:
      return em::ChromeProfileReportRequest::
          PROFILE_REPORT_WITH_SECURITY_SIGNALS;
    case SecuritySignalsMode::kSignalsOnly:
      return em::ChromeProfileReportRequest::PROFILE_SECURITY_SIGNALS;
  }
}

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
  request->GetChromeProfileReportRequest().set_report_type(
      GetReportTypeFromSignalsMode(generation_config.security_signals_mode));
  if (generation_config.security_signals_mode ==
      SecuritySignalsMode::kSignalsOnly) {
    return OnBaseReportsReady(std::move(request), std::move(callback),
                              generation_config,
                              std::make_unique<em::BrowserReport>(),
                              std::make_unique<em::ChromeUserProfileInfo>());
  }

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
    request->GetChromeProfileReportRequest().set_allocated_os_report(
        GetOSReport().release());
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
  auto os_report = GetOSReport();
  auto device_identifier = std::make_unique<em::BrowserDeviceIdentifier>();
  auto profile_signals_report = std::make_unique<em::ProfileSignalsReport>();

  if (response.os_signals_response) {
    const auto& os_signals = response.os_signals_response.value();

    device_identifier->set_computer_name(os_signals.display_name.value_or(""));
    device_identifier->set_serial_number(os_signals.serial_number.value_or(""));
    device_identifier->set_host_name(os_signals.hostname.value_or(""));

    os_report->set_device_enrollment_domain(
        os_signals.device_enrollment_domain.value_or(""));
    os_report->set_device_manufacturer(os_signals.device_manufacturer);
    os_report->set_device_model(os_signals.device_model);
    os_report->set_disk_encryption(
        TranslateSettingValue(os_signals.disk_encryption));
    const auto& mac_addresses =
        os_signals.mac_addresses.value_or(std::vector<std::string>());
    os_report->mutable_mac_addresses()->Add(mac_addresses.begin(),
                                            mac_addresses.end());

    os_report->set_name(os_signals.operating_system);
    os_report->set_os_firewall(TranslateSettingValue(os_signals.os_firewall));
    os_report->set_screen_lock_secured(
        TranslateSettingValue(os_signals.screen_lock_secured));
    const auto& system_dns_servers =
        os_signals.system_dns_servers.value_or(std::vector<std::string>());
    os_report->mutable_system_dns_servers()->Add(system_dns_servers.begin(),
                                                 system_dns_servers.end());

    os_report->set_version(os_signals.os_version);
#if BUILDFLAG(IS_WIN)
    os_report->set_machine_guid(os_signals.machine_guid.value_or(""));
    if (os_signals.secure_boot_mode) {
      os_report->set_secure_boot_mode(
          (os_signals.secure_boot_mode)
              ? TranslateSettingValue(os_signals.secure_boot_mode.value())
              : em::SettingValue::UNKNOWN);
    }
    os_report->set_windows_machine_domain(
        os_signals.windows_machine_domain.value_or(""));
    os_report->set_windows_user_domain(
        os_signals.windows_user_domain.value_or(""));
#endif  // BUILDFLAG(IS_WIN)

    browser_report->set_browser_version(os_signals.browser_version);
  }

  if (response.profile_signals_response) {
    const auto& profile_signals = response.profile_signals_response.value();
    profile_signals_report->set_built_in_dns_client_enabled(
        profile_signals.built_in_dns_client_enabled);
    profile_signals_report->set_chrome_remote_desktop_app_blocked(
        profile_signals.chrome_remote_desktop_app_blocked);
    profile_signals_report->set_password_protection_warning_trigger(
        TranslatePasswordProtectionTrigger(
            profile_signals.password_protection_warning_trigger));
    profile_signals_report->set_profile_enrollment_domain(
        profile_signals.profile_enrollment_domain.value_or(""));
    profile_signals_report->set_realtime_url_check_mode(
        TranslateRealtimeUrlCheckMode(profile_signals.realtime_url_check_mode));
    profile_signals_report->set_safe_browsing_protection_level(
        TranslateSafeBrowsingLevel(
            profile_signals.safe_browsing_protection_level));
    profile_signals_report->set_site_isolation_enabled(
        profile_signals.site_isolation_enabled);

    profile_signals_report->mutable_file_downloaded_providers()->Add(
        profile_signals.file_downloaded_providers.begin(),
        profile_signals.file_downloaded_providers.end());
    profile_signals_report->mutable_file_attached_providers()->Add(
        profile_signals.file_attached_providers.begin(),
        profile_signals.file_attached_providers.end());
    profile_signals_report->mutable_bulk_data_entry_providers()->Add(
        profile_signals.bulk_data_entry_providers.begin(),
        profile_signals.bulk_data_entry_providers.end());
    profile_signals_report->mutable_print_providers()->Add(
        profile_signals.print_providers.begin(),
        profile_signals.print_providers.end());
    profile_signals_report->mutable_security_event_providers()->Add(
        profile_signals.security_event_providers.begin(),
        profile_signals.security_event_providers.end());

    profile_report->set_allocated_profile_signals_report(
        profile_signals_report.release());
  }

#if BUILDFLAG(IS_WIN)
  if (response.av_signal_response) {
    const auto& antivirus_signals = response.av_signal_response.value();
    for (auto av_product : antivirus_signals.av_products) {
      os_report->add_antivirus_info()->Swap(
          TranslateAvProduct(av_product).get());
    }
  }

  if (response.hotfix_signal_response) {
    const auto& hotfix_signals = response.hotfix_signal_response.value();
    for (auto hotfix : hotfix_signals.hotfixes) {
      os_report->add_hotfixes(hotfix.hotfix_id);
    }
  }
#endif  // BUILDFLAG(IS_WIN)

  request->GetChromeProfileReportRequest()
      .set_allocated_browser_device_identifier(device_identifier.release());

  request->GetChromeProfileReportRequest().set_allocated_os_report(
      os_report.release());

  browser_report->add_chrome_user_profile_infos()->Swap(profile_report.get());

  request->GetChromeProfileReportRequest().set_allocated_browser_report(
      browser_report.release());

  VLOG_POLICY(1, REPORTING)
      << "Signals report request generated: "
      << GetSecuritySignalsInReport(request->GetChromeProfileReportRequest());
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
