// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/security_interstitials/core/safe_browsing_loud_error_ui.h"

#include "base/i18n/time_formatting.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/escape.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "components/feature_engagement/public/feature_list.h"
#include "components/google/core/common/google_util.h"
#include "components/grit/components_resources.h"
#include "components/safe_browsing/core/common/features.h"
#include "components/security_interstitials/core/common_string_util.h"
#include "components/security_interstitials/core/controller_client.h"
#include "components/security_interstitials/core/metrics_helper.h"
#include "components/strings/grit/components_strings.h"
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
    "https://safebrowsing.google.com/safebrowsing/report_error/?url=%s";

void RecordExtendedReportingPrefChanged(bool report) {
  UMA_HISTOGRAM_BOOLEAN("SafeBrowsing.Pref.Extended.SecurityInterstitial",
                        report);
}

}  // namespace

SafeBrowsingLoudErrorUI::SafeBrowsingLoudErrorUI(
    const GURL& request_url,
    SBInterstitialReason reason,
    const SBErrorDisplayOptions& display_options,
    const std::string& app_locale,
    const base::Time& time_triggered,
    ControllerClient* controller,
    bool created_prior_to_navigation)
    : BaseSafeBrowsingErrorUI(request_url,
                              reason,
                              display_options,
                              app_locale,
                              time_triggered,
                              controller),
      created_prior_to_navigation_(created_prior_to_navigation) {
  user_made_decision_ = false;
  interstitial_interaction_data_ =
      std::make_unique<InterstitialInteractionMap>();
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
    base::Value::Dict& load_time_data) {
  load_time_data.Set("type", "SAFEBROWSING");
  load_time_data.Set("tabTitle",
                     l10n_util::GetStringUTF16(IDS_SAFEBROWSING_V3_TITLE));
  load_time_data.Set(
      "openDetails",
      l10n_util::GetStringUTF16(IDS_SAFEBROWSING_V3_OPEN_DETAILS_BUTTON));
  load_time_data.Set(
      "closeDetails",
      l10n_util::GetStringUTF16(IDS_SAFEBROWSING_V3_CLOSE_DETAILS_BUTTON));
  load_time_data.Set(
      "primaryButtonText",
      l10n_util::GetStringUTF16(IDS_SAFEBROWSING_OVERRIDABLE_SAFETY_BUTTON));
  load_time_data.Set("overridable", !is_proceed_anyway_disabled());
  load_time_data.Set(
      security_interstitials::kOptInLink,
      l10n_util::GetStringUTF16(IDS_SAFE_BROWSING_SCOUT_REPORTING_AGREE));
  load_time_data.Set(
      security_interstitials::kEnhancedProtectionMessage,
      l10n_util::GetStringUTF16(IDS_SAFE_BROWSING_ENHANCED_PROTECTION_MESSAGE));

  if (always_show_back_to_safety()) {
    load_time_data.Set("hide_primary_button", false);
  } else {
    load_time_data.Set("hide_primary_button",
                       created_prior_to_navigation_
                           ? !controller()->CanGoBackBeforeNavigation()
                           : !controller()->CanGoBack());
  }

  load_time_data.Set("billing", interstitial_reason() ==
                                    BaseSafeBrowsingErrorUI::SB_REASON_BILLING);

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

  PopulateExtendedReportingOption(load_time_data);
  PopulateEnhancedProtectionMessage(load_time_data);
}

