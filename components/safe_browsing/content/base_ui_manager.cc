// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <utility>

#include "components/safe_browsing/content/base_ui_manager.h"

#include "base/bind.h"
#include "base/callback.h"
#include "base/feature_list.h"
#include "base/i18n/rtl.h"
#include "base/memory/ptr_util.h"
#include "base/supports_user_data.h"
#include "components/safe_browsing/content/base_blocking_page.h"
#include "components/safe_browsing/core/features.h"
#include "components/security_interstitials/content/unsafe_resource_util.h"
#include "components/security_interstitials/core/unsafe_resource.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/web_contents.h"

using content::BrowserThread;
using content::NavigationEntry;
using content::WebContents;
using safe_browsing::HitReport;
using safe_browsing::SBThreatType;

namespace {

const void* const kAllowlistKey = &kAllowlistKey;

// A AllowlistUrlSet holds the set of URLs that have been allowlisted
// for a specific WebContents, along with pending entries that are still
// undecided. Each URL is associated with the first SBThreatType that
// was seen for that URL. The URLs in this set should come from
// GetAllowlistUrl() or GetMainFrameAllowlistUrlForResource() (in
// SafeBrowsingUIManager)
class AllowlistUrlSet : public base::SupportsUserData::Data {
 public:
  AllowlistUrlSet() {}
  bool Contains(const GURL& url, SBThreatType* threat_type) {
    auto found = map_.find(url);
    if (found == map_.end())
      return false;
    if (threat_type)
      *threat_type = found->second;
    return true;
  }
  void RemovePending(const GURL& url) {
    DCHECK(pending_.end() != pending_.find(url));
    if (--pending_[url].second < 1)
      pending_.erase(url);
  }
  void Remove(const GURL& url) { map_.erase(url); }
  void Insert(const GURL& url, SBThreatType threat_type) {
    if (Contains(url, nullptr))
      return;
    map_[url] = threat_type;
    RemoveAllPending(url);
  }
  bool ContainsPending(const GURL& url, SBThreatType* threat_type) {
    auto found = pending_.find(url);
    if (found == pending_.end())
      return false;
    if (threat_type)
      *threat_type = found->second.first;
    return true;
  }
  void InsertPending(const GURL url, SBThreatType threat_type) {
    if (pending_.find(url) != pending_.end()) {
      pending_[url].first = threat_type;
      pending_[url].second++;
      return;
    }
    pending_[url] = {threat_type, 1};
  }

 protected:
  // Method to remove all the instances of a website in the pending list
  // disregarding the count. Used when adding a site to the permanent list.
  void RemoveAllPending(const GURL& url) { pending_.erase(url); }

 private:
  std::map<GURL, SBThreatType> map_;
  // Keep a count of how many times a site has been added to the pending list
  // in order to solve a problem where upon reloading an interstitial, a site
  // would be re-added to and removed from the allowlist in the wrong order.
  std::map<GURL, std::pair<SBThreatType, int>> pending_;
  DISALLOW_COPY_AND_ASSIGN(AllowlistUrlSet);
};

// Returns the URL that should be used in a AllowlistUrlSet for the
// resource loaded from |url| on a navigation |entry|.
GURL GetAllowlistUrl(const GURL& url,
                     bool is_subresource,
                     NavigationEntry* entry) {
  if (is_subresource) {
    if (!entry)
      return GURL();
    return entry->GetURL().GetWithEmptyPath();
  }
  return url.GetWithEmptyPath();
}

AllowlistUrlSet* GetOrCreateAllowlist(WebContents* web_contents) {
  AllowlistUrlSet* site_list =
      static_cast<AllowlistUrlSet*>(web_contents->GetUserData(kAllowlistKey));
  if (!site_list) {
    site_list = new AllowlistUrlSet;
    web_contents->SetUserData(kAllowlistKey, base::WrapUnique(site_list));
  }
  return site_list;
}

}  // namespace

