// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// The Safe Browsing service is responsible for downloading anti-phishing and
// anti-malware tables and checking urls against them.

#ifndef COMPONENTS_SAFE_BROWSING_CONTENT_BROWSER_UI_MANAGER_H_
#define COMPONENTS_SAFE_BROWSING_CONTENT_BROWSER_UI_MANAGER_H_

#include <string>
#include <vector>

#include "base/functional/callback.h"
#include "base/observer_list.h"
#include "components/safe_browsing/content/browser/base_ui_manager.h"
#include "components/safe_browsing/content/browser/safe_browsing_blocking_page_factory.h"
#include "components/security_interstitials/content/security_interstitial_page.h"
#include "components/security_interstitials/core/unsafe_resource.h"

class GURL;

namespace content {
class WebContents;
}  // namespace content

namespace history {
class HistoryService;
}  // namespace history

namespace prerender {
class NoStatePrefetchContents;
}

namespace safe_browsing {

class PingManager;

struct HitReport;

// Construction needs to happen on the main thread.
class SafeBrowsingUIManager : public BaseUIManager {
 public:
  // Observer class can be used to get notified when a SafeBrowsing hit
  // is found.
  class Observer {
   public:
    Observer(const Observer&) = delete;
    Observer& operator=(const Observer&) = delete;

    // Called when |resource| is classified as unsafe by SafeBrowsing, and is
    // not allowlisted.
    // The |resource| must not be accessed after OnSafeBrowsingHit returns.
    // This method will be called on the UI thread.
    virtual void OnSafeBrowsingHit(const UnsafeResource& resource) = 0;

   protected:
    Observer() {}
    virtual ~Observer() {}
  };

  // Interface via which the embedder supplies contextual information to
  // SafeBrowsingUIManager.
  class Delegate {
   public:
    Delegate() = default;
    virtual ~Delegate() = default;

    Delegate(const Delegate&) = delete;
    Delegate& operator=(const Delegate&) = delete;

    // Returns the locale used by the application. It is the IETF language tag,
    // defined in BCP 47. The region subtag is not included when it adds no
    // distinguishing information to the language tag (e.g. both "en-US" and
    // "fr" are correct here).
    virtual std::string GetApplicationLocale() = 0;

    // Notifies the embedder that given events occurred so that the embedder can
    // trigger corresponding extension events if desired. This triggering is
    // optional (e.g., not all embedders support extensions, even those who do
    // might not wish to trigger extension events in incognito mode, etc).
    virtual void TriggerSecurityInterstitialShownExtensionEventIfDesired(
        content::WebContents* web_contents,
        const GURL& page_url,
        const std::string& reason,
        int net_error_code) = 0;
    virtual void TriggerSecurityInterstitialProceededExtensionEventIfDesired(
        content::WebContents* web_contents,
        const GURL& page_url,
        const std::string& reason,
        int net_error_code) = 0;
#if !BUILDFLAG(IS_ANDROID)
    virtual void TriggerUrlFilteringInterstitialExtensionEventIfDesired(
        content::WebContents* web_contents,
        const GURL& page_url,
        const std::string& threat_type,
        safe_browsing::RTLookupResponse rt_lookup_response) = 0;
#endif

    // Gets the NoStatePrefetchContents instance associated with |web_contents|
    // if one exists (i.e., if |web_contents| is being prerendered).
    virtual prerender::NoStatePrefetchContents*
    GetNoStatePrefetchContentsIfExists(content::WebContents* web_contents) = 0;

    // Returns true if |web_contents| is hosting a page for an extension.
    virtual bool IsHostingExtension(content::WebContents* web_contents) = 0;

    // Returns the PrefService that the embedder associates with
    // |browser_context|.
    virtual PrefService* GetPrefs(content::BrowserContext* browser_context) = 0;

    // Returns the HistoryService that the embedder associates with
    // |browser_context|.
    virtual history::HistoryService* GetHistoryService(
        content::BrowserContext* browser_context) = 0;

    // Gets the PingManager.
    virtual PingManager* GetPingManager(
        content::BrowserContext* browser_context) = 0;

    // Returns true if metrics reporting is enabled.
    virtual bool IsMetricsAndCrashReportingEnabled() = 0;

    // Returns true if sending of hit reports is enabled, in which case
    // SafeBrowsingUIManager will send hit reports when it deems the context
    // appropriate to do so (see ShouldSendHitReport()). If this method returns
    // false, SafeBrowsingUIManager will never send hit reports.
    // TODO(crbug.com/40780174): Eliminate this method if/once hit report
    // sending is enabled in WebLayer.
    virtual bool IsSendingOfHitReportsEnabled() = 0;
  };

  SafeBrowsingUIManager(
      std::unique_ptr<Delegate> delegate,
      std::unique_ptr<SafeBrowsingBlockingPageFactory> blocking_page_factory,
      const GURL& default_safe_page);

  SafeBrowsingUIManager(const SafeBrowsingUIManager&) = delete;
  SafeBrowsingUIManager& operator=(const SafeBrowsingUIManager&) = delete;

  // Displays a SafeBrowsing interstitial.
  // |resource| is the unsafe resource for which the warning is displayed.
  void StartDisplayingBlockingPage(const UnsafeResource& resource);

