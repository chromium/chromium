// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/enterprise/browser/reporting/report_generation_config.h"

#include "base/check_op.h"

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

}  // namespace enterprise_reporting
