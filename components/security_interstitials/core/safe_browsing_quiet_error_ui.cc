// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/security_interstitials/core/safe_browsing_quiet_error_ui.h"

#include "base/i18n/time_formatting.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/stringprintf.h"
#include "components/google/core/common/google_util.h"
#include "components/grit/components_resources.h"
#include "components/security_interstitials/core/common_string_util.h"
#include "components/security_interstitials/core/metrics_helper.h"
#include "components/strings/grit/components_strings.h"
#include "net/base/escape.h"
#include "ui/base/l10n/l10n_util.h"

namespace security_interstitials {

SafeBrowsingQuietErrorUI::SafeBrowsingQuietErrorUI(
    const GURL& request_url,
    const GURL& main_frame_url,
    BaseSafeBrowsingErrorUI::SBInterstitialReason reason,
    const BaseSafeBrowsingErrorUI::SBErrorDisplayOptions& display_options,
    const std::string& app_locale,
    const base::Time& time_triggered,
    ControllerClient* controller,
    const bool is_giant_webview)
    : BaseSafeBrowsingErrorUI(request_url,
                              main_frame_url,
                              reason,
                              display_options,
                              app_locale,
                              time_triggered,
                              controller),
      is_giant_webview_(is_giant_webview) {
  controller->metrics_helper()->RecordUserDecision(MetricsHelper::SHOW);
  controller->metrics_helper()->RecordUserInteraction(
      MetricsHelper::TOTAL_VISITS);
  if (is_proceed_anyway_disabled()) {
    controller->metrics_helper()->RecordUserDecision(
        security_interstitials::MetricsHelper::PROCEEDING_DISABLED);
  }
}

SafeBrowsingQuietErrorUI::~SafeBrowsingQuietErrorUI() {
  controller()->metrics_helper()->RecordShutdownMetrics();
}

void SafeBrowsingQuietErrorUI::PopulateStringsForHtml(
    base::DictionaryValue* load_time_data) {
  DCHECK(load_time_data);

  common_string_util::PopulateDarkModeDisplaySetting(load_time_data);

  load_time_data->SetString("type", "SAFEBROWSING");
  load_time_data->SetString(
      "tabTitle", l10n_util::GetStringUTF16(IDS_SAFEBROWSING_V3_TITLE));
  load_time_data->SetBoolean("overridable", !is_proceed_anyway_disabled());
  load_time_data->SetString(
      "openDetails",
      l10n_util::GetStringUTF16(IDS_SAFEBROWSING_V3_OPEN_DETAILS_BUTTON));
  load_time_data->SetBoolean("is_giant", is_giant_webview_);

  switch (interstitial_reason()) {
    case BaseSafeBrowsingErrorUI::SB_REASON_MALWARE:
      PopulateMalwareLoadTimeData(load_time_data);
      break;
    case BaseSafeBrowsingErrorUI::SB_REASON_HARMFUL:
      PopulateHarmfulLoadTimeData(load_time_data);
      break;
    case BaseSafeBrowsingErrorUI::SB_REASON_PHISHING:
      PopulatePhishingLoadTimeData(load_time_data);
      break;
    case BaseSafeBrowsingErrorUI::SB_REASON_BILLING:
      PopulateBillingLoadTimeData(load_time_data);
      break;
  }

  // Not used by this interstitial.
  load_time_data->SetString("recurrentErrorParagraph", "");
  load_time_data->SetBoolean("show_recurrent_error_paragraph", false);
}

void SafeBrowsingQuietErrorUI::SetGiantWebViewForTesting(
    bool is_giant_webview) {
  is_giant_webview_ = is_giant_webview;
}

void SafeBrowsingQuietErrorUI::HandleCommand(
    SecurityInterstitialCommand command) {
  switch (command) {
    case CMD_PROCEED: {
      // User pressed on the button to proceed.
      if (!is_proceed_anyway_disabled()) {
        controller()->metrics_helper()->RecordUserDecision(
            MetricsHelper::PROCEED);
        controller()->Proceed();
      }
      break;
    }
    case CMD_DONT_PROCEED:
    case CMD_DO_REPORT:
    case CMD_DONT_REPORT:
    case CMD_SHOW_MORE_SECTION:
    case CMD_OPEN_HELP_CENTER:
    case CMD_RELOAD:
    case CMD_OPEN_REPORTING_PRIVACY:
    case CMD_OPEN_WHITEPAPER:
    case CMD_OPEN_DIAGNOSTIC:
    case CMD_REPORT_PHISHING_ERROR:
    case CMD_OPEN_DATE_SETTINGS:
    case CMD_OPEN_LOGIN:
    case CMD_ERROR:
    case CMD_TEXT_FOUND:
    case CMD_TEXT_NOT_FOUND:
      NOTREACHED();
      break;
  }
}

void SafeBrowsingQuietErrorUI::PopulateMalwareLoadTimeData(
    base::DictionaryValue* load_time_data) {
  load_time_data->SetBoolean("phishing", false);
  load_time_data->SetString(
      "heading", l10n_util::GetStringUTF16(IDS_MALWARE_WEBVIEW_HEADING));
  load_time_data->SetString(
      "explanationParagraph",
      l10n_util::GetStringUTF16(IDS_MALWARE_WEBVIEW_EXPLANATION_PARAGRAPH));
}

void SafeBrowsingQuietErrorUI::PopulateHarmfulLoadTimeData(
    base::DictionaryValue* load_time_data) {
  load_time_data->SetBoolean("phishing", false);
  load_time_data->SetString(
      "heading", l10n_util::GetStringUTF16(IDS_HARMFUL_WEBVIEW_HEADING));
  load_time_data->SetString(
      "explanationParagraph",
      l10n_util::GetStringUTF16(IDS_HARMFUL_WEBVIEW_EXPLANATION_PARAGRAPH));
}

void SafeBrowsingQuietErrorUI::PopulatePhishingLoadTimeData(
    base::DictionaryValue* load_time_data) {
  load_time_data->SetBoolean("phishing", true);
  load_time_data->SetString(
      "heading", l10n_util::GetStringUTF16(IDS_PHISHING_WEBVIEW_HEADING));
  load_time_data->SetString(
      "explanationParagraph",
      l10n_util::GetStringUTF16(IDS_PHISHING_WEBVIEW_EXPLANATION_PARAGRAPH));
}

void SafeBrowsingQuietErrorUI::PopulateBillingLoadTimeData(
    base::DictionaryValue* load_time_data) {
  load_time_data->SetBoolean("phishing", false);
  load_time_data->SetString("tabTitle",
                            l10n_util::GetStringUTF16(IDS_BILLING_TITLE));
  load_time_data->SetString(
      "heading", l10n_util::GetStringUTF16(IDS_BILLING_WEBVIEW_HEADING));
  load_time_data->SetString(
      "explanationParagraph",
      l10n_util::GetStringUTF16(IDS_BILLING_WEBVIEW_EXPLANATION_PARAGRAPH));
}

int SafeBrowsingQuietErrorUI::GetHTMLTemplateId() const {
  return IDR_SECURITY_INTERSTITIAL_QUIET_HTML;
}

}  // namespace security_interstitials
