// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/security_interstitials/core/legacy_tls_ui.h"

#include "base/i18n/number_formatting.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "components/security_interstitials/core/common_string_util.h"
#include "components/security_interstitials/core/controller_client.h"
#include "components/security_interstitials/core/metrics_helper.h"
#include "components/ssl_errors/error_info.h"
#include "components/strings/grit/components_strings.h"
#include "net/base/escape.h"
#include "ui/base/l10n/l10n_util.h"

namespace {

// Path to the relevant help center page.
const char kHelpPath[] = "answer/6098869";

}  // namespace

namespace security_interstitials {

LegacyTLSUI::LegacyTLSUI(const GURL& request_url,
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

LegacyTLSUI::~LegacyTLSUI() {
  // If the page is closing without an explicit decision, record it as not
  // proceeding.
  if (!user_made_decision_) {
    controller_->metrics_helper()->RecordUserDecision(
        MetricsHelper::DONT_PROCEED);
  }
  controller_->metrics_helper()->RecordShutdownMetrics();
}

void LegacyTLSUI::PopulateStringsForHTML(
    base::DictionaryValue* load_time_data) {
  CHECK(load_time_data);

  // Shared with other SSL errors.
  common_string_util::PopulateSSLLayoutStrings(cert_error_, load_time_data);
  common_string_util::PopulateSSLDebuggingStrings(
      ssl_info_, base::Time::NowFromSystemTime(), load_time_data);
  load_time_data->SetBoolean("overridable", true);
  load_time_data->SetBoolean("hide_primary_button", false);
  load_time_data->SetBoolean("bad_clock", false);
  load_time_data->SetString("type", "LEGACY_TLS");

  const base::string16 hostname(
      common_string_util::GetFormattedHostName(request_url_));

  // Set strings that are shared between enterprise and non-enterprise
  // interstitials.
  load_time_data->SetString("tabTitle",
                            l10n_util::GetStringUTF16(IDS_SSL_V2_TITLE));
  load_time_data->SetString("heading",
                            l10n_util::GetStringUTF16(IDS_LEGACY_TLS_HEADING));
  load_time_data->SetString(
      "primaryButtonText",
      l10n_util::GetStringUTF16(IDS_SSL_OVERRIDABLE_SAFETY_BUTTON));
  load_time_data->SetString(
      "primaryParagraph",
      l10n_util::GetStringUTF16(IDS_LEGACY_TLS_PRIMARY_PARAGRAPH));
  load_time_data->SetString(
      "explanationParagraph",
      l10n_util::GetStringUTF16(IDS_LEGACY_TLS_EXPLANATION));
  load_time_data->SetString(
      "finalParagraph", l10n_util::GetStringFUTF16(
                            IDS_SSL_OVERRIDABLE_PROCEED_PARAGRAPH, hostname));
}

void LegacyTLSUI::HandleCommand(SecurityInterstitialCommand command) {
  switch (command) {
    case CMD_DONT_PROCEED: {
      controller_->metrics_helper()->RecordUserDecision(
          MetricsHelper::DONT_PROCEED);
      user_made_decision_ = true;
      controller_->GoBack();
      break;
    }
    case CMD_PROCEED: {
      controller_->metrics_helper()->RecordUserDecision(MetricsHelper::PROCEED);
      controller_->Proceed();
      user_made_decision_ = true;
      break;
    }
    case CMD_SHOW_MORE_SECTION:
      controller_->metrics_helper()->RecordUserInteraction(
          security_interstitials::MetricsHelper::SHOW_ADVANCED);
      break;
    case CMD_OPEN_HELP_CENTER: {
      controller_->metrics_helper()->RecordUserInteraction(
          security_interstitials::MetricsHelper::SHOW_LEARN_MORE);
      // Add cert error code as a ref to support URL, this is used to expand the
      // right section if the user is redirected to chrome://connection-help.
      GURL::Replacements replacements;
      // This has to be stored in a separate variable, otherwise asan throws a
      // use-after-scope error
      std::string cert_error_string =
          base::UTF16ToUTF8(base::FormatNumber(cert_error_));
      replacements.SetRefStr(cert_error_string);
      // If |support_url_| is invalid, use the default help center url.
      controller_->OpenUrlInNewForegroundTab(
          controller_->GetBaseHelpCenterUrl()
              .Resolve(kHelpPath)
              .ReplaceComponents(replacements));
      break;
    }

    case CMD_RELOAD:
    case CMD_OPEN_DATE_SETTINGS:
    case CMD_OPEN_DIAGNOSTIC:
    case CMD_OPEN_LOGIN:
    case CMD_REPORT_PHISHING_ERROR:
    case CMD_OPEN_WHITEPAPER:
    case CMD_OPEN_REPORTING_PRIVACY:
    case CMD_DO_REPORT:
    case CMD_DONT_REPORT:
    case CMD_OPEN_ENHANCED_PROTECTION_SETTINGS:
      // Not supported by the legacy TLS error page.
      NOTREACHED() << "Unsupported command: " << command;
      break;
    case CMD_ERROR:
    case CMD_TEXT_FOUND:
    case CMD_TEXT_NOT_FOUND:
      // Commands are for testing.
      break;
  }
}

}  // namespace security_interstitials