void SafeBrowsingLoudErrorUI::HandleCommand(
    SecurityInterstitialCommand command) {
  UpdateInterstitialInteractionData(command);

  switch (command) {
    case CMD_PROCEED: {
      // User pressed on the button to proceed.
      user_made_decision_ = true;
      if (!is_proceed_anyway_disabled()) {
        controller()->metrics_helper()->RecordUserDecision(
            MetricsHelper::PROCEED);
        controller()->Proceed();
        break;
      }
      // If the user can't proceed, fall through to CMD_DONT_PROCEED.
      [[fallthrough]];
    }
    case CMD_DONT_PROCEED: {
      // User pressed on the button to return to safety.
      user_made_decision_ = true;
      // Don't record the user action here because there are other ways of
      // triggering DontProceed, like clicking the back button.
      if (is_main_frame_load_pending()) {
        // If the load is pending, we want to close the interstitial and discard
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
          base::EscapeQueryParamValue(request_url().spec(), true).c_str());
      GURL diagnostic_url(diagnostic);
      diagnostic_url =
          google_util::AppendGoogleLocaleParam(diagnostic_url, app_locale());
      controller()->OpenURL(should_open_links_in_new_tab(), diagnostic_url);
      break;
    }
    case CMD_REPORT_PHISHING_ERROR: {
      controller()->metrics_helper()->RecordUserInteraction(
          security_interstitials::MetricsHelper::REPORT_PHISHING_ERROR);
      std::string phishing_error = base::StringPrintf(
          kReportPhishingErrorUrl,
          base::EscapeQueryParamValue(request_url().spec(), true).c_str());
      GURL phishing_error_url(phishing_error);
      phishing_error_url = google_util::AppendGoogleLocaleParam(
          phishing_error_url, app_locale());
      controller()->OpenURL(should_open_links_in_new_tab(), phishing_error_url);
      break;
    }
    case CMD_OPEN_ENHANCED_PROTECTION_SETTINGS: {
      controller()->metrics_helper()->RecordUserInteraction(
          security_interstitials::MetricsHelper::OPEN_ENHANCED_PROTECTION);
      controller()->OpenEnhancedProtectionSettings();
      break;
    }
    case CMD_OPEN_DATE_SETTINGS:
    case CMD_OPEN_LOGIN:
    case CMD_ERROR:
    case CMD_TEXT_FOUND:
    case CMD_TEXT_NOT_FOUND:
    case CMD_CLOSE_INTERSTITIAL_WITHOUT_UI:
    case CMD_REQUEST_SITE_ACCESS_PERMISSION:
      break;
  }
}

void SafeBrowsingLoudErrorUI::PopulateMalwareLoadTimeData(
    base::Value::Dict& load_time_data) {
  load_time_data.Set("phishing", false);
  load_time_data.Set("heading",
                     l10n_util::GetStringUTF16(IDS_SAFEBROWSING_HEADING));
  load_time_data.Set("primaryParagraph", l10n_util::GetStringUTF16(
                                             IDS_MALWARE_V3_PRIMARY_PARAGRAPH));
  load_time_data.Set(
      "explanationParagraph",
      l10n_util::GetStringUTF16(IDS_MALWARE_V3_EXPLANATION_PARAGRAPH));
  load_time_data.Set("finalParagraph", l10n_util::GetStringUTF16(
                                           IDS_MALWARE_V3_PROCEED_PARAGRAPH));
}

void SafeBrowsingLoudErrorUI::PopulateHarmfulLoadTimeData(
    base::Value::Dict& load_time_data) {
  load_time_data.Set("phishing", false);
  load_time_data.Set("heading",
                     l10n_util::GetStringUTF16(IDS_SAFEBROWSING_HEADING));
  load_time_data.Set("primaryParagraph", l10n_util::GetStringUTF16(
                                             IDS_HARMFUL_V3_PRIMARY_PARAGRAPH));
  load_time_data.Set(
      "explanationParagraph",
      l10n_util::GetStringUTF16(IDS_HARMFUL_V3_EXPLANATION_PARAGRAPH));
  load_time_data.Set("finalParagraph", l10n_util::GetStringUTF16(
                                           IDS_HARMFUL_V3_PROCEED_PARAGRAPH));
}

void SafeBrowsingLoudErrorUI::PopulatePhishingLoadTimeData(
    base::Value::Dict& load_time_data) {
  load_time_data.Set("phishing", true);
  load_time_data.Set("heading",
                     l10n_util::GetStringUTF16(IDS_SAFEBROWSING_HEADING));
  load_time_data.Set(
      "primaryParagraph",
      l10n_util::GetStringUTF16(IDS_PHISHING_V4_PRIMARY_PARAGRAPH));
  load_time_data.Set(
      "explanationParagraph",
      l10n_util::GetStringUTF16(IDS_PHISHING_V4_EXPLANATION_PARAGRAPH));
  load_time_data.Set("finalParagraph", l10n_util::GetStringUTF16(
                                           IDS_PHISHING_V4_PROCEED_PARAGRAPH));
}

