// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ENTERPRISE_BROWSER_REPORTING_REPORT_GENERATION_CONFIG_H_
#define COMPONENTS_ENTERPRISE_BROWSER_REPORTING_REPORT_GENERATION_CONFIG_H_

#include <cstdint>
#include <string>

#include "components/enterprise/browser/reporting/report_type.h"

// Enum represnting how security signals will be included in the current report.
// This enum should be kept in sync with the `SecuritySignalsMode` enum in
// tools/metrics/histograms/metadata/enterprise/enums.xml.
enum class SecuritySignalsMode {
  // No security signals will be uploaded in the report.
  kNoSignals = 0,
  // Security signals will be uploaded alongside an existing report format. Only
  // profile status report is currently supported.
  kSignalsAttached = 1,
  // Security signals will be uploaded exclusively in its own report.
  kSignalsOnly = 2,
  kMaxValue = kSignalsOnly
};

namespace enterprise_reporting {

// The trigger leading to report generation. Values are bitmasks in the
// |pending_triggers_| bitfield.
enum ReportTrigger : uint32_t {
  kTriggerNone = 0,              // No trigger.
  kTriggerTimer = 1U << 0,       // The periodic timer expired.
  kTriggerUpdate = 1U << 1,      // An update was detected.
  kTriggerNewVersion = 1U << 2,  // A new version is running.
  kTriggerManual = 1U << 3,      // Trigger manually.
  kTriggerSecurity = 1U << 4,    // Triggered by a security trigger.
};

// Struct that includes various configuration of report generation and upload
// process. Only used by profile-level reporting for now.
struct ReportGenerationConfig {
  ReportGenerationConfig(ReportTrigger report_trigger,
                         ReportType report_type,
                         SecuritySignalsMode security_signals_mode,
                         bool use_cookies);
  explicit ReportGenerationConfig(ReportTrigger report_trigger);
  ReportGenerationConfig();
  ~ReportGenerationConfig();

  bool operator==(const ReportGenerationConfig&) const;

  // Returns readable string representation of the configuration, used for
  // logging and debugging purposes.
  std::string ToString() const;

  ReportTrigger report_trigger;
  ReportType report_type;
  SecuritySignalsMode security_signals_mode;
  bool use_cookies;
};

}  // namespace enterprise_reporting

#endif  // COMPONENTS_ENTERPRISE_BROWSER_REPORTING_REPORT_GENERATION_CONFIG_H_