namespace safe_browsing {

BaseUIManager::BaseUIManager() {}

BaseUIManager::~BaseUIManager() {}

bool BaseUIManager::IsAllowlisted(const UnsafeResource& resource) {
  NavigationEntry* entry = nullptr;
  if (resource.is_subresource) {
    entry = GetNavigationEntryForResource(resource);
  }
  SBThreatType unused_threat_type;
  return IsUrlAllowlistedOrPendingForWebContents(
      resource.url, resource.is_subresource, entry,
      resource.web_contents_getter.Run(), true, &unused_threat_type);
}

// Check if the user has already seen and/or ignored a SB warning for this
// WebContents and top-level domain.
bool BaseUIManager::IsUrlAllowlistedOrPendingForWebContents(
    const GURL& url,
    bool is_subresource,
    NavigationEntry* entry,
    WebContents* web_contents,
    bool allowlist_only,
    SBThreatType* threat_type) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  GURL lookup_url = GetAllowlistUrl(url, is_subresource, entry);
  if (lookup_url.is_empty())
    return false;

  AllowlistUrlSet* site_list =
      static_cast<AllowlistUrlSet*>(web_contents->GetUserData(kAllowlistKey));
  if (!site_list)
    return false;

  bool allowlisted = site_list->Contains(lookup_url, threat_type);
  if (allowlist_only) {
    return allowlisted;
  } else {
    return allowlisted || site_list->ContainsPending(lookup_url, threat_type);
  }
}

void BaseUIManager::OnBlockingPageDone(
    const std::vector<UnsafeResource>& resources,
    bool proceed,
    WebContents* web_contents,
    const GURL& main_frame_url,
    bool showed_interstitial) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  for (const auto& resource : resources) {
    resource.DispatchCallback(FROM_HERE, proceed, showed_interstitial);

    GURL allowlist_url = GetAllowlistUrl(
        main_frame_url, false /* is subresource */,
        nullptr /* no navigation entry needed for main resource */);
    if (proceed) {
      AddToAllowlistUrlSet(allowlist_url, web_contents,
                           false /* Pending -> permanent */,
                           resource.threat_type);
    } else if (web_contents) {
      // |web_contents| doesn't exist if the tab has been closed.
      RemoveAllowlistUrlSet(allowlist_url, web_contents,
                            true /* from_pending_only */);
    }
  }
}

namespace {
// In the case of nested WebContents, returns the WebContents where it is
// suitable to show an interstitial.
content::WebContents* GetEmbeddingWebContentsForInterstitial(
    content::WebContents* source_contents) {
  content::WebContents* top_level_contents = source_contents;
  // Note that |WebContents::GetResponsibleWebContents| is not suitable here
  // since we want to stay within any GuestViews.
  while (top_level_contents->IsPortal()) {
    top_level_contents = top_level_contents->GetPortalHostWebContents();
  }
  return top_level_contents;
}
}  // namespace