void SafeBrowsingLoudErrorUI::PopulateExtendedReportingOption(
    base::Value::Dict& load_time_data) {
  bool can_show_extended_reporting_option = CanShowExtendedReportingOption();
  bool can_show_enhanced_protection_message =
      CanShowEnhancedProtectionMessage();
  load_time_data.Set(security_interstitials::kDisplayCheckBox,
                     can_show_extended_reporting_option &&
                         !can_show_enhanced_protection_message);
  if (!can_show_extended_reporting_option) {
    return;
  }

  load_time_data.Set(security_interstitials::kBoxChecked,
                     is_extended_reporting_enabled());
}

void SafeBrowsingLoudErrorUI::PopulateEnhancedProtectionMessage(
    base::Value::Dict& load_time_data) {
  bool can_show_enhanced_protection_message =
      CanShowEnhancedProtectionMessage();
  if (can_show_enhanced_protection_message) {
    controller()->metrics_helper()->RecordUserInteraction(
        security_interstitials::MetricsHelper::SHOW_ENHANCED_PROTECTION);
  }
  load_time_data.Set(security_interstitials::kDisplayEnhancedProtectionMessage,
                     can_show_enhanced_protection_message);
}

void SafeBrowsingLoudErrorUI::PopulateBillingLoadTimeData(
    base::Value::Dict& load_time_data) {
  load_time_data.Set("phishing", false);
  load_time_data.Set("overridable", true);

  load_time_data.Set("tabTitle", l10n_util::GetStringUTF16(IDS_BILLING_TITLE));
  load_time_data.Set("heading", l10n_util::GetStringUTF16(IDS_BILLING_HEADING));
  load_time_data.Set("primaryParagraph",
                     l10n_util::GetStringUTF16(IDS_BILLING_PRIMARY_PARAGRAPH));

  load_time_data.Set("primaryButtonText",
                     l10n_util::GetStringUTF16(IDS_BILLING_PRIMARY_BUTTON));
  load_time_data.Set("proceedButtonText",
                     l10n_util::GetStringUTF16(IDS_BILLING_PROCEED_BUTTON));

  load_time_data.Set("openDetails", "");
  load_time_data.Set("closeDetails", "");
  load_time_data.Set("explanationParagraph", "");
  load_time_data.Set("finalParagraph", "");
}

void SafeBrowsingLoudErrorUI::UpdateInterstitialInteractionData(
    SecurityInterstitialCommand command) {
  int new_occurrence_count = 1;
  int64_t new_first_timestamp =
      base::Time::Now().InMillisecondsSinceUnixEpoch();
  int64_t new_last_timestamp = base::Time::Now().InMillisecondsSinceUnixEpoch();
  // If this is not the first occurrence, use data in the map for correct
  // occurrence and first timestamp values.
  if (auto interaction_data = interstitial_interaction_data_->find(command);
      interaction_data != interstitial_interaction_data_->end()) {
    new_occurrence_count += interaction_data->second.occurrence_count;
    new_first_timestamp = interaction_data->second.first_timestamp;
  }
  interstitial_interaction_data_->insert_or_assign(
      command,
      InterstitialInteractionDetails(new_occurrence_count, new_first_timestamp,
                                     new_last_timestamp));
}

int SafeBrowsingLoudErrorUI::GetHTMLTemplateId() const {
  if (base::FeatureList::IsEnabled(safe_browsing::kEnhancedSafeBrowsingPromo)) {
    return IDR_SECURITY_INTERSTITIAL_WITHOUT_PROMO_HTML;
  }
  return IDR_SECURITY_INTERSTITIAL_HTML;
}

}  // namespace security_interstitials
