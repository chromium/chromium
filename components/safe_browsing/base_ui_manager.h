// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SAFE_BROWSING_BASE_UI_MANAGER_H_
#define COMPONENTS_SAFE_BROWSING_BASE_UI_MANAGER_H_

#include <string>
#include <utility>
#include <vector>

#include "base/bind_helpers.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "components/security_interstitials/content/unsafe_resource.h"

class GURL;

namespace content {
class NavigationEntry;
class WebContents;
}  // namespace content

namespace history {
class HistoryService;
}  // namespace history

namespace safe_browsing {

class BaseBlockingPage;

// Construction needs to happen on the main thread.
class BaseUIManager
    : public base::RefCountedThreadSafe<BaseUIManager> {
 public:
  typedef security_interstitials::UnsafeResource UnsafeResource;

  BaseUIManager();

  // Called on the UI thread to display an interstitial page.
  // |resource| is the unsafe resource that triggered the interstitial.
  virtual void DisplayBlockingPage(const UnsafeResource& resource);

  // This is a no-op in the base class, but should be overridden to send threat
  // details. Called on the UI thread by the ThreatDetails with the serialized
  // protocol buffer.
  virtual void SendSerializedThreatDetails(const std::string& serialized);

  // Updates the whitelist URL set for |web_contents|. Called on the UI thread.
  void AddToWhitelistUrlSet(const GURL& whitelist_url,
                            content::WebContents* web_contents,
                            bool is_pending,
                            SBThreatType threat_type);

  // This is a no-op in the base class, but should be overridden to report hits
  // to the unsafe contents (malware, phishing, unsafe download URL)
  // to the server. Can only be called on UI thread. Will only upload a hit
  // report if the user has enabled SBER and is not currently in incognito mode.
  virtual void MaybeReportSafeBrowsingHit(
      const safe_browsing::HitReport& hit_report,
      content::WebContents* web_contents);

  // A convenience wrapper method for IsUrlWhitelistedOrPendingForWebContents.
  virtual bool IsWhitelisted(const UnsafeResource& resource);

  // Checks if we already displayed or are displaying an interstitial
  // for the top-level site |url| in a given WebContents. If
  // |whitelist_only|, it returns true only if the user chose to ignore
  // the interstitial. Otherwise, it returns true if an interstitial for
  // |url| is already displaying *or* if the user has seen an
  // interstitial for |url| before in this WebContents and proceeded
  // through it. Called on the UI thread.
  //
  // If the resource was found in the whitelist or pending for the
  // whitelist, |threat_type| will be set to the SBThreatType for which
  // the URL was first whitelisted.
  virtual bool IsUrlWhitelistedOrPendingForWebContents(
      const GURL& url,
      bool is_subresource,
      content::NavigationEntry* entry,
      content::WebContents* web_contents,
      bool whitelist_only,
      SBThreatType* threat_type);

  // The blocking page for |web_contents| on the UI thread has
  // completed, with |proceed| set to true if the user has chosen to
  // proceed through the blocking page and false
  // otherwise. |web_contents| is the WebContents that was displaying
  // the blocking page. |main_frame_url| is the top-level URL on which
  // the blocking page was displayed. If |proceed| is true,
  // |main_frame_url| is whitelisted so that the user will not see
  // another warning for that URL in this WebContents.
  virtual void OnBlockingPageDone(const std::vector<UnsafeResource>& resources,
                                  bool proceed,
                                  content::WebContents* web_contents,
                                  const GURL& main_frame_url);

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

  // Checks if an UnsafeResource |resource| exists for |url|, if so, it is
  // removed from the vector, assigned to |resource| and the function returns
  // true. Otherwise the function returns false and nothing gets assigned to
  // |resource|.
  bool PopUnsafeResourceForURL(
      GURL url,
      security_interstitials::UnsafeResource* resource);

 protected:
  friend class ChromePasswordProtectionService;
  virtual ~BaseUIManager();

  // Removes |whitelist_url| from the whitelist for |web_contents|.
  // Called on the UI thread.
  void RemoveWhitelistUrlSet(const GURL& whitelist_url,
                             content::WebContents* web_contents,
                             bool from_pending_only);

  // Ensures that |web_contents| has its whitelist set in its userdata
  static void EnsureWhitelistCreated(content::WebContents* web_contents);

  // Returns the URL that should be used in a WhitelistUrlSet for the given
  // |resource|.
  static GURL GetMainFrameWhitelistUrlForResource(
      const security_interstitials::UnsafeResource& resource);

  // BaseUIManager does not send SafeBrowsingHitReport. Subclasses should
  // implement the reporting logic themselves if needed.
  virtual void CreateAndSendHitReport(const UnsafeResource& resource);

  // Calls BaseBlockingPage::ShowBlockingPage(). Override this if using a
  // different blocking page.
  virtual void ShowBlockingPageForResource(const UnsafeResource& resource);

 private:
  // When true, we immediately cancel navigations that have been blocked by Safe
  // Browsing, otherwise we call show on the interstitial.
  bool SafeBrowsingInterstitialsAreCommittedNavigations();

  friend class base::RefCountedThreadSafe<BaseUIManager>;

  // Creates a blocking page, used for interstitials triggered by subresources.
  // Should be overridden with a blocking page implementation.
  virtual BaseBlockingPage* CreateBlockingPageForSubresource(
      content::WebContents* contents,
      const GURL& blocked_url,
      const UnsafeResource& unsafe_resource);

  // Stores unsafe resources so they can be fetched from a navigation throttle
  // in the committed interstitials flow. Implemented as a pair vector since
  // most of the time it will be empty or contain a single element.
  std::vector<std::pair<GURL, security_interstitials::UnsafeResource>>
      unsafe_resources_;

  DISALLOW_COPY_AND_ASSIGN(BaseUIManager);
};

}  // namespace safe_browsing

#endif  // COMPONENTS_SAFE_BROWSING_BASE_UI_MANAGER_H_
