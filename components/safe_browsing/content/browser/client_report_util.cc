// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/safe_browsing/content/browser/client_report_util.h"
#include "components/security_interstitials/content/unsafe_resource_util.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/navigation_entry.h"

namespace safe_browsing::client_report_utils {

ClientSafeBrowsingReportRequest::ReportType GetReportTypeFromSBThreatType(
    SBThreatType threat_type) {
  switch (threat_type) {
    case SB_THREAT_TYPE_URL_PHISHING:
      return ClientSafeBrowsingReportRequest::URL_PHISHING;
    case SB_THREAT_TYPE_URL_MALWARE:
      return ClientSafeBrowsingReportRequest::URL_MALWARE;
    case SB_THREAT_TYPE_URL_UNWANTED:
      return ClientSafeBrowsingReportRequest::URL_UNWANTED;
    case SB_THREAT_TYPE_URL_CLIENT_SIDE_PHISHING:
      return ClientSafeBrowsingReportRequest::URL_CLIENT_SIDE_PHISHING;
    case SB_THREAT_TYPE_URL_CLIENT_SIDE_MALWARE:
      return ClientSafeBrowsingReportRequest::URL_CLIENT_SIDE_MALWARE;
    case SB_THREAT_TYPE_BLOCKED_AD_POPUP:
      return ClientSafeBrowsingReportRequest::BLOCKED_AD_POPUP;
    case SB_THREAT_TYPE_AD_SAMPLE:
      return ClientSafeBrowsingReportRequest::AD_SAMPLE;
    case SB_THREAT_TYPE_BLOCKED_AD_REDIRECT:
      return ClientSafeBrowsingReportRequest::BLOCKED_AD_REDIRECT;
    case SB_THREAT_TYPE_SAVED_PASSWORD_REUSE:
    case SB_THREAT_TYPE_SIGNED_IN_SYNC_PASSWORD_REUSE:
    case SB_THREAT_TYPE_SIGNED_IN_NON_SYNC_PASSWORD_REUSE:
    case SB_THREAT_TYPE_ENTERPRISE_PASSWORD_REUSE:
      return ClientSafeBrowsingReportRequest::URL_PASSWORD_PROTECTION_PHISHING;
    case SB_THREAT_TYPE_SUSPICIOUS_SITE:
      return ClientSafeBrowsingReportRequest::URL_SUSPICIOUS;
    case SB_THREAT_TYPE_BILLING:
      return ClientSafeBrowsingReportRequest::BILLING;
    case SB_THREAT_TYPE_APK_DOWNLOAD:
      return ClientSafeBrowsingReportRequest::APK_DOWNLOAD;
    case SB_THREAT_TYPE_UNUSED:
    case SB_THREAT_TYPE_SAFE:
    case SB_THREAT_TYPE_URL_BINARY_MALWARE:
    case SB_THREAT_TYPE_EXTENSION:
    case SB_THREAT_TYPE_BLOCKLISTED_RESOURCE:
    case SB_THREAT_TYPE_API_ABUSE:
    case SB_THREAT_TYPE_SUBRESOURCE_FILTER:
    case SB_THREAT_TYPE_CSD_ALLOWLIST:
    case SB_THREAT_TYPE_HIGH_CONFIDENCE_ALLOWLIST:
    case DEPRECATED_SB_THREAT_TYPE_URL_PASSWORD_PROTECTION_PHISHING:
    case SB_THREAT_TYPE_MANAGED_POLICY_WARN:
    case SB_THREAT_TYPE_MANAGED_POLICY_BLOCK:
      // Gated by SafeBrowsingBlockingPage::ShouldReportThreatDetails.
      NOTREACHED() << "We should not send report for threat type: "
                   << threat_type;
      return ClientSafeBrowsingReportRequest::UNKNOWN;
  }
}

ClientSafeBrowsingReportRequest::UrlRequestDestination
GetUrlRequestDestinationFromMojomRequestDestination(
    network::mojom::RequestDestination request_destination) {
  switch (request_destination) {
    case network::mojom::RequestDestination::kEmpty:
      return ClientSafeBrowsingReportRequest::EMPTY;
    case network::mojom::RequestDestination::kAudio:
      return ClientSafeBrowsingReportRequest::AUDIO;
    case network::mojom::RequestDestination::kAudioWorklet:
      return ClientSafeBrowsingReportRequest::AUDIO_WORKLET;
    case network::mojom::RequestDestination::kDocument:
      return ClientSafeBrowsingReportRequest::DOCUMENT;
    case network::mojom::RequestDestination::kEmbed:
      return ClientSafeBrowsingReportRequest::EMBED;
    case network::mojom::RequestDestination::kFont:
      return ClientSafeBrowsingReportRequest::FONT;
    case network::mojom::RequestDestination::kFrame:
      return ClientSafeBrowsingReportRequest::FRAME;
    case network::mojom::RequestDestination::kIframe:
      return ClientSafeBrowsingReportRequest::IFRAME;
    case network::mojom::RequestDestination::kImage:
      return ClientSafeBrowsingReportRequest::IMAGE;
    case network::mojom::RequestDestination::kManifest:
      return ClientSafeBrowsingReportRequest::MANIFEST;
    case network::mojom::RequestDestination::kObject:
      return ClientSafeBrowsingReportRequest::OBJECT;
    case network::mojom::RequestDestination::kPaintWorklet:
      return ClientSafeBrowsingReportRequest::PAINT_WORKLET;
    case network::mojom::RequestDestination::kReport:
      return ClientSafeBrowsingReportRequest::REPORT;
    case network::mojom::RequestDestination::kScript:
      return ClientSafeBrowsingReportRequest::SCRIPT;
    case network::mojom::RequestDestination::kServiceWorker:
      return ClientSafeBrowsingReportRequest::SERVICE_WORKER;
    case network::mojom::RequestDestination::kSharedWorker:
      return ClientSafeBrowsingReportRequest::SHARED_WORKER;
    case network::mojom::RequestDestination::kStyle:
      return ClientSafeBrowsingReportRequest::STYLE;
    case network::mojom::RequestDestination::kTrack:
      return ClientSafeBrowsingReportRequest::TRACK;
    case network::mojom::RequestDestination::kVideo:
      return ClientSafeBrowsingReportRequest::VIDEO;
    case network::mojom::RequestDestination::kWebBundle:
      return ClientSafeBrowsingReportRequest::WEB_BUNDLE;
    case network::mojom::RequestDestination::kWorker:
      return ClientSafeBrowsingReportRequest::WORKER;
    case network::mojom::RequestDestination::kXslt:
      return ClientSafeBrowsingReportRequest::XSLT;
    case network::mojom::RequestDestination::kFencedframe:
      return ClientSafeBrowsingReportRequest::FENCED_FRAME;
    case network::mojom::RequestDestination::kWebIdentity:
      return ClientSafeBrowsingReportRequest::WEB_IDENTITY;
    case network::mojom::RequestDestination::kDictionary:
      return ClientSafeBrowsingReportRequest::DICTIONARY;
  }
}

// Helper function that converts SecurityInterstitialCommand to CSBRR
// SecurityInterstitialInteraction.
ClientSafeBrowsingReportRequest::InterstitialInteraction::
    SecurityInterstitialInteraction
    GetSecurityInterstitialInteractionFromCommand(
        security_interstitials::SecurityInterstitialCommand command) {
  switch (command) {
    case security_interstitials::CMD_DONT_PROCEED:
      return ClientSafeBrowsingReportRequest::InterstitialInteraction::
          CMD_DONT_PROCEED;
    case security_interstitials::CMD_PROCEED:
      return ClientSafeBrowsingReportRequest::InterstitialInteraction::
          CMD_PROCEED;
    case security_interstitials::CMD_SHOW_MORE_SECTION:
      return ClientSafeBrowsingReportRequest::InterstitialInteraction::
          CMD_SHOW_MORE_SECTION;
    case security_interstitials::CMD_OPEN_HELP_CENTER:
      return ClientSafeBrowsingReportRequest::InterstitialInteraction::
          CMD_OPEN_HELP_CENTER;
    case security_interstitials::CMD_OPEN_DIAGNOSTIC:
      return ClientSafeBrowsingReportRequest::InterstitialInteraction::
          CMD_OPEN_DIAGNOSTIC;
    case security_interstitials::CMD_RELOAD:
      return ClientSafeBrowsingReportRequest::InterstitialInteraction::
          CMD_RELOAD;
    case security_interstitials::CMD_OPEN_DATE_SETTINGS:
      return ClientSafeBrowsingReportRequest::InterstitialInteraction::
          CMD_OPEN_DATE_SETTINGS;
    case security_interstitials::CMD_OPEN_LOGIN:
      return ClientSafeBrowsingReportRequest::InterstitialInteraction::
          CMD_OPEN_LOGIN;
    case security_interstitials::CMD_DO_REPORT:
      return ClientSafeBrowsingReportRequest::InterstitialInteraction::
          CMD_DO_REPORT;
    case security_interstitials::CMD_DONT_REPORT:
      return ClientSafeBrowsingReportRequest::InterstitialInteraction::
          CMD_DONT_REPORT;
    case security_interstitials::CMD_OPEN_REPORTING_PRIVACY:
      return ClientSafeBrowsingReportRequest::InterstitialInteraction::
          CMD_OPEN_REPORTING_PRIVACY;
    case security_interstitials::CMD_OPEN_WHITEPAPER:
      return ClientSafeBrowsingReportRequest::InterstitialInteraction::
          CMD_OPEN_WHITEPAPER;
    case security_interstitials::CMD_REPORT_PHISHING_ERROR:
      return ClientSafeBrowsingReportRequest::InterstitialInteraction::
          CMD_REPORT_PHISHING_ERROR;
    case security_interstitials::CMD_OPEN_ENHANCED_PROTECTION_SETTINGS:
      return ClientSafeBrowsingReportRequest::InterstitialInteraction::
          CMD_OPEN_ENHANCED_PROTECTION_SETTINGS;
    case security_interstitials::CMD_CLOSE_INTERSTITIAL_WITHOUT_UI:
      return ClientSafeBrowsingReportRequest::InterstitialInteraction::
          CMD_CLOSE_INTERSTITIAL_WITHOUT_UI;
    case security_interstitials::CMD_TEXT_FOUND:
    case security_interstitials::CMD_TEXT_NOT_FOUND:
    case security_interstitials::CMD_ERROR:
    case security_interstitials::CMD_REQUEST_SITE_ACCESS_PERMISSION:
      break;
  }
  return ClientSafeBrowsingReportRequest::InterstitialInteraction::UNSPECIFIED;
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
          GetNavigationEntryForResource(resource);
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
          GetNavigationEntryForResource(resource);
      if (nav_entry) {
        referrer_url = nav_entry->GetReferrer().url;
      }
    }
  }
  return referrer_url;
}

void FillReportBasicResourceDetails(
    ClientSafeBrowsingReportRequest* report,
    const security_interstitials::UnsafeResource& resource) {
  if (IsReportableUrl(resource.url)) {
    report->set_url(resource.url.spec());
    report->set_type(GetReportTypeFromSBThreatType(resource.threat_type));
    report->set_url_request_destination(
        GetUrlRequestDestinationFromMojomRequestDestination(
            resource.request_destination));
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
}

void FillInterstitialInteractionsHelper(
    ClientSafeBrowsingReportRequest* report,
    security_interstitials::InterstitialInteractionMap*
        interstitial_interactions) {
  if (report == nullptr || interstitial_interactions == nullptr) {
    return;
  }
  for (auto const& interaction : *interstitial_interactions) {
    // Create InterstitialInteraction object.
    ClientSafeBrowsingReportRequest::InterstitialInteraction
        new_interstitial_interaction;
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
