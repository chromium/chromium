// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/security_interstitials/core/safe_browsing_loud_error_ui.h"

#include "base/i18n/time_formatting.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "components/google/core/common/google_util.h"
#include "components/grit/components_resources.h"
#include "components/security_interstitials/core/common_string_util.h"
#include "components/security_interstitials/core/metrics_helper.h"
#include "components/strings/grit/components_strings.h"
#include "net/base/escape.h"
#include "net/base/url_util.h"
#include "ui/base/l10n/l10n_util.h"

namespace security_interstitials {
namespace {

// For malware interstitial pages, we link the problematic URL to Google's
// diagnostic page.
const char kSbDiagnosticUrl[] =
    "https://transparencyreport.google.com/safe-browsing/search?url=%s";

// Constants for the V4 phishing string upgrades.
const char kReportPhishingErrorUrl[] =
    "https://www.google.com/safebrowsing/report_error/";

void RecordExtendedReportingPrefChanged(bool report) {
  UMA_HISTOGRAM_BOOLEAN(
      "SafeBrowsing.Pref.Scout.SetPref.SBER2Pref.SecurityInterstitial", report);
}

}  // namespace

SafeBrowsingLoudErrorUI::SafeBrowsingLoudErrorUI(
    const GURL& request_url,
    const GURL& main_frame_url,
    SBInterstitialReason reason,
    const SBErrorDisplayOptions& display_options,
    const std::string& app_locale,
    const base::Time& time_triggered,
    ControllerClient* controller)
    : BaseSafeBrowsingErrorUI(request_url,
                              main_frame_url,
                              reason,
                              display_options,
                              app_locale,
                              time_triggered,
                              controller) {
  controller->metrics_helper()->RecordUserDecision(MetricsHelper::SHOW);
  controller->metrics_helper()->RecordUserInteraction(
      MetricsHelper::TOTAL_VISITS);
  if (is_proceed_anyway_disabled()) {
    controller->metrics_helper()->RecordUserDecision(
        security_interstitials::MetricsHelper::PROCEEDING_DISABLED);
  }
}

SafeBrowsingLoudErrorUI::~SafeBrowsingLoudErrorUI() {
  controller()->metrics_helper()->RecordShutdownMetrics();
}

void SafeBrowsingLoudErrorUI::PopulateStringsForHtml(
    base::DictionaryValue* load_time_data) {
  DCHECK(load_time_data);

  load_time_data->SetString("type", "SAFEBROWSING");
  load_time_data->SetString(
      "tabTitle", l10n_util::GetStringUTF16(IDS_SAFEBROWSING_V3_TITLE));
  load_time_data->SetString(
      "openDetails",
      l10n_util::GetStringUTF16(IDS_SAFEBROWSING_V3_OPEN_DETAILS_BUTTON));
  load_time_data->SetString(
      "closeDetails",
      l10n_util::GetStringUTF16(IDS_SAFEBROWSING_V3_CLOSE_DETAILS_BUTTON));
  load_time_data->SetString(
      "primaryButtonText",
      l10n_util::GetStringUTF16(IDS_SAFEBROWSING_OVERRIDABLE_SAFETY_BUTTON));
  load_time_data->SetBoolean("overridable", !is_proceed_anyway_disabled());

  load_time_data->SetBoolean(
      "hide_primary_button",
      always_show_back_to_safety() ? false : !controller()->CanGoBack());

  load_time_data->SetBoolean(
      "billing",
      interstitial_reason() == BaseSafeBrowsingErrorUI::SB_REASON_BILLING);

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

  PopulateExtendedReportingOption(load_time_data);
}

void SafeBrowsingLoudErrorUI::HandleCommand(
    SecurityInterstitialCommand command) {
  switch (command) {
    case CMD_PROCEED: {
      // User pressed on the button to proceed.
      if (!is_proceed_anyway_disabled()) {
        controller()->metrics_helper()->RecordUserDecision(
            MetricsHelper::PROCEED);
        controller()->Proceed();
        break;
      }
      // If the user can't proceed, fall through to CMD_DONT_PROCEED.
      FALLTHROUGH;
    }
    case CMD_DONT_PROCEED: {
      // User pressed on the button to return to safety.
      // Don't record the user action here because there are other ways of
      // triggering DontProceed, like clicking the back button.
      if (is_main_frame_load_blocked()) {
        // If the load is blocked, we want to close the interstitial and discard
        // the pending entry.
        controller()->GoBack();
      } else {
        // Otherwise the offending entry has committed, and we need to go back
        // or to a safe page.  We will close the interstitial when that page
        // commits.
        controller()->GoBackAfterNavigationCommitted();
      }
      break;
    }
    case CMD_DO_REPORT: {
      // User enabled SB Extended Reporting via the checkbox.
      set_extended_reporting(true);
      controller()->SetReportingPreference(true);
      RecordExtendedReportingPrefChanged(true);
      break;
    }
    case CMD_DONT_REPORT: {
      // User disabled SB Extended Reporting via the checkbox.
      set_extended_reporting(false);
      controller()->SetReportingPreference(false);
      RecordExtendedReportingPrefChanged(false);
      break;
    }
    case CMD_SHOW_MORE_SECTION: {
      controller()->metrics_helper()->RecordUserInteraction(
          security_interstitials::MetricsHelper::SHOW_ADVANCED);
      break;
    }
    case CMD_OPEN_HELP_CENTER: {
      // User pressed "Learn more".
      controller()->metrics_helper()->RecordUserInteraction(
          security_interstitials::MetricsHelper::SHOW_LEARN_MORE);

      GURL learn_more_url = controller()->GetBaseHelpCenterUrl();
      learn_more_url = net::AppendQueryParameter(
          learn_more_url, "p", get_help_center_article_link());
      learn_more_url =
          google_util::AppendGoogleLocaleParam(learn_more_url, app_locale());
      controller()->OpenURL(should_open_links_in_new_tab(), learn_more_url);
      break;
    }
    case CMD_RELOAD: {
      controller()->metrics_helper()->RecordUserInteraction(
          security_interstitials::MetricsHelper::RELOAD);
      controller()->Reload();
      break;
    }
    case CMD_OPEN_REPORTING_PRIVACY: {
      // User pressed on the SB Extended Reporting "privacy policy" link.
      controller()->OpenExtendedReportingPrivacyPolicy(
          should_open_links_in_new_tab());
      break;
    }
    case CMD_OPEN_WHITEPAPER: {
      controller()->OpenExtendedReportingWhitepaper(
          should_open_links_in_new_tab());
      break;
    }
    case CMD_OPEN_DIAGNOSTIC: {
      controller()->metrics_helper()->RecordUserInteraction(
          security_interstitials::MetricsHelper::SHOW_DIAGNOSTIC);
      std::string diagnostic = base::StringPrintf(
          kSbDiagnosticUrl,
          net::EscapeQueryParamValue(request_url().spec(), true).c_str());
      GURL diagnostic_url(diagnostic);
      diagnostic_url =
          google_util::AppendGoogleLocaleParam(diagnostic_url, app_locale());
      controller()->OpenURL(should_open_links_in_new_tab(), diagnostic_url);
      break;
    }
    case CMD_REPORT_PHISHING_ERROR: {
      controller()->metrics_helper()->RecordUserInteraction(
          security_interstitials::MetricsHelper::REPORT_PHISHING_ERROR);
      GURL phishing_error_url(kReportPhishingErrorUrl);
      phishing_error_url = google_util::AppendGoogleLocaleParam(
          phishing_error_url, app_locale());
      controller()->OpenURL(should_open_links_in_new_tab(), phishing_error_url);
      break;
    }
    case CMD_OPEN_DATE_SETTINGS:
    case CMD_OPEN_LOGIN:
    case CMD_ERROR:
    case CMD_TEXT_FOUND:
    case CMD_TEXT_NOT_FOUND:
      break;
  }
}

void SafeBrowsingLoudErrorUI::PopulateMalwareLoadTimeData(
    base::DictionaryValue* load_time_data) {
  load_time_data->SetBoolean("phishing", false);
  load_time_data->SetString("heading",
                            l10n_util::GetStringUTF16(IDS_MALWARE_V3_HEADING));
  load_time_data->SetString(
      "primaryParagraph",
      l10n_util::GetStringFUTF16(
          IDS_MALWARE_V3_PRIMARY_PARAGRAPH,
          common_string_util::GetFormattedHostName(request_url())));
  load_time_data->SetString(
      "explanationParagraph",
      is_main_frame_load_blocked()
          ? l10n_util::GetStringFUTF16(
                IDS_MALWARE_V3_EXPLANATION_PARAGRAPH,
                common_string_util::GetFormattedHostName(request_url()))
          : l10n_util::GetStringFUTF16(
                IDS_MALWARE_V3_EXPLANATION_PARAGRAPH_SUBRESOURCE,
                base::UTF8ToUTF16(main_frame_url().host()),
                common_string_util::GetFormattedHostName(request_url())));
  load_time_data->SetString(
      "finalParagraph",
      l10n_util::GetStringUTF16(IDS_MALWARE_V3_PROCEED_PARAGRAPH));
}

void SafeBrowsingLoudErrorUI::PopulateHarmfulLoadTimeData(
    base::DictionaryValue* load_time_data) {
  load_time_data->SetBoolean("phishing", false);
  load_time_data->SetString("heading",
                            l10n_util::GetStringUTF16(IDS_HARMFUL_V3_HEADING));
  load_time_data->SetString(
      "primaryParagraph",
      l10n_util::GetStringFUTF16(
          IDS_HARMFUL_V3_PRIMARY_PARAGRAPH,
          common_string_util::GetFormattedHostName(request_url())));
  load_time_data->SetString(
      "explanationParagraph",
      l10n_util::GetStringFUTF16(
          IDS_HARMFUL_V3_EXPLANATION_PARAGRAPH,
          common_string_util::GetFormattedHostName(request_url())));
  load_time_data->SetString(
      "finalParagraph",
      l10n_util::GetStringUTF16(IDS_HARMFUL_V3_PROCEED_PARAGRAPH));
}

void SafeBrowsingLoudErrorUI::PopulatePhishingLoadTimeData(
    base::DictionaryValue* load_time_data) {
  load_time_data->SetBoolean("phishing", true);
  load_time_data->SetString("heading",
                            l10n_util::GetStringUTF16(IDS_PHISHING_V4_HEADING));
  load_time_data->SetString(
      "primaryParagraph",
      l10n_util::GetStringFUTF16(
          IDS_PHISHING_V4_PRIMARY_PARAGRAPH,
          common_string_util::GetFormattedHostName(request_url())));
  load_time_data->SetString(
      "explanationParagraph",
      l10n_util::GetStringFUTF16(
          IDS_PHISHING_V4_EXPLANATION_PARAGRAPH,
          common_string_util::GetFormattedHostName(request_url())));
  load_time_data->SetString(
      "finalParagraph",
      l10n_util::GetStringUTF16(IDS_PHISHING_V4_PROCEED_AND_REPORT_PARAGRAPH));
}

void SafeBrowsingLoudErrorUI::PopulateExtendedReportingOption(
    base::DictionaryValue* load_time_data) {
  bool can_show_extended_reporting_option = CanShowExtendedReportingOption();
  load_time_data->SetBoolean(security_interstitials::kDisplayCheckBox,
                             can_show_extended_reporting_option);
  if (!can_show_extended_reporting_option) {
    return;
  }

  const std::string privacy_link = base::StringPrintf(
      security_interstitials::kPrivacyLinkHtml,
      security_interstitials::CMD_OPEN_REPORTING_PRIVACY,
      l10n_util::GetStringUTF8(IDS_SAFE_BROWSING_PRIVACY_POLICY_PAGE).c_str());
  load_time_data->SetString(
      security_interstitials::kOptInLink,
      l10n_util::GetStringFUTF16(IDS_SAFE_BROWSING_SCOUT_REPORTING_AGREE,
                                 base::UTF8ToUTF16(privacy_link)));
  load_time_data->SetBoolean(security_interstitials::kBoxChecked,
                             is_extended_reporting_enabled());
}

void SafeBrowsingLoudErrorUI::PopulateBillingLoadTimeData(
    base::DictionaryValue* load_time_data) {
  common_string_util::PopulateDarkModeDisplaySetting(load_time_data);

  load_time_data->SetBoolean("phishing", false);
  load_time_data->SetBoolean("overridable", true);

  load_time_data->SetString("tabTitle",
                            l10n_util::GetStringUTF16(IDS_BILLING_TITLE));
  load_time_data->SetString("heading",
                            l10n_util::GetStringUTF16(IDS_BILLING_HEADING));
  load_time_data->SetString(
      "primaryParagraph",
      l10n_util::GetStringUTF16(IDS_BILLING_PRIMARY_PARAGRAPH));

  load_time_data->SetString(
      "primaryButtonText",
      l10n_util::GetStringUTF16(IDS_BILLING_PRIMARY_BUTTON));
  load_time_data->SetString(
      "proceedButtonText",
      l10n_util::GetStringUTF16(IDS_BILLING_PROCEED_BUTTON));

  load_time_data->SetString("openDetails", "");
  load_time_data->SetString("closeDetails", "");
  load_time_data->SetString("explanationParagraph", "");
  load_time_data->SetString("finalParagraph", "");
}

int SafeBrowsingLoudErrorUI::GetHTMLTemplateId() const {
  return IDR_SECURITY_INTERSTITIAL_HTML;
}

}  // namespace security_interstitials
