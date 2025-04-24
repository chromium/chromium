// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/enterprise/browser/reporting/report_util.h"

#include "base/files/file_path.h"
#include "crypto/sha2.h"

namespace enterprise_reporting {

namespace em = enterprise_management;

std::string ObfuscateFilePath(const std::string& file_path) {
  return crypto::SHA256HashString(file_path);
}

em::SettingValue TranslateSettingValue(
    device_signals::SettingValue setting_value) {
  switch (setting_value) {
    case device_signals::SettingValue::UNKNOWN:
      return em::SettingValue::UNKNOWN;
    case device_signals::SettingValue::DISABLED:
      return em::SettingValue::DISABLED;
    case device_signals::SettingValue::ENABLED:
      return em::SettingValue::ENABLED;
  }
}

em::ProfileSignalsReport::PasswordProtectionTrigger
TranslatePasswordProtectionTrigger(
    std::optional<safe_browsing::PasswordProtectionTrigger> trigger) {
  if (trigger == std::nullopt) {
    return em::ProfileSignalsReport::POLICY_UNSET;
  }
  switch (trigger.value()) {
    case safe_browsing::PasswordProtectionTrigger::PASSWORD_PROTECTION_OFF:
      return em::ProfileSignalsReport::PASSWORD_PROTECTION_OFF;
    case safe_browsing::PasswordProtectionTrigger::PASSWORD_REUSE:
      return em::ProfileSignalsReport::PASSWORD_REUSE;
    case safe_browsing::PasswordProtectionTrigger::PHISHING_REUSE:
      return em::ProfileSignalsReport::PHISHING_REUSE;
    case safe_browsing::PasswordProtectionTrigger::
        PASSWORD_PROTECTION_TRIGGER_MAX:
      NOTREACHED();
  }
}

em::ProfileSignalsReport::RealtimeUrlCheckMode TranslateRealtimeUrlCheckMode(
    enterprise_connectors::EnterpriseRealTimeUrlCheckMode mode) {
  switch (mode) {
    case enterprise_connectors::EnterpriseRealTimeUrlCheckMode::
        REAL_TIME_CHECK_DISABLED:
      return em::ProfileSignalsReport::DISABLED;
    case enterprise_connectors::EnterpriseRealTimeUrlCheckMode::
        REAL_TIME_CHECK_FOR_MAINFRAME_ENABLED:
      return em::ProfileSignalsReport::ENABLED_MAIN_FRAME;
  }
}

em::ProfileSignalsReport::SafeBrowsingLevel TranslateSafeBrowsingLevel(
    safe_browsing::SafeBrowsingState level) {
  switch (level) {
    case safe_browsing::SafeBrowsingState::NO_SAFE_BROWSING:
      return em::ProfileSignalsReport::NO_SAFE_BROWSING;
    case safe_browsing::SafeBrowsingState::STANDARD_PROTECTION:
      return em::ProfileSignalsReport::STANDARD_PROTECTION;
    case safe_browsing::SafeBrowsingState::ENHANCED_PROTECTION:
      return em::ProfileSignalsReport::ENHANCED_PROTECTION;
  }
}

#if BUILDFLAG(IS_WIN)
std::unique_ptr<em::AntiVirusProduct> TranslateAvProduct(
    device_signals::AvProduct av_product) {
  auto av_product_in_report = std::make_unique<em::AntiVirusProduct>();
  switch (av_product.state) {
    case device_signals::AvProductState::kOn:
      av_product_in_report->set_state(em::AntiVirusProduct::ON);
      break;
    case device_signals::AvProductState::kOff:
      av_product_in_report->set_state(em::AntiVirusProduct::OFF);
      break;
    case device_signals::AvProductState::kSnoozed:
      av_product_in_report->set_state(em::AntiVirusProduct::SNOOZED);
      break;
    case device_signals::AvProductState::kExpired:
      av_product_in_report->set_state(em::AntiVirusProduct::EXPIRED);
      break;
  }

  av_product_in_report->set_display_name(av_product.display_name);
  av_product_in_report->set_product_id(av_product.product_id);

  return av_product_in_report;
}
#endif  // BUILDFLAG(IS_WIN)

}  // namespace enterprise_reporting
