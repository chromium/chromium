// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/enterprise/browser/reporting/report_generation_config.h"

#include "base/check_op.h"
#include "base/strings/stringprintf.h"

namespace {
constexpr char kReportGenerationConfigTemplate[] =
    R"(Report Type: %s, Security Signals Mode: %s, Using Cookies: %s)";

std::string TranslateReportType(enterprise_reporting::ReportType report_type) {
  switch (report_type) {
    case enterprise_reporting::ReportType::kFull:
      return "Full/Browser Report";
    case enterprise_reporting::ReportType::kBrowserVersion:
      return "Browser Version Report";
    case enterprise_reporting::ReportType::kProfileReport:
      return "Profile Report";
  }
}

std::string TranslateSecuritySignalsMode(
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
    ReportType report_type,
    SecuritySignalsMode security_signals_mode,
    bool use_cookies)
    : report_type(report_type),
      security_signals_mode(security_signals_mode),
      use_cookies(use_cookies) {
  // Currently security signals are only being reported in profile level
  // reporting.
  if (report_type != ReportType::kProfileReport) {
    CHECK_EQ(security_signals_mode, SecuritySignalsMode::kNoSignals);
  }
}

ReportGenerationConfig::ReportGenerationConfig()
    : ReportGenerationConfig(ReportType::kFull,
                             SecuritySignalsMode::kNoSignals,
                             /*use_cookies=*/false) {}

ReportGenerationConfig::~ReportGenerationConfig() = default;

bool ReportGenerationConfig::operator==(const ReportGenerationConfig&) const =
    default;

std::string ReportGenerationConfig::ToString() const {
  return base::StringPrintf(kReportGenerationConfigTemplate,
                            TranslateReportType(report_type),
                            TranslateSecuritySignalsMode(security_signals_mode),
                            use_cookies ? "Yes" : "No");
}

}  // namespace enterprise_reporting
