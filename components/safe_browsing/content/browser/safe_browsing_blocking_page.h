// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Classes for managing the SafeBrowsing interstitial pages.
//
// When a user is about to visit a page the SafeBrowsing system has deemed to
// be malicious, either as malware or a phishing page, we show an interstitial
// page with some options (go back, continue) to give the user a chance to avoid
// the harmful page.
//
// The SafeBrowsingBlockingPage is created by the SafeBrowsingUIManager on the
// UI thread when we've determined that a page is malicious. The operation of
// the blocking page occurs on the UI thread, where it waits for the user to
// make a decision about what to do: either go back or continue on.
//
// The blocking page forwards the result of the user's choice back to the
// SafeBrowsingUIManager so that we can cancel the request for the new page,
// or allow it to continue.
//
// A web page may contain several resources flagged as malware/phishing.  This
// results into more than one interstitial being shown.  On the first unsafe
// resource received we show an interstitial.  Any subsequent unsafe resource
// notifications while the first interstitial is showing is queued.  If the user
// decides to proceed in the first interstitial, we display all queued unsafe
// resources in a new interstitial.

#ifndef COMPONENTS_SAFE_BROWSING_CONTENT_BROWSER_SAFE_BROWSING_BLOCKING_PAGE_H_
#define COMPONENTS_SAFE_BROWSING_CONTENT_BROWSER_SAFE_BROWSING_BLOCKING_PAGE_H_

#include <map>

#include "base/gtest_prod_util.h"
#include "base/memory/raw_ptr.h"
#include "components/safe_browsing/content/browser/base_blocking_page.h"
#include "components/safe_browsing/content/browser/base_ui_manager.h"

namespace history {
class HistoryService;
}

namespace network {
class SharedURLLoaderFactory;
}

namespace weblayer {
class WebLayerSafeBrowsingBlockingPageFactory;
}

namespace safe_browsing {

class SafeBrowsingNavigationObserverManager;
class SafeBrowsingMetricsCollector;
class ThreatDetails;
class TriggerManager;

class SafeBrowsingBlockingPage : public BaseBlockingPage {
 public:
  typedef security_interstitials::BaseSafeBrowsingErrorUI
      BaseSafeBrowsingErrorUI;
  // Interstitial type, used in tests.
  static const security_interstitials::SecurityInterstitialPage::TypeID
      kTypeForTesting;

  SafeBrowsingBlockingPage(const SafeBrowsingBlockingPage&) = delete;
  SafeBrowsingBlockingPage& operator=(const SafeBrowsingBlockingPage&) = delete;

  ~SafeBrowsingBlockingPage() override;

  // SecurityInterstitialPage method:
  security_interstitials::SecurityInterstitialPage::TypeID GetTypeForTesting()
      override;

 protected:
  friend class ChromeSafeBrowsingBlockingPageFactory;
  friend class weblayer::WebLayerSafeBrowsingBlockingPageFactory;
  friend class SafeBrowsingBlockingPageTestBase;
  friend class SafeBrowsingBlockingPageBrowserTest;
  friend class SafeBrowsingBlockingQuietPageFactoryImpl;
  friend class SafeBrowsingBlockingQuietPageTest;
  FRIEND_TEST_ALL_PREFIXES(SafeBrowsingBlockingPageTest,
                           ProceedThenDontProceed);
  FRIEND_TEST_ALL_PREFIXES(SafeBrowsingBlockingPageTest,
                           MalwareReportsDisabled);
  FRIEND_TEST_ALL_PREFIXES(SafeBrowsingBlockingPageTest,
                           MalwareReportsToggling);
  FRIEND_TEST_ALL_PREFIXES(SafeBrowsingBlockingPageTest,
                           ExtendedReportingNotShownOnSecurePage);
  FRIEND_TEST_ALL_PREFIXES(SafeBrowsingBlockingPageTest,
                           MalwareReportsTransitionDisabled);
  FRIEND_TEST_ALL_PREFIXES(SafeBrowsingBlockingPageIncognitoTest,
                           ExtendedReportingNotShownInIncognito);
  FRIEND_TEST_ALL_PREFIXES(SafeBrowsingBlockingPageTest,
                           ExtendedReportingNotShownNotAllowExtendedReporting);
  FRIEND_TEST_ALL_PREFIXES(SafeBrowsingBlockingPageTest,
                           ExtendedReportingNotShownForEnhancedProtection);
  FRIEND_TEST_ALL_PREFIXES(SafeBrowsingBlockingPageTest, BillingPage);

