// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/security_interstitials/core/mitm_software_ui.h"

#include "base/i18n/time_formatting.h"
#include "base/strings/escape.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "components/security_interstitials/core/common_string_util.h"
#include "components/security_interstitials/core/metrics_helper.h"
#include "components/ssl_errors/error_info.h"
#include "components/strings/grit/components_strings.h"
#include "ui/base/l10n/l10n_util.h"

namespace security_interstitials {

MITMSoftwareUI::MITMSoftwareUI(const GURL& request_url,
                               int cert_error,
                               const net::SSLInfo& ssl_info,
                               const std::string& mitm_software_name,
                               bool is_enterprise_managed,
                               ControllerClient* controller)
    : request_url_(request_url),
      cert_error_(cert_error),
      ssl_info_(ssl_info),
      mitm_software_name_(mitm_software_name),
      is_enterprise_managed_(is_enterprise_managed),
      controller_(controller) {
  controller_->metrics_helper()->RecordUserInteraction(
      security_interstitials::MetricsHelper::TOTAL_VISITS);
}

MITMSoftwareUI::~MITMSoftwareUI() {
  controller_->metrics_helper()->RecordShutdownMetrics();
}

void MITMSoftwareUI::PopulateStringsForHTML(base::Value::Dict& load_time_data) {
  // Shared with other SSL errors.
  common_string_util::PopulateSSLLayoutStrings(cert_error_, load_time_data);
  common_string_util::PopulateSSLDebuggingStrings(
      ssl_info_, base::Time::NowFromSystemTime(), load_time_data);

  // Set display booleans.
  load_time_data.Set("overridable", false);
  load_time_data.Set("hide_primary_button", true);
  load_time_data.Set("bad_clock", false);

  // Set strings that are shared between enterprise and non-enterprise
  // interstitials.
  load_time_data.Set("tabTitle", l10n_util::GetStringUTF16(IDS_SSL_V2_TITLE));
  load_time_data.Set("heading",
                     l10n_util::GetStringUTF16(IDS_MITM_SOFTWARE_HEADING));
  load_time_data.Set("primaryButtonText", "");
  load_time_data.Set("finalParagraph", "");

  if (is_enterprise_managed_) {
    MITMSoftwareUI::PopulateEnterpriseUserStringsForHTML(load_time_data);
    return;
  }

  MITMSoftwareUI::PopulateAtHomeUserStringsForHTML(load_time_data);
}

void MITMSoftwareUI::HandleCommand(SecurityInterstitialCommand command) {
  switch (command) {
    case CMD_DO_REPORT:
      controller_->SetReportingPreference(true);
      break;
    case CMD_DONT_REPORT:
      controller_->SetReportingPreference(false);
      break;
    case CMD_SHOW_MORE_SECTION:
      controller_->metrics_helper()->RecordUserInteraction(
          security_interstitials::MetricsHelper::SHOW_ADVANCED);
      break;
    case CMD_OPEN_REPORTING_PRIVACY:
      controller_->OpenExtendedReportingPrivacyPolicy(true);
      break;
    case CMD_OPEN_WHITEPAPER:
      controller_->OpenExtendedReportingWhitepaper(true);
      break;
    case CMD_OPEN_ENHANCED_PROTECTION_SETTINGS:
      controller_->metrics_helper()->RecordUserInteraction(
          security_interstitials::MetricsHelper::OPEN_ENHANCED_PROTECTION);
      controller_->OpenEnhancedProtectionSettings();
      break;
    case CMD_DONT_PROCEED:
    case CMD_OPEN_HELP_CENTER:
    case CMD_RELOAD:
    case CMD_PROCEED:
    case CMD_OPEN_DATE_SETTINGS:
    case CMD_OPEN_DIAGNOSTIC:
    case CMD_OPEN_LOGIN:
    case CMD_REPORT_PHISHING_ERROR:
    case CMD_CLOSE_INTERSTITIAL_WITHOUT_UI:
    case CMD_REQUEST_SITE_ACCESS_PERMISSION:
      // Not supported by the SSL error page.
      NOTREACHED_IN_MIGRATION() << "Unsupported command: " << command;
      break;
    case CMD_ERROR:
    case CMD_TEXT_FOUND:
    case CMD_TEXT_NOT_FOUND:
      // Commands are for testing.
      break;
  }
}

void MITMSoftwareUI::PopulateEnterpriseUserStringsForHTML(
    base::Value::Dict& load_time_data) {
  load_time_data.Set(
      "primaryParagraph",
      l10n_util::GetStringFUTF16(
          IDS_MITM_SOFTWARE_PRIMARY_PARAGRAPH_ENTERPRISE,
          base::EscapeForHTML(base::UTF8ToUTF16(mitm_software_name_))));
  load_time_data.Set(
      "explanationParagraph",
      l10n_util::GetStringFUTF16(
          IDS_MITM_SOFTWARE_EXPLANATION_ENTERPRISE,
          base::EscapeForHTML(base::UTF8ToUTF16(mitm_software_name_)),
          l10n_util::GetStringUTF16(IDS_MITM_SOFTWARE_EXPLANATION)));
}

void MITMSoftwareUI::PopulateAtHomeUserStringsForHTML(
    base::Value::Dict& load_time_data) {
  load_time_data.Set(
      "primaryParagraph",
      l10n_util::GetStringFUTF16(
          IDS_MITM_SOFTWARE_PRIMARY_PARAGRAPH_NONENTERPRISE,
          base::EscapeForHTML(base::UTF8ToUTF16(mitm_software_name_))));
  load_time_data.Set(
      "explanationParagraph",
      l10n_util::GetStringFUTF16(
          IDS_MITM_SOFTWARE_EXPLANATION_NONENTERPRISE,
          base::EscapeForHTML(base::UTF8ToUTF16(mitm_software_name_)),
          l10n_util::GetStringUTF16(IDS_MITM_SOFTWARE_EXPLANATION)));
}

}  // namespace security_interstitials
