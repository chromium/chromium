// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/security_interstitials/core/safe_browsing_quiet_error_ui.h"

#include "base/i18n/time_formatting.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/escape.h"
#include "components/google/core/common/google_util.h"
#include "components/grit/components_resources.h"
#include "components/security_interstitials/core/common_string_util.h"
#include "components/security_interstitials/core/controller_client.h"
#include "components/security_interstitials/core/metrics_helper.h"
#include "components/strings/grit/components_strings.h"
#include "ui/base/l10n/l10n_util.h"

namespace security_interstitials {

SafeBrowsingQuietErrorUI::SafeBrowsingQuietErrorUI(
    const GURL& request_url,
    BaseSafeBrowsingErrorUI::SBInterstitialReason reason,
    const BaseSafeBrowsingErrorUI::SBErrorDisplayOptions& display_options,
    const std::string& app_locale,
    const base::Time& time_triggered,
    ControllerClient* controller,
    const bool is_giant_webview)
    : BaseSafeBrowsingErrorUI(request_url,
                              reason,
                              display_options,
                              app_locale,
                              time_triggered,
                              controller),
      is_giant_webview_(is_giant_webview) {
  user_made_decision_ = false;
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
    base::Value::Dict& load_time_data) {
  load_time_data.Set("type", "SAFEBROWSING");
  load_time_data.Set("tabTitle",
                     l10n_util::GetStringUTF16(IDS_SAFEBROWSING_V3_TITLE));
  load_time_data.Set("overridable", !is_proceed_anyway_disabled());
  load_time_data.Set(
      "openDetails",
      l10n_util::GetStringUTF16(IDS_SAFEBROWSING_V3_OPEN_DETAILS_BUTTON));
  load_time_data.Set("is_giant", is_giant_webview_);

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
  load_time_data.Set("recurrentErrorParagraph", "");
  load_time_data.Set("show_recurrent_error_paragraph", false);
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
      user_made_decision_ = true;
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
    case CMD_OPEN_ENHANCED_PROTECTION_SETTINGS:
    case CMD_CLOSE_INTERSTITIAL_WITHOUT_UI:
    case CMD_REQUEST_SITE_ACCESS_PERMISSION:
      NOTREACHED_IN_MIGRATION();
      break;
  }
}

void SafeBrowsingQuietErrorUI::PopulateMalwareLoadTimeData(
    base::Value::Dict& load_time_data) {
  load_time_data.Set("phishing", false);
  load_time_data.Set("heading",
                     l10n_util::GetStringUTF16(IDS_MALWARE_WEBVIEW_HEADING));
  load_time_data.Set(
      "explanationParagraph",
      l10n_util::GetStringUTF16(IDS_MALWARE_WEBVIEW_EXPLANATION_PARAGRAPH));
}

void SafeBrowsingQuietErrorUI::PopulateHarmfulLoadTimeData(
    base::Value::Dict& load_time_data) {
  load_time_data.Set("phishing", false);
  load_time_data.Set("heading",
                     l10n_util::GetStringUTF16(IDS_HARMFUL_WEBVIEW_HEADING));
  load_time_data.Set(
      "explanationParagraph",
      l10n_util::GetStringUTF16(IDS_HARMFUL_WEBVIEW_EXPLANATION_PARAGRAPH));
}

void SafeBrowsingQuietErrorUI::PopulatePhishingLoadTimeData(
    base::Value::Dict& load_time_data) {
  load_time_data.Set("phishing", true);
  load_time_data.Set("heading",
                     l10n_util::GetStringUTF16(IDS_PHISHING_WEBVIEW_HEADING));
  load_time_data.Set(
      "explanationParagraph",
      l10n_util::GetStringUTF16(IDS_PHISHING_WEBVIEW_EXPLANATION_PARAGRAPH));
}

void SafeBrowsingQuietErrorUI::PopulateBillingLoadTimeData(
    base::Value::Dict& load_time_data) {
  load_time_data.Set("phishing", false);
  load_time_data.Set("tabTitle", l10n_util::GetStringUTF16(IDS_BILLING_TITLE));
  load_time_data.Set("heading",
                     l10n_util::GetStringUTF16(IDS_BILLING_WEBVIEW_HEADING));
  load_time_data.Set(
      "explanationParagraph",
      l10n_util::GetStringUTF16(IDS_BILLING_WEBVIEW_EXPLANATION_PARAGRAPH));
}

int SafeBrowsingQuietErrorUI::GetHTMLTemplateId() const {
  return IDR_SECURITY_INTERSTITIAL_QUIET_HTML;
}

}  // namespace security_interstitials
