// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ENTERPRISE_BROWSER_REPORTING_REPORT_GENERATION_CONFIG_H_
#define COMPONENTS_ENTERPRISE_BROWSER_REPORTING_REPORT_GENERATION_CONFIG_H_

#include <string>

#include "components/enterprise/browser/reporting/report_type.h"

enum class SecuritySignalsMode {
  kNoSignals = 0,
  kSignalsAttached = 1,
  kSignalsOnly = 2,
};

namespace enterprise_reporting {

// Struct that includes various configuration of report generation and upload
// process. Only used by profile-level reporting for now.
struct ReportGenerationConfig {
  ReportGenerationConfig(ReportType report_type,
                         SecuritySignalsMode security_signals_mode,
                         bool use_cookies);
  ReportGenerationConfig();
  ~ReportGenerationConfig();

  bool operator==(const ReportGenerationConfig&) const;

  // Returns readable string representation of the configuration, used for
  // logging and debugging purposes.
  std::string ToString() const;

  ReportType report_type{};
  SecuritySignalsMode security_signals_mode{SecuritySignalsMode::kNoSignals};
  bool use_cookies = false;
};

}  // namespace enterprise_reporting

#endif  // COMPONENTS_ENTERPRISE_BROWSER_REPORTING_REPORT_GENERATION_CONFIG_H_