  void UpdateReportingPref();  // Used for the transition from old to new pref.

  // Don't instantiate this class directly, use CreateBlockingPage instead.
  // |trigger_manager| may be null, in which case reporting will not occur.
  SafeBrowsingBlockingPage(
      BaseUIManager* ui_manager,
      content::WebContents* web_contents,
      const GURL& main_frame_url,
      const UnsafeResourceList& unsafe_resources,
      std::unique_ptr<
          security_interstitials::SecurityInterstitialControllerClient>
          controller_client,
      const BaseSafeBrowsingErrorUI::SBErrorDisplayOptions& display_options,
      bool should_trigger_reporting,
      history::HistoryService* history_service,
      SafeBrowsingNavigationObserverManager* navigation_observer_manager,
      SafeBrowsingMetricsCollector* metrics_collector,
      TriggerManager* trigger_manager,
      bool is_proceed_anyway_disabled,
      bool is_safe_browsing_surveys_enabled,
      base::OnceCallback<void(bool, SBThreatType)>
          trust_safety_sentiment_service_trigger,
      base::OnceCallback<void(bool, SBThreatType)>
          ignore_auto_revocation_notifications_trigger,
      network::SharedURLLoaderFactory* url_loader_for_testing = nullptr);

  // Called when an interstitial is closed, either due to a click through or a
  // navigation elsewhere.
  void OnInterstitialClosing() override;

  // Called when the trigger manager can't send the report because the threat
  // details are unavailable. This typically happens when the user closes the
  // tab without using the interstitial UI.
  void SendFallbackReport(
      const security_interstitials::UnsafeResource resource,
      bool did_proceed,
      int num_visits,
      security_interstitials::InterstitialInteractionMap* interactions,
      bool is_hats_candidate);

  // Called when the interstitial is going away. If there is a
  // pending threat details object, we look at the user's
  // preferences, and if the option to send threat details is
  // enabled, the report is scheduled to be sent on the |ui_manager_|.
  void FinishThreatDetails(const base::TimeDelta& delay,
                           bool did_proceed,
                           int num_visits) override;

  // Log UKM for the user bypassing a safe browsing interstitial.
  void LogSafeBrowsingInterstitialBypassedUKM(
      content::WebContents* web_contents);

  // Log UKM for the safe browsing interstitial being shown to the user.
  void LogSafeBrowsingInterstitialShownUKM(content::WebContents* web_contents4);

  // Whether ThreatDetails collection is in progress as part of this
  // interstitial.
  bool threat_details_in_progress_;

  // The threat source that triggers the blocking page.
  ThreatSource threat_source_;

  // The threat type of the resource that triggered the blocking page.
  SBThreatType threat_type_;

 private:
  raw_ptr<history::HistoryService> history_service_ = nullptr;
  raw_ptr<SafeBrowsingNavigationObserverManager> navigation_observer_manager_ =
      nullptr;
  raw_ptr<SafeBrowsingMetricsCollector> metrics_collector_ = nullptr;
  raw_ptr<TriggerManager> trigger_manager_ = nullptr;
  std::unique_ptr<security_interstitials::InterstitialInteractionMap>
      interstitial_interactions_;
  // Whether the user has SafeBrowsingProceedAnywayDisabled enabled.
  bool is_proceed_anyway_disabled_;
  // Whether the user has SafeBrowsingSurveysEnabled enabled.
  bool is_safe_browsing_surveys_enabled_;
  // Triggers trust and safety sentiment service when interstitial closes.
  base::OnceCallback<void(bool, SBThreatType)>
      trust_safety_sentiment_service_trigger_ = base::NullCallback();
  // Triggers callback for ignoring the url for future auto abusive notification
  // revocation.
  base::OnceCallback<void(bool, SBThreatType)>
      ignore_auto_revocation_notifications_trigger_ = base::NullCallback();
  // Timestamp of when the safe browsing blocking page was shown to the user.
  int64_t warning_shown_ts_;
};

}  // namespace safe_browsing

#endif  // COMPONENTS_SAFE_BROWSING_CONTENT_BROWSER_SAFE_BROWSING_BLOCKING_PAGE_H_
