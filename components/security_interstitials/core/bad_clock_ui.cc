// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/security_interstitials/core/bad_clock_ui.h"

#include "base/i18n/time_formatting.h"
#include "components/security_interstitials/core/common_string_util.h"
#include "components/security_interstitials/core/metrics_helper.h"
#include "components/strings/grit/components_strings.h"
#include "ui/base/l10n/l10n_util.h"

namespace security_interstitials {

BadClockUI::BadClockUI(const GURL& request_url,
                       int cert_error,
                       const net::SSLInfo& ssl_info,
                       const base::Time& time_triggered,
                       ssl_errors::ClockState clock_state,
                       ControllerClient* controller)
    : request_url_(request_url),
      cert_error_(cert_error),
      ssl_info_(ssl_info),
      time_triggered_(time_triggered),
      controller_(controller),
      clock_state_(clock_state) {
  controller_->metrics_helper()->RecordUserInteraction(
      security_interstitials::MetricsHelper::TOTAL_VISITS);

  ssl_errors::RecordUMAStatisticsForClockInterstitial(false, clock_state_,
                                                      cert_error_);
}

BadClockUI::~BadClockUI() {
  controller_->metrics_helper()->RecordShutdownMetrics();
}

void BadClockUI::PopulateStringsForHTML(base::Value::Dict& load_time_data) {
  // Shared with other SSL errors.
  common_string_util::PopulateSSLLayoutStrings(cert_error_, load_time_data);
  common_string_util::PopulateSSLDebuggingStrings(ssl_info_, time_triggered_,
                                                  load_time_data);

  // Clock-specific strings.
  PopulateClockStrings(load_time_data);
  load_time_data.Set("finalParagraph", "");  // Placeholder.
}

void BadClockUI::PopulateClockStrings(base::Value::Dict& load_time_data) {
  load_time_data.Set("bad_clock", true);
  load_time_data.Set("overridable", false);
  load_time_data.Set("hide_primary_button",
                     !controller_->CanLaunchDateAndTimeSettings());
  int heading_string = 0;
  switch (clock_state_) {
    case ssl_errors::CLOCK_STATE_FUTURE:
      heading_string = IDS_CLOCK_ERROR_AHEAD_HEADING;
      break;
    case ssl_errors::CLOCK_STATE_PAST:
      heading_string = IDS_CLOCK_ERROR_BEHIND_HEADING;
      break;
    default:
      NOTREACHED_IN_MIGRATION();
  }
  load_time_data.Set("tabTitle",
                     l10n_util::GetStringUTF16(IDS_CLOCK_ERROR_TITLE));
  load_time_data.Set("heading", l10n_util::GetStringUTF16(heading_string));
  load_time_data.Set("primaryParagraph",
                     l10n_util::GetStringFUTF16(
                         IDS_CLOCK_ERROR_PRIMARY_PARAGRAPH,
                         common_string_util::GetFormattedHostName(request_url_),
                         base::TimeFormatFriendlyDateAndTime(time_triggered_)));
  load_time_data.Set(
      "primaryButtonText",
      l10n_util::GetStringUTF16(IDS_CLOCK_ERROR_UPDATE_DATE_AND_TIME));
  load_time_data.Set("explanationParagraph",
                     l10n_util::GetStringUTF16(IDS_CLOCK_ERROR_EXPLANATION));
}

void BadClockUI::HandleCommand(SecurityInterstitialCommand command) {
  switch (command) {
    case CMD_DONT_PROCEED:
      controller_->GoBack();
      break;
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
    case CMD_OPEN_DATE_SETTINGS:
      if (!controller_->CanLaunchDateAndTimeSettings())
        NOTREACHED_IN_MIGRATION()
            << "This platform does not support date settings";
      controller_->metrics_helper()->RecordUserInteraction(
          security_interstitials::MetricsHelper::OPEN_TIME_SETTINGS);
      controller_->LaunchDateAndTimeSettings();
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
    case CMD_PROCEED:
    case CMD_OPEN_HELP_CENTER:
    case CMD_RELOAD:
    case CMD_OPEN_DIAGNOSTIC:
    case CMD_OPEN_LOGIN:
    case CMD_REPORT_PHISHING_ERROR:
    case CMD_CLOSE_INTERSTITIAL_WITHOUT_UI:
    case CMD_REQUEST_SITE_ACCESS_PERMISSION:
      // Not supported by the bad clock error page.
      NOTREACHED_IN_MIGRATION() << "Unsupported command: " << command;
      break;
    case CMD_ERROR:
    case CMD_TEXT_FOUND:
    case CMD_TEXT_NOT_FOUND:
      // Commands are for testing.
      break;
  }
}

}  // security_interstitials