void BaseUIManager::DisplayBlockingPage(
    const UnsafeResource& resource) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (resource.is_subresource && !resource.is_subframe) {
    // Sites tagged as serving Unwanted Software should only show a warning for
    // main-frame or sub-frame resource. Similar warning restrictions should be
    // applied to malware sites tagged as "landing sites" (see "Types of
    // Malware sites" under
    // https://developers.google.com/safe-browsing/developers_guide_v3#UserWarnings).
    if (resource.threat_type == SB_THREAT_TYPE_URL_UNWANTED ||
        (resource.threat_type == SB_THREAT_TYPE_URL_MALWARE &&
         resource.threat_metadata.threat_pattern_type ==
             ThreatPatternType::MALWARE_LANDING)) {
      resource.DispatchCallback(FROM_HERE, true /* proceed */,
                                false /* showed_interstitial */);
      return;
    }
  }

  // The tab might have been closed. If it was closed, just act as if "Don't
  // Proceed" had been chosen.
  WebContents* web_contents = resource.web_contents_getter.Run();
  if (!web_contents) {
    OnBlockingPageDone(std::vector<UnsafeResource>{resource},
                       false /* proceed */, web_contents,
                       GetMainFrameAllowlistUrlForResource(resource),
                       false /* showed_interstitial */);
    return;
  }

  // Check if the user has already ignored a SB warning for the same WebContents
  // and top-level domain.
  if (IsAllowlisted(resource)) {
    resource.DispatchCallback(FROM_HERE, true /* proceed */,
                              false /* showed_interstitial */);
    return;
  }

  if (resource.threat_type != SB_THREAT_TYPE_SAFE &&
      resource.threat_type != SB_THREAT_TYPE_BILLING) {
    // TODO(vakh): crbug/883462: The reports for SB_THREAT_TYPE_BILLING should
    // be disabled for M70 but enabled for a later release (M71?).
    CreateAndSendHitReport(resource);
  }

  AddToAllowlistUrlSet(GetMainFrameAllowlistUrlForResource(resource),
                       web_contents, true /* A decision is now pending */,
                       resource.threat_type);

  // |entry| can be null if we are on a brand new tab, and a resource is added
  // via javascript without a navigation.
  content::NavigationEntry* entry = GetNavigationEntryForResource(resource);

  // If unsafe content is loaded in a portal, we treat its embedder as
  // dangerous.
  content::WebContents* outermost_contents =
      GetEmbeddingWebContentsForInterstitial(web_contents);

  GURL unsafe_url = resource.url;
  if (outermost_contents != web_contents) {
    DCHECK(outermost_contents->GetController().GetLastCommittedEntry());
    unsafe_url =
        outermost_contents->GetController().GetLastCommittedEntry()->GetURL();
  } else if (entry && !resource.IsMainPageLoadBlocked()) {
    unsafe_url = entry->GetURL();
  }
  AddUnsafeResource(unsafe_url, resource);
  // If the delayed warnings experiment is not enabled, with committed
  // interstitials we just cancel the load from here, the actual interstitial
  // will be shown from the SafeBrowsingNavigationThrottle.
  // showed_interstitial is set to false for subresources since this
  // cancellation doesn't correspond to the navigation that triggers the error
  // page (the call to LoadPostCommitErrorPage creates another navigation).
  //
  // If the experiment is enabled, the interstitial is shown below.
  resource.DispatchCallback(
      FROM_HERE, false /* proceed */,
      resource.IsMainPageLoadBlocked() /* showed_interstitial */);

  if (!base::FeatureList::IsEnabled(safe_browsing::kDelayedWarnings)) {
    DCHECK(!resource.is_delayed_warning);
  }

  if (!resource.IsMainPageLoadBlocked() || resource.is_delayed_warning ||
      outermost_contents != web_contents) {
    DCHECK(!IsAllowlisted(resource));
    // For subresource triggered interstitials, we trigger the error page
    // navigation from here since there will be no navigation to intercept
    // in the throttle.
    //
    // Blocking pages handle both user interaction, and generation of the
    // interstitial HTML. In the case of subresources, we need the HTML
    // content prior to (and in a different process than when) installing the
    // command handlers. For this reason we create a blocking page here just
    // to generate the HTML, and immediately delete it.
    std::unique_ptr<BaseBlockingPage> blocking_page =
        base::WrapUnique(CreateBlockingPageForSubresource(
            outermost_contents, unsafe_url, resource));
    outermost_contents->GetController().LoadPostCommitErrorPage(
        outermost_contents->GetMainFrame(), unsafe_url,
        blocking_page->GetHTMLContents(), net::ERR_BLOCKED_BY_CLIENT);
  }
}

void BaseUIManager::EnsureAllowlistCreated(WebContents* web_contents) {
  GetOrCreateAllowlist(web_contents);
}

void BaseUIManager::CreateAndSendHitReport(const UnsafeResource& resource) {}

BaseBlockingPage* BaseUIManager::CreateBlockingPageForSubresource(
    content::WebContents* contents,
    const GURL& blocked_url,
    const UnsafeResource& unsafe_resource) {
  // TODO(carlosil): This can be removed once all implementations of SB use
  // committed interstitials. In the meantime, there is no create method for the
  // non-committed implementations, and this code won't be called if committed
  // interstitials are disabled.
  NOTREACHED();
  return nullptr;
}

// A SafeBrowsing hit is sent after a blocking page for malware/phishing
// or after the warning dialog for download urls, only for extended_reporting
// users who are not in incognito mode.
void BaseUIManager::MaybeReportSafeBrowsingHit(
    const HitReport& hit_report,
    content::WebContents* web_contents) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  return;
}

