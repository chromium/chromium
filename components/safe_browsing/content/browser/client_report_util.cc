// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/safe_browsing/content/browser/client_report_util.h"
#include "components/safe_browsing/content/browser/unsafe_resource_util.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/navigation_entry.h"

namespace safe_browsing::client_report_utils {

CSBRR::SafeBrowsingUrlApiType GetUrlApiTypeForThreatSource(
    safe_browsing::ThreatSource source) {
  switch (source) {
    case safe_browsing::ThreatSource::LOCAL_PVER4:
      return CSBRR::PVER4_NATIVE;
    case safe_browsing::ThreatSource::URL_REAL_TIME_CHECK:
      return CSBRR::REAL_TIME;
    case safe_browsing::ThreatSource::NATIVE_PVER5_REAL_TIME:
      return CSBRR::PVER5_NATIVE_REAL_TIME;
    case safe_browsing::ThreatSource::ANDROID_SAFEBROWSING_REAL_TIME:
      return CSBRR::ANDROID_SAFEBROWSING_REAL_TIME;
    case safe_browsing::ThreatSource::ANDROID_SAFEBROWSING:
      return CSBRR::ANDROID_SAFEBROWSING;
    case safe_browsing::ThreatSource::CLIENT_SIDE_DETECTION:
      return CSBRR::CLIENT_SIDE_DETECTION;
    case safe_browsing::ThreatSource::UNKNOWN:
      return CSBRR::SAFE_BROWSING_URL_API_TYPE_UNSPECIFIED;
  }
}

CSBRR::ReportType GetReportTypeFromSBThreatType(SBThreatType threat_type) {
  using enum SBThreatType;

  switch (threat_type) {
    case SB_THREAT_TYPE_URL_PHISHING:
      return CSBRR::URL_PHISHING;
    case SB_THREAT_TYPE_URL_MALWARE:
      return CSBRR::URL_MALWARE;
    case SB_THREAT_TYPE_URL_UNWANTED:
      return CSBRR::URL_UNWANTED;
    case SB_THREAT_TYPE_URL_CLIENT_SIDE_PHISHING:
      return CSBRR::URL_CLIENT_SIDE_PHISHING;
    case SB_THREAT_TYPE_BLOCKED_AD_POPUP:
      return CSBRR::BLOCKED_AD_POPUP;
    case SB_THREAT_TYPE_AD_SAMPLE:
      return CSBRR::AD_SAMPLE;
    case SB_THREAT_TYPE_BLOCKED_AD_REDIRECT:
      return CSBRR::BLOCKED_AD_REDIRECT;
    case SB_THREAT_TYPE_SAVED_PASSWORD_REUSE:
    case SB_THREAT_TYPE_SIGNED_IN_SYNC_PASSWORD_REUSE:
    case SB_THREAT_TYPE_SIGNED_IN_NON_SYNC_PASSWORD_REUSE:
    case SB_THREAT_TYPE_ENTERPRISE_PASSWORD_REUSE:
      return CSBRR::URL_PASSWORD_PROTECTION_PHISHING;
    case SB_THREAT_TYPE_SUSPICIOUS_SITE:
      return CSBRR::URL_SUSPICIOUS;
    case SB_THREAT_TYPE_BILLING:
      return CSBRR::BILLING;
    case SB_THREAT_TYPE_APK_DOWNLOAD:
      return CSBRR::APK_DOWNLOAD;
    case SB_THREAT_TYPE_UNUSED:
    case SB_THREAT_TYPE_SAFE:
    case SB_THREAT_TYPE_URL_BINARY_MALWARE:
    case SB_THREAT_TYPE_EXTENSION:
    case SB_THREAT_TYPE_API_ABUSE:
    case SB_THREAT_TYPE_SUBRESOURCE_FILTER:
    case SB_THREAT_TYPE_CSD_ALLOWLIST:
    case SB_THREAT_TYPE_HIGH_CONFIDENCE_ALLOWLIST:
    case DEPRECATED_SB_THREAT_TYPE_URL_PASSWORD_PROTECTION_PHISHING:
    case DEPRECATED_SB_THREAT_TYPE_URL_CLIENT_SIDE_MALWARE:
    case SB_THREAT_TYPE_MANAGED_POLICY_WARN:
    case SB_THREAT_TYPE_MANAGED_POLICY_BLOCK:
      // Gated by SafeBrowsingBlockingPage::ShouldReportThreatDetails.
      NOTREACHED_IN_MIGRATION() << "We should not send report for threat type: "
                                << static_cast<int>(threat_type);
      return CSBRR::UNKNOWN;
  }
}

CSBRR::WarningShownInfo::WarningUXType GetWarningUXTypeFromSBThreatType(
    SBThreatType threat_type) {
  using enum SBThreatType;

  switch (threat_type) {
    case SB_THREAT_TYPE_URL_PHISHING:
      return CSBRR::WarningShownInfo::PHISHING_INTERSTITIAL;
    case SB_THREAT_TYPE_URL_MALWARE:
      return CSBRR::WarningShownInfo::MALWARE_INTERSTITIAL;
    case SB_THREAT_TYPE_URL_UNWANTED:
      return CSBRR::WarningShownInfo::UWS_INTERSTITIAL;
    case SB_THREAT_TYPE_URL_CLIENT_SIDE_PHISHING:
      return CSBRR::WarningShownInfo::CLIENT_SIDE_PHISHING_INTERSTITIAL;
    case SB_THREAT_TYPE_BILLING:
      return CSBRR::WarningShownInfo::BILLING_INTERSTITIAL;
    case SB_THREAT_TYPE_BLOCKED_AD_POPUP:
    case SB_THREAT_TYPE_AD_SAMPLE:
    case SB_THREAT_TYPE_BLOCKED_AD_REDIRECT:
    case SB_THREAT_TYPE_SAVED_PASSWORD_REUSE:
    case SB_THREAT_TYPE_SIGNED_IN_SYNC_PASSWORD_REUSE:
    case SB_THREAT_TYPE_SIGNED_IN_NON_SYNC_PASSWORD_REUSE:
    case SB_THREAT_TYPE_ENTERPRISE_PASSWORD_REUSE:
    case SB_THREAT_TYPE_SUSPICIOUS_SITE:
    case SB_THREAT_TYPE_APK_DOWNLOAD:
    case SB_THREAT_TYPE_UNUSED:
    case SB_THREAT_TYPE_SAFE:
    case SB_THREAT_TYPE_URL_BINARY_MALWARE:
    case SB_THREAT_TYPE_EXTENSION:
    case SB_THREAT_TYPE_API_ABUSE:
    case SB_THREAT_TYPE_SUBRESOURCE_FILTER:
    case SB_THREAT_TYPE_CSD_ALLOWLIST:
    case SB_THREAT_TYPE_HIGH_CONFIDENCE_ALLOWLIST:
    case DEPRECATED_SB_THREAT_TYPE_URL_PASSWORD_PROTECTION_PHISHING:
    case DEPRECATED_SB_THREAT_TYPE_URL_CLIENT_SIDE_MALWARE:
    case SB_THREAT_TYPE_MANAGED_POLICY_WARN:
    case SB_THREAT_TYPE_MANAGED_POLICY_BLOCK:
      NOTREACHED_IN_MIGRATION() << "We should not send report for threat type: "
                                << static_cast<int>(threat_type);
      return CSBRR::WarningShownInfo::UNKNOWN;
  }
}

// Helper function that converts SecurityInterstitialCommand to CSBRR
// SecurityInterstitialInteraction.
CSBRR::InterstitialInteraction::SecurityInterstitialInteraction
GetSecurityInterstitialInteractionFromCommand(
    security_interstitials::SecurityInterstitialCommand command) {
  switch (command) {
    case security_interstitials::CMD_DONT_PROCEED:
      return CSBRR::InterstitialInteraction::CMD_DONT_PROCEED;
    case security_interstitials::CMD_PROCEED:
      return CSBRR::InterstitialInteraction::CMD_PROCEED;
    case security_interstitials::CMD_SHOW_MORE_SECTION:
      return CSBRR::InterstitialInteraction::CMD_SHOW_MORE_SECTION;
    case security_interstitials::CMD_OPEN_HELP_CENTER:
      return CSBRR::InterstitialInteraction::CMD_OPEN_HELP_CENTER;
    case security_interstitials::CMD_OPEN_DIAGNOSTIC:
      return CSBRR::InterstitialInteraction::CMD_OPEN_DIAGNOSTIC;
    case security_interstitials::CMD_RELOAD:
      return CSBRR::InterstitialInteraction::CMD_RELOAD;
    case security_interstitials::CMD_OPEN_DATE_SETTINGS:
      return CSBRR::InterstitialInteraction::CMD_OPEN_DATE_SETTINGS;
    case security_interstitials::CMD_OPEN_LOGIN:
      return CSBRR::InterstitialInteraction::CMD_OPEN_LOGIN;
    case security_interstitials::CMD_DO_REPORT:
      return CSBRR::InterstitialInteraction::CMD_DO_REPORT;
    case security_interstitials::CMD_DONT_REPORT:
      return CSBRR::InterstitialInteraction::CMD_DONT_REPORT;
    case security_interstitials::CMD_OPEN_REPORTING_PRIVACY:
      return CSBRR::InterstitialInteraction::CMD_OPEN_REPORTING_PRIVACY;
    case security_interstitials::CMD_OPEN_WHITEPAPER:
      return CSBRR::InterstitialInteraction::CMD_OPEN_WHITEPAPER;
    case security_interstitials::CMD_REPORT_PHISHING_ERROR:
      return CSBRR::InterstitialInteraction::CMD_REPORT_PHISHING_ERROR;
    case security_interstitials::CMD_OPEN_ENHANCED_PROTECTION_SETTINGS:
      return CSBRR::InterstitialInteraction::
          CMD_OPEN_ENHANCED_PROTECTION_SETTINGS;
    case security_interstitials::CMD_CLOSE_INTERSTITIAL_WITHOUT_UI:
      return CSBRR::InterstitialInteraction::CMD_CLOSE_INTERSTITIAL_WITHOUT_UI;
    case security_interstitials::CMD_TEXT_FOUND:
    case security_interstitials::CMD_TEXT_NOT_FOUND:
    case security_interstitials::CMD_ERROR:
    case security_interstitials::CMD_REQUEST_SITE_ACCESS_PERMISSION:
      break;
  }
  return CSBRR::InterstitialInteraction::UNSPECIFIED;
}

bool IsReportableUrl(const GURL& url) {
  // TODO(panayiotis): also skip internal urls.
  return url.SchemeIs("http") || url.SchemeIs("https");
}

GURL GetPageUrl(const security_interstitials::UnsafeResource& resource) {
  GURL page_url;
  if (!resource.navigation_url.is_empty()) {
    page_url = resource.navigation_url;
  } else {
    // |GetNavigationEntryForResource| can only be called from the UI thread.
    if (content::BrowserThread::CurrentlyOn(content::BrowserThread::UI)) {
      content::NavigationEntry* nav_entry =
          unsafe_resource_util::GetNavigationEntryForResource(resource);
      if (nav_entry) {
        page_url = nav_entry->GetURL();
      }
    }
  }
  return page_url;
}

GURL GetReferrerUrl(const security_interstitials::UnsafeResource& resource) {
  GURL referrer_url;
  if (!resource.navigation_url.is_empty()) {
    referrer_url = resource.referrer_url;
  } else {
    // |GetNavigationEntryForResource| can only be called from the UI thread.
    if (content::BrowserThread::CurrentlyOn(content::BrowserThread::UI)) {
      content::NavigationEntry* nav_entry =
          unsafe_resource_util::GetNavigationEntryForResource(resource);
      if (nav_entry) {
        referrer_url = nav_entry->GetReferrer().url;
      }
    }
  }
  return referrer_url;
}

void FillReportBasicResourceDetails(
    CSBRR* report,
    const security_interstitials::UnsafeResource& resource) {
  if (IsReportableUrl(resource.url)) {
    report->set_url(resource.url.spec());
    report->set_type(GetReportTypeFromSBThreatType(resource.threat_type));
    report->set_url_request_destination(CSBRR::DOCUMENT);
  }

  // With committed interstitials, the information is pre-filled into the
  // UnsafeResource, since the navigation entry we have at this point is for the
  // navigation to the interstitial, and the entry with the page details get
  // destroyed when leaving the interstitial.
  GURL page_url = GetPageUrl(resource);
  GURL referrer_url = GetReferrerUrl(resource);
  if (IsReportableUrl(page_url)) {
    report->set_page_url(page_url.spec());
  }
  if (IsReportableUrl(referrer_url)) {
    report->set_referrer_url(referrer_url.spec());
  }
  report->mutable_client_properties()->set_url_api_type(
      client_report_utils::GetUrlApiTypeForThreatSource(
          resource.threat_source));
  report->mutable_client_properties()->set_is_async_check(
      resource.is_async_check);
}

void FillInterstitialInteractionsHelper(
    CSBRR* report,
    security_interstitials::InterstitialInteractionMap*
        interstitial_interactions) {
  if (report == nullptr || interstitial_interactions == nullptr) {
    return;
  }
  for (auto const& interaction : *interstitial_interactions) {
    // Create InterstitialInteraction object.
    CSBRR::InterstitialInteraction new_interstitial_interaction;
    new_interstitial_interaction.set_security_interstitial_interaction(
        GetSecurityInterstitialInteractionFromCommand(interaction.first));
    new_interstitial_interaction.set_occurrence_count(
        interaction.second.occurrence_count);
    new_interstitial_interaction.set_first_interaction_timestamp_msec(
        interaction.second.first_timestamp);
    new_interstitial_interaction.set_last_interaction_timestamp_msec(
        interaction.second.last_timestamp);
    // Add the InterstitialInteraction object to report's
    // interstitial_interactions.
    report->mutable_interstitial_interactions()->Add()->Swap(
        &new_interstitial_interaction);
  }
}

}  // namespace safe_browsing::client_report_utils
