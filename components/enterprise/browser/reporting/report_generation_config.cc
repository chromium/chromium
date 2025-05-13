// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/enterprise/browser/reporting/report_generation_config.h"

#include "base/check_op.h"
#include "base/strings/stringprintf.h"

namespace {
constexpr char kReportGenerationConfigTemplate[] =
    R"(Trigger: %s, Report Type: %s, Security Signals Mode: %s,"
    " Using Cookies: %s)";

std::string_view ReportTriggerToString(
    enterprise_reporting::ReportTrigger report_trigger) {
  switch (report_trigger) {
    case enterprise_reporting::ReportTrigger::kTriggerNone:
      return "No trigger";
    case enterprise_reporting::ReportTrigger::kTriggerTimer:
      return "Periodic timer expired";
    case enterprise_reporting::ReportTrigger::kTriggerUpdate:
      return "An update was detected";
    case enterprise_reporting::ReportTrigger::kTriggerNewVersion:
      return "A new version is running";
    case enterprise_reporting::ReportTrigger::kTriggerManual:
      return "Trigger manually";
    case enterprise_reporting::ReportTrigger::kTriggerSecurity:
      return "Trigger for security signals";
  }
}

std::string_view TranslateReportType(
    enterprise_reporting::ReportType report_type) {
  switch (report_type) {
    case enterprise_reporting::ReportType::kFull:
      return "Full/Browser Report";
    case enterprise_reporting::ReportType::kBrowserVersion:
      return "Browser Version Report";
    case enterprise_reporting::ReportType::kProfileReport:
      return "Profile Report";
  }
}

std::string_view TranslateSecuritySignalsMode(
    SecuritySignalsMode security_signals_mode) {
  switch (security_signals_mode) {
    case SecuritySignalsMode::kNoSignals:
      return "No security signals reported";
    case SecuritySignalsMode::kSignalsAttached:
      return "Security signals reported alongside status report";
    case SecuritySignalsMode::kSignalsOnly:
      return "Security signals are reported exclusively";
  }
}

}  // namespace

namespace enterprise_reporting {

ReportGenerationConfig::ReportGenerationConfig(
    ReportTrigger report_trigger,
    ReportType report_type,
    SecuritySignalsMode security_signals_mode,
    bool use_cookies)
    : report_trigger(report_trigger),
      report_type(report_type),
      security_signals_mode(security_signals_mode),
      use_cookies(use_cookies) {
  // Currently security signals are only being reported in profile level
  // reporting.
  if (report_type != ReportType::kProfileReport) {
    CHECK_EQ(security_signals_mode, SecuritySignalsMode::kNoSignals);
  }
}

ReportGenerationConfig::ReportGenerationConfig(ReportTrigger report_trigger)
    : ReportGenerationConfig(report_trigger,
                             ReportType::kFull,
                             SecuritySignalsMode::kNoSignals,
                             /*use_cookies=*/false) {}

ReportGenerationConfig::ReportGenerationConfig()
    : ReportGenerationConfig(ReportTrigger::kTriggerNone) {}

ReportGenerationConfig::~ReportGenerationConfig() = default;

bool ReportGenerationConfig::operator==(const ReportGenerationConfig&) const =
    default;

std::string ReportGenerationConfig::ToString() const {
  return base::StringPrintf(kReportGenerationConfigTemplate,
                            ReportTriggerToString(report_trigger),
                            TranslateReportType(report_type),
                            TranslateSecuritySignalsMode(security_signals_mode),
                            use_cookies ? "Yes" : "No");
}

}  // namespace enterprise_reporting