  // Creates a blocking page, used for both pre commit and post commit warnings.
  // Override is using a different blocking page.
  security_interstitials::SecurityInterstitialPage* CreateBlockingPage(
      content::WebContents* contents,
      const GURL& blocked_url,
      const UnsafeResource& unsafe_resource,
      bool forward_extension_event,
      std::optional<base::TimeTicks> blocked_page_shown_timestamp) override;

  // Called to stop or shutdown operations on the UI thread. This may be called
  // multiple times during the life of the UIManager. Should be called
  // on UI thread. If shutdown is true, the manager is disabled permanently.
  void Stop(bool shutdown);

  // Called on the UI thread by the ThreatDetails with the report, so the
  // PingManager can send it over.
  void SendThreatDetails(
      content::BrowserContext* browser_context,
      std::unique_ptr<ClientSafeBrowsingReportRequest> report) override;

  // Called on the UI thread by the ThreatDetails with the report, so the
  // HaTS service can later send it over if the user takes the survey.
  void AttachThreatDetailsAndLaunchSurvey(
      content::BrowserContext* browser_context,
      std::unique_ptr<ClientSafeBrowsingReportRequest> report) override;

  // Calls |BaseUIManager::OnBlockingPageDone()| and triggers
  // |OnSecurityInterstitialProceeded| event if |proceed| is true.
  void OnBlockingPageDone(const std::vector<UnsafeResource>& resources,
                          bool proceed,
                          content::WebContents* web_contents,
                          const GURL& main_frame_url,
                          bool showed_interstitial) override;

  // Report hits to unsafe contents (malware, phishing, unsafe download URL)
  // to the server. Can only be called on UI thread.  The hit report will
  // only be sent if the user has enabled SBER and is not in incognito mode.
  void MaybeReportSafeBrowsingHit(std::unique_ptr<HitReport> hit_report,
                                  content::WebContents* web_contents) override;

  // Send ClientSafeBrowsingReport for unsafe contents (malware, phishing,
  // unsafe download URL) to the server. Can only be called on UI thread.  The
  // report will only be sent if the user has enabled SBER and is not in
  // incognito mode.
  void MaybeSendClientSafeBrowsingWarningShownReport(
      std::unique_ptr<ClientSafeBrowsingReportRequest> report,
      content::WebContents* web_contents) override;

  // Creates the allowlist URL set for tests that create a blocking page
  // themselves and then simulate OnBlockingPageDone(). OnBlockingPageDone()
  // expects the allowlist to exist, but the tests don't necessarily call
  // DisplayBlockingPage(), which creates it.
  static void CreateAllowlistForTesting(content::WebContents* web_contents);

  static std::string GetThreatTypeStringForInterstitial(
      safe_browsing::SBThreatType threat_type);

  // Add and remove observers. These methods must be invoked on the UI thread.
  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* remove);

  // Invokes TriggerSecurityInterstitialShownExtensionEventIfDesired() on
  // |delegate_|.
  void ForwardSecurityInterstitialShownExtensionEventToEmbedder(
      content::WebContents* web_contents,
      const GURL& page_url,
      const std::string& reason,
      int net_error_code);

#if !BUILDFLAG(IS_ANDROID)
  // Invokes TriggerUrlFilteringInterstitialExtensionEventIfDesired() on
  // |delegate_|.
  void ForwardUrlFilteringInterstitialExtensionEventToEmbedder(
      content::WebContents* web_contents,
      const GURL& page_url,
      const std::string& threat_type,
      safe_browsing::RTLookupResponse rt_lookup_response);
#endif

  const std::string app_locale() const override;
  history::HistoryService* history_service(
      content::WebContents* web_contents) override;
  const GURL default_safe_page() const override;

 protected:
  ~SafeBrowsingUIManager() override;

  // Creates a hit report for the given resource and calls
  // MaybeReportSafeBrowsingHit. This also notifies all observers in
  // |observer_list_|.
  void CreateAndSendHitReport(const UnsafeResource& resource) override;

  // Creates a safe browsing report for the given resource and calls
  // MaybeSendClientSafeBrowsingWarningShownReport.
  void CreateAndSendClientSafeBrowsingWarningShownReport(
      const UnsafeResource& resource) override;

  // Helper method to ensure hit reports are only sent when the user has
  // opted in to extended reporting and is not currently in incognito mode.
  bool ShouldSendHitReport(HitReport* hit_report,
                           content::WebContents* web_contents);

  // Helper method to ensure client safe browsing reports are only sent when the
  // user has opted in to extended reporting and is not currently in incognito
  // mode.
  bool ShouldSendClientSafeBrowsingWarningShownReport(
      content::WebContents* web_contents);

 private:
  friend class SafeBrowsingUIManagerTest;
  friend class TestSafeBrowsingUIManager;
  FRIEND_TEST_ALL_PREFIXES(
      SafeBrowsingUIManagerTest,
      DontSendClientSafeBrowsingWarningShownReportNullWebContents);

  std::unique_ptr<Delegate> delegate_;

  std::unique_ptr<SafeBrowsingBlockingPageFactory> blocking_page_factory_;

  GURL default_safe_page_;

  base::ObserverList<Observer>::Unchecked observer_list_;

  bool shut_down_ = false;
};

}  // namespace safe_browsing

#endif  // COMPONENTS_SAFE_BROWSING_CONTENT_BROWSER_UI_MANAGER_H_
