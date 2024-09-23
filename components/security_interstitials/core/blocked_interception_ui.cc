// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/security_interstitials/core/blocked_interception_ui.h"

#include "base/i18n/number_formatting.h"
#include "base/strings/escape.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "components/security_interstitials/core/common_string_util.h"
#include "components/security_interstitials/core/metrics_helper.h"
#include "components/ssl_errors/error_info.h"
#include "components/strings/grit/components_strings.h"
#include "ui/base/l10n/l10n_util.h"

namespace security_interstitials {

BlockedInterceptionUI::BlockedInterceptionUI(const GURL& request_url,
                                             int cert_error,
                                             const net::SSLInfo& ssl_info,
                                             ControllerClient* controller)
    : request_url_(request_url),
      cert_error_(cert_error),
      ssl_info_(ssl_info),
      controller_(controller),
      user_made_decision_(false) {
  controller_->metrics_helper()->RecordUserInteraction(
      security_interstitials::MetricsHelper::TOTAL_VISITS);
}

BlockedInterceptionUI::~BlockedInterceptionUI() {
  // If the page is closing without an explicit decision, record it as not
  // proceeding.
  if (!user_made_decision_) {
    controller_->metrics_helper()->RecordUserDecision(
        MetricsHelper::DONT_PROCEED);
  }
  controller_->metrics_helper()->RecordShutdownMetrics();
}

void BlockedInterceptionUI::PopulateStringsForHTML(
    base::Value::Dict& load_time_data) {
  // Shared with other SSL errors.
  common_string_util::PopulateSSLLayoutStrings(cert_error_, load_time_data);
  common_string_util::PopulateSSLDebuggingStrings(
      ssl_info_, base::Time::NowFromSystemTime(), load_time_data);

  load_time_data.Set("overridable", true);
  load_time_data.Set("hide_primary_button", false);
  load_time_data.Set("bad_clock", false);
  load_time_data.Set("type", "BLOCKED_INTERCEPTION");

  const std::u16string hostname(
      common_string_util::GetFormattedHostName(request_url_));

  // Set strings that are shared between enterprise and non-enterprise
  // interstitials.
  load_time_data.Set(
      "tabTitle",
      l10n_util::GetStringFUTF16(IDS_BLOCKED_INTERCEPTION_HEADING, hostname));
  load_time_data.Set(
      "heading",
      l10n_util::GetStringFUTF16(IDS_BLOCKED_INTERCEPTION_HEADING, hostname));
  load_time_data.Set(
      "primaryButtonText",
      l10n_util::GetStringUTF16(IDS_SSL_OVERRIDABLE_SAFETY_BUTTON));
  load_time_data.Set("finalParagraph", "");

  // Reuse the strings from the WebUI page.
  load_time_data.Set("primaryParagraph",
                     l10n_util::GetStringUTF16(IDS_KNOWN_INTERCEPTION_BODY1));
  load_time_data.Set("explanationParagraph",
                     l10n_util::GetStringUTF16(IDS_KNOWN_INTERCEPTION_BODY2));

  load_time_data.Set("finalParagraph",
                     l10n_util::GetStringFUTF16(
                         IDS_SSL_OVERRIDABLE_PROCEED_PARAGRAPH, hostname));
}

void BlockedInterceptionUI::HandleCommand(SecurityInterstitialCommand command) {
  switch (command) {
    case CMD_PROCEED: {
      controller_->metrics_helper()->RecordUserDecision(MetricsHelper::PROCEED);
      controller_->Proceed();
      user_made_decision_ = true;
      break;
    }

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
    case CMD_OPEN_HELP_CENTER:
    case CMD_DONT_PROCEED:
    case CMD_RELOAD:
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

}  // namespace security_interstitials
