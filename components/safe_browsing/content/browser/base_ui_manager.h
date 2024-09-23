// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SAFE_BROWSING_CONTENT_BROWSER_BASE_UI_MANAGER_H_
#define COMPONENTS_SAFE_BROWSING_CONTENT_BROWSER_BASE_UI_MANAGER_H_

#include <memory>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "base/functional/callback_helpers.h"
#include "base/memory/ref_counted.h"
#include "components/safe_browsing/core/common/proto/csd.pb.h"
#include "components/security_interstitials/content/security_interstitial_page.h"
#include "components/security_interstitials/core/unsafe_resource.h"
#include "content/public/browser/navigation_handle.h"

class GURL;

namespace content {
class NavigationEntry;
class WebContents;
class BrowserContext;
}  // namespace content

namespace history {
class HistoryService;
}  // namespace history

namespace safe_browsing {

typedef unsigned ThreatSeverity;

// Construction needs to happen on the main thread.
class BaseUIManager : public base::RefCountedThreadSafe<BaseUIManager> {
 public:
  typedef security_interstitials::UnsafeResource UnsafeResource;
  typedef security_interstitials::SecurityInterstitialPage
      SecurityInterstitialPage;

  BaseUIManager();

  BaseUIManager(const BaseUIManager&) = delete;
  BaseUIManager& operator=(const BaseUIManager&) = delete;

  // Called on the UI thread to display an interstitial page.
  // |resource| is the unsafe resource that triggered the interstitial.
  // With committed interstitials:
  // -For pre-commit navigations this will only cancel the load, the
  // interstitial will then be shown from a navigation throttle.
  // -For post-commit navigations this will cancel the load, then call
  // LoadPostCommitErrorPage, which will show the interstitial.
  virtual void DisplayBlockingPage(const UnsafeResource& resource);

  // Creates a blocking page, used for both pre commit and post commit warnings.
  // Also forwards an interstitial shown extension event to embedder if
  // |forward_extension_event| is true. |blocked_page_shown_timestamp| is set to
  // the time when the |blocked_url| is committed. If |blocked_url| is never
  // committed, it will be set to nullopt. Should be overridden with a blocking
  // page implementation.
  virtual SecurityInterstitialPage* CreateBlockingPage(
      content::WebContents* contents,
      const GURL& blocked_url,
      const UnsafeResource& unsafe_resource,
      bool forward_extension_event,
      std::optional<base::TimeTicks> blocked_page_shown_timestamp);

  // This is a no-op in the base class, but should be overridden to send threat
  // details. Called on the UI thread by the ThreatDetails with the report.
  virtual void SendThreatDetails(
      content::BrowserContext* browser_context,
      std::unique_ptr<ClientSafeBrowsingReportRequest> report);

  // This is a no-op in the base class, but should be overridden to have threat
  // details included as part of a user's response to a HaTS survey.
  virtual void AttachThreatDetailsAndLaunchSurvey(
      content::BrowserContext* browser_context,
      std::unique_ptr<ClientSafeBrowsingReportRequest> report);

  // Updates the allowlist URL set for |web_contents|. |navigation_id| is used
  // to ensure the |allowlist_url| for same navigation is only added once.
  // Called on the UI thread.
  void AddToAllowlistUrlSet(const GURL& allowlist_url,
                            const std::optional<int64_t> navigation_id,
                            content::WebContents* web_contents,
                            bool is_pending,
                            SBThreatType threat_type);

  // This is a no-op in the base class, but should be overridden to report hits
  // to the unsafe contents (malware, phishing, unsafe download URL)
  // to the server. Can only be called on UI thread. Will only upload a hit
  // report if the user has enabled SBER and is not currently in incognito mode.
  virtual void MaybeReportSafeBrowsingHit(
      std::unique_ptr<safe_browsing::HitReport> hit_report,
      content::WebContents* web_contents);

  // This is a no-op in the base class, but should be overridden to send report
  // about unsafe contents (malware, phishing, unsafe download URL) to the
  // server. Can only be called on UI thread and only sent for
  // extended_reporting users who are not in incognito mode.
  virtual void MaybeSendClientSafeBrowsingWarningShownReport(
      std::unique_ptr<ClientSafeBrowsingReportRequest> report,
      content::WebContents* web_contents);

  // A convenience wrapper method for IsUrlAllowlistedOrPendingForWebContents.
  virtual bool IsAllowlisted(const UnsafeResource& resource);