// If the user had opted-in to send ThreatDetails, this gets called
// when the report is ready.
void BaseUIManager::SendSerializedThreatDetails(
    content::BrowserContext* browser_context,
    const std::string& serialized) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  return;
}

// Record this domain in the given WebContents as either allowlisted or
// pending allowlisted (if an interstitial is currently displayed). If an
// existing AllowlistUrlSet does not yet exist, create a new AllowlistUrlSet.
void BaseUIManager::AddToAllowlistUrlSet(const GURL& allowlist_url,
                                         WebContents* web_contents,
                                         bool pending,
                                         SBThreatType threat_type) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  // A WebContents might not exist if the tab has been closed.
  if (!web_contents)
    return;

  AllowlistUrlSet* site_list = GetOrCreateAllowlist(web_contents);

  if (allowlist_url.is_empty())
    return;

  if (pending) {
    site_list->InsertPending(allowlist_url, threat_type);
  } else {
    site_list->Insert(allowlist_url, threat_type);
  }

  // Notify security UI that security state has changed.
  web_contents->DidChangeVisibleSecurityState();
}

const std::string BaseUIManager::app_locale() const {
  return base::i18n::GetConfiguredLocale();
}

history::HistoryService* BaseUIManager::history_service(
    content::WebContents* web_contents) {
  return nullptr;
}

const GURL BaseUIManager::default_safe_page() const {
  return GURL(url::kAboutBlankURL);
}

void BaseUIManager::AddUnsafeResource(
    GURL url,
    security_interstitials::UnsafeResource resource) {
  unsafe_resources_.push_back(std::make_pair(url, resource));
}

bool BaseUIManager::PopUnsafeResourceForURL(
    GURL url,
    security_interstitials::UnsafeResource* resource) {
  for (auto it = unsafe_resources_.begin(); it != unsafe_resources_.end();
       it++) {
    if (it->first == url) {
      *resource = it->second;
      unsafe_resources_.erase(it);
      return true;
    }
  }
  return false;
}

void BaseUIManager::RemoveAllowlistUrlSet(const GURL& allowlist_url,
                                          WebContents* web_contents,
                                          bool from_pending_only) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  // A WebContents might not exist if the tab has been closed.
  if (!web_contents)
    return;

  // Use |web_contents| rather than |resource.web_contents_getter|
  // here. By this point, a "Back" navigation could have already been
  // committed, so the page loading |resource| might be gone and
  // |web_contents_getter| may no longer be valid.
  AllowlistUrlSet* site_list =
      static_cast<AllowlistUrlSet*>(web_contents->GetUserData(kAllowlistKey));

  if (allowlist_url.is_empty())
    return;

  // Note that this function does not DCHECK that |allowlist_url|
  // appears in the pending allowlist. In the common case, it's expected
  // that a URL is in the pending allowlist when it is removed, but it's
  // not always the case. For example, if there are several blocking
  // pages queued up for different resources on the same page, and the
  // user goes back to dimiss the first one, the subsequent blocking
  // pages get dismissed as well (as if the user had clicked "Back to
  // safety" on each of them). In this case, the first dismissal will
  // remove the main-frame URL from the pending allowlist, so the
  // main-frame URL will have already been removed when the subsequent
  // blocking pages are dismissed.
  if (site_list && site_list->ContainsPending(allowlist_url, nullptr)) {
    site_list->RemovePending(allowlist_url);
  }

  if (!from_pending_only && site_list &&
      site_list->Contains(allowlist_url, nullptr)) {
    site_list->Remove(allowlist_url);
  }

  // Notify security UI that security state has changed.
  web_contents->DidChangeVisibleSecurityState();
}

// static
GURL BaseUIManager::GetMainFrameAllowlistUrlForResource(
    const security_interstitials::UnsafeResource& resource) {
  if (resource.is_subresource) {
    NavigationEntry* entry = GetNavigationEntryForResource(resource);
    if (!entry)
      return GURL();
    return entry->GetURL().GetWithEmptyPath();
  }
  return resource.url.GetWithEmptyPath();
}

}  // namespace safe_browsing
