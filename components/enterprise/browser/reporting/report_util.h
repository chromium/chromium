// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ENTERPRISE_BROWSER_REPORTING_REPORT_UTIL_H_
#define COMPONENTS_ENTERPRISE_BROWSER_REPORTING_REPORT_UTIL_H_

#include <string>

#include "build/build_config.h"
#include "components/device_signals/core/browser/signals_types.h"
#include "components/policy/proto/device_management_backend.pb.h"

namespace enterprise_reporting {

// Returns the obfusted `file_path` string with SHA256 algorithm.
std::string ObfuscateFilePath(const std::string& file_path);

enterprise_management::SettingValue TranslateSettingValue(
    device_signals::SettingValue setting_value);

enterprise_management::ProfileSignalsReport::PasswordProtectionTrigger
TranslatePasswordProtectionTrigger(
    std::optional<safe_browsing::PasswordProtectionTrigger> trigger);

enterprise_management::ProfileSignalsReport::RealtimeUrlCheckMode
TranslateRealtimeUrlCheckMode(
    enterprise_connectors::EnterpriseRealTimeUrlCheckMode mode);

enterprise_management::ProfileSignalsReport::SafeBrowsingLevel
TranslateSafeBrowsingLevel(safe_browsing::SafeBrowsingState level);

#if BUILDFLAG(IS_WIN)
std::unique_ptr<enterprise_management::AntiVirusProduct> TranslateAvProduct(
    device_signals::AvProduct av_product);
#endif  // BUILDFLAG(IS_WIN)

// Utility function to convert report proto to readable, JSON format that
// contains security signals-related fields only. Only
// `ChromeProfileReportRequest` is currently supported.
std::string GetSecuritySignalsInReport(
    const enterprise_management::ChromeProfileReportRequest&
        chrome_profile_report_request);

}  // namespace enterprise_reporting

#endif  // COMPONENTS_ENTERPRISE_BROWSER_REPORTING_REPORT_UTIL_H_