  // Checks if we already displayed or are displaying an interstitial
  // for the top-level site |url| or any URLs in the redirect chain of |entry|
  // in a given WebContents. If |allowlist_only|, it returns true only if the
  // user chose to ignore the interstitial. Otherwise, it returns true if an
  // interstitial for |url| is already displaying *or* if the user has seen an
  // interstitial for |url| before in this WebContents and proceeded
  // through it. Called on the UI thread.
  //
  // If the resource was found in the allowlist or pending for the
  // allowlist, |threat_type| will be set to the SBThreatType for which
  // the URL was first allowlisted.
  virtual bool IsUrlAllowlistedOrPendingForWebContents(
      const GURL& url,
      content::NavigationEntry* entry,
      content::WebContents* web_contents,
      bool allowlist_only,
      SBThreatType* threat_type);

  // The blocking page for |web_contents| on the UI thread has
  // completed, with |proceed| set to true if the user has chosen to
  // proceed through the blocking page and false
  // otherwise. |web_contents| is the WebContents that was displaying
  // the blocking page. |main_frame_url| is the top-level URL on which
  // the blocking page was displayed. If |proceed| is true,
  // |main_frame_url| is allowlisted so that the user will not see
  // another warning for that URL in this WebContents. |showed_interstitial|
  // should be set to true if an interstitial was shown, or false if the action
  // was decided without showing an interstitial.
  virtual void OnBlockingPageDone(const std::vector<UnsafeResource>& resources,
                                  bool proceed,
                                  content::WebContents* web_contents,
                                  const GURL& main_frame_url,
                                  bool showed_interstitial);

  virtual const std::string app_locale() const;

  virtual history::HistoryService* history_service(
      content::WebContents* web_contents);

  // The default safe page when there is no entry in the history to go back to.
  // e.g. about::blank page, or chrome's new tab page.
  virtual const GURL default_safe_page() const;

  // Adds an UnsafeResource |resource| for |url| to unsafe_resources_,
  // this should be called whenever a resource load is blocked due to a SB hit.
  void AddUnsafeResource(GURL url,
                         security_interstitials::UnsafeResource resource);

  // Checks if an UnsafeResource |resource| exists for |url| and
  // |navigation_id|, if so, it is removed from the vector, assigned to
  // |resource| and the function returns true. Otherwise the function returns
  // false and nothing gets assigned to |resource|.
  bool PopUnsafeResourceForNavigation(
      GURL url,
      int64_t navigation_id,
      security_interstitials::UnsafeResource* resource);

  // Goes over the |handle->RedirectChain| and returns the severest threat.
  // The lowest value is 0, which represents the most severe type.
  ThreatSeverity GetSeverestThreatForNavigation(
      content::NavigationHandle* handle,
      security_interstitials::UnsafeResource& severest_resource);

  // Goes over the |redirect_chain| and returns the severest threat.
  // The lowest value is 0, which represents the most severe type.
  ThreatSeverity GetSeverestThreatForRedirectChain(
      const std::vector<GURL>& redirect_chain,
      int64_t navigation_id,
      security_interstitials::UnsafeResource& severest_resource);

 protected:
  friend class ChromePasswordProtectionService;
  virtual ~BaseUIManager();

  // Removes |allowlist_url| associated with the |navigation_id| from the
  // allowlist for |web_contents|. Called on the UI thread.
  void RemoveAllowlistUrlSet(const GURL& allowlist_url,
                             const std::optional<int64_t> navigation_id,
                             content::WebContents* web_contents,
                             bool from_pending_only);

  // Ensures that |web_contents| has its allowlist set in its userdata
  static void EnsureAllowlistCreated(content::WebContents* web_contents);

  // BaseUIManager does not send SafeBrowsingHitReport. Subclasses should
  // implement the reporting logic themselves if needed.
  virtual void CreateAndSendHitReport(const UnsafeResource& resource);

  // BaseUIManager does not send ClientSafeBrowsingReport. Subclasses should
  // implement the reporting logic themselves if needed.
  virtual void CreateAndSendClientSafeBrowsingWarningShownReport(
      const UnsafeResource& resource);

 private:
  friend class base::RefCountedThreadSafe<BaseUIManager>;

  // Stores unsafe resources so they can be fetched from a navigation throttle
  // in the committed interstitials flow. Implemented as a pair vector since
  // most of the time it will be empty or contain a single element.
  std::vector<std::pair<GURL, security_interstitials::UnsafeResource>>
      unsafe_resources_;

  // Tracks the navigation IDs that have already had Safe Browsing telemetry
  // reports sent. Used to ensure duplicate reports aren't sent for the same
  // navigation.
  std::set<int64_t> report_sent_navigation_ids_;
};

}  // namespace safe_browsing

#endif  // COMPONENTS_SAFE_BROWSING_CONTENT_BROWSER_BASE_UI_MANAGER_H_
