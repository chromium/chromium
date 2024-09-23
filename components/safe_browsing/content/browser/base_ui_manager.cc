// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/safe_browsing/content/browser/base_ui_manager.h"

#include <utility>

#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/i18n/rtl.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/histogram_functions.h"
#include "components/safe_browsing/content/browser/async_check_tracker.h"
#include "components/safe_browsing/content/browser/base_blocking_page.h"
#include "components/safe_browsing/content/browser/unsafe_resource_util.h"
#include "components/safe_browsing/core/common/features.h"
#include "components/safe_browsing/core/common/scheme_logger.h"
#include "components/security_interstitials/content/security_interstitial_page.h"
#include "components/security_interstitials/content/security_interstitial_tab_helper.h"
#include "components/security_interstitials/core/unsafe_resource.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_user_data.h"
#include "services/network/public/mojom/fetch_api.mojom-shared.h"

using content::BrowserThread;
using content::NavigationEntry;
using content::WebContents;
using safe_browsing::ClientSafeBrowsingReportRequest;
using safe_browsing::HitReport;
using safe_browsing::SBThreatType;

namespace {

using safe_browsing::ThreatSeverity;

// Returns the URL host that should be used in a AllowlistUrlSet.
std::string GetCanonicalizedHost(const GURL& url) {
  if (url.HostIsIPAddress()) {
    return url.host();
  } else {
    std::string canon_host;
    safe_browsing::V4ProtocolManagerUtil::CanonicalizeUrl(url, &canon_host,
                                                          nullptr, nullptr);
    return canon_host;
  }
}

// A AllowlistUrlSet holds the set of URL hosts that have been allowlisted
// for a specific WebContents, along with pending entries that are still
// undecided. Each host is associated with the first SBThreatType that was seen
// for that host. The hosts in this set should be from the actual URL that was
// flagged by Safe Browsing, not the final URL in the redirect chain.
class AllowlistUrlSet : public content::WebContentsUserData<AllowlistUrlSet> {
 public:
  ~AllowlistUrlSet() override = default;
  AllowlistUrlSet(const AllowlistUrlSet&) = delete;
  AllowlistUrlSet& operator=(const AllowlistUrlSet&) = delete;

  bool ContainsHost(const std::string& canonicalized_host,
                    SBThreatType* threat_type) {
    if (canonicalized_host.empty()) {
      return false;
    }
    auto found = allowlisted_url_hosts_.find(canonicalized_host);
    if (found == allowlisted_url_hosts_.end()) {
      return false;
    }
    if (threat_type)
      *threat_type = found->second;
    return true;
  }
  bool Contains(const GURL& url, SBThreatType* threat_type) {
    std::string canonicalized_host = GetCanonicalizedHost(url);
    return ContainsHost(canonicalized_host, threat_type);
  }
  void RemovePending(const GURL& url,
                     const std::optional<int64_t> navigation_id) {
    std::string canonicalized_host = GetCanonicalizedHost(url);
    if (canonicalized_host.empty()) {
      return;
    }
    if (navigation_id.has_value()) {
      pending_navigation_ids_.erase(navigation_id.value());
    }
    DCHECK(pending_allowlisted_url_hosts_.end() !=
           pending_allowlisted_url_hosts_.find(canonicalized_host));
    if (--pending_allowlisted_url_hosts_[canonicalized_host].second < 1) {
      pending_allowlisted_url_hosts_.erase(canonicalized_host);
    }
  }
  void Remove(const GURL& url) {
    std::string canonicalized_host = GetCanonicalizedHost(url);
    if (canonicalized_host.empty()) {
      return;
    }
    allowlisted_url_hosts_.erase(canonicalized_host);
  }
  void Insert(const GURL& url,
              const std::optional<int64_t> navigation_id,
              SBThreatType threat_type) {
    std::string canonicalized_host = GetCanonicalizedHost(url);
    if (canonicalized_host.empty()) {
      return;
    }
    if (ContainsHost(canonicalized_host, nullptr)) {
      return;
    }
    allowlisted_url_hosts_[canonicalized_host] = threat_type;
    RemoveAllPending(canonicalized_host, navigation_id);
  }
  bool ContainsPending(const GURL& url, SBThreatType* threat_type) {
    std::string canonicalized_host = GetCanonicalizedHost(url);
    if (canonicalized_host.empty()) {
      return false;
    }
    auto found = pending_allowlisted_url_hosts_.find(canonicalized_host);
    if (found == pending_allowlisted_url_hosts_.end()) {
      return false;
    }
    if (threat_type) {
      *threat_type = found->second.first;
    }
    return true;
  }
  void InsertPending(const GURL url,
                     const std::optional<int64_t> navigation_id,
                     SBThreatType threat_type) {
    std::string canonicalized_host = GetCanonicalizedHost(url);
    if (canonicalized_host.empty()) {
      return;
    }
    if (navigation_id.has_value()) {
      if (base::Contains(pending_navigation_ids_, navigation_id.value())) {
        // Do not add URL host for the same navigation id in
        // |pending_allowlisted_url_hosts_| more than once. Otherwise, the
        // security indicator may not be cleared properly when navigating away.
        return;
      } else {
        pending_navigation_ids_.insert(navigation_id.value());
      }
    }
    if (pending_allowlisted_url_hosts_.find(canonicalized_host) !=
        pending_allowlisted_url_hosts_.end()) {
      pending_allowlisted_url_hosts_[canonicalized_host].first = threat_type;
      pending_allowlisted_url_hosts_[canonicalized_host].second++;
      return;
    }
    pending_allowlisted_url_hosts_[canonicalized_host] = {threat_type, 1};
  }

 private:
  friend class content::WebContentsUserData<AllowlistUrlSet>;
  WEB_CONTENTS_USER_DATA_KEY_DECL();

  explicit AllowlistUrlSet(content::WebContents* web_contents)
      : content::WebContentsUserData<AllowlistUrlSet>(*web_contents) {}

  // Method to remove all the instances of a website in
  // `pending_allowlisted_url_hosts_` disregarding the count. Used when adding a
  // site to `allowlisted_url_hosts_`.
  void RemoveAllPending(const std::string& url_host,
                        const std::optional<int64_t> navigation_id) {
    if (navigation_id.has_value()) {
      pending_navigation_ids_.erase(navigation_id.value());
    }
    pending_allowlisted_url_hosts_.erase(url_host);
  }

  // Hosts of URLs that have been allowlisted (i.e. bypassed by the user).
  std::map<std::string, SBThreatType> allowlisted_url_hosts_;
  // Hosts of URLs that are pending to be allowlisted (i.e. actively showing
  // interstitials). Keep a count of how many times a site has been added to the
  // pending list in order to solve a problem where upon reloading an
  // interstitial, a site would be re-added to and removed from the allowlist in
  // the wrong order.
  std::map<std::string, std::pair<SBThreatType, int>>
      pending_allowlisted_url_hosts_;
  // Ensure that the URL host for the same navigation id is added to
  // |pending_allowlisted_url_hosts_| at most once.
  std::set<int64_t> pending_navigation_ids_;
};

WEB_CONTENTS_USER_DATA_KEY_IMPL(AllowlistUrlSet);

// Returns the corresponding ThreatSeverity to a SBThreatType
// Keep the same as v4_local_database_manager GetThreatSeverity()
ThreatSeverity GetThreatSeverity(safe_browsing::SBThreatType threat_type) {
  using enum SBThreatType;
  switch (threat_type) {
    case SB_THREAT_TYPE_URL_MALWARE:
    case SB_THREAT_TYPE_URL_BINARY_MALWARE:
    case SB_THREAT_TYPE_URL_PHISHING:
    case SB_THREAT_TYPE_MANAGED_POLICY_BLOCK:
    case SB_THREAT_TYPE_MANAGED_POLICY_WARN:
      return 0;
    case SB_THREAT_TYPE_URL_UNWANTED:
      return 1;
    case SB_THREAT_TYPE_API_ABUSE:
    case SB_THREAT_TYPE_URL_CLIENT_SIDE_PHISHING:
    case SB_THREAT_TYPE_SUBRESOURCE_FILTER:
    case SB_THREAT_TYPE_SIGNED_IN_SYNC_PASSWORD_REUSE:
    case SB_THREAT_TYPE_ENTERPRISE_PASSWORD_REUSE:
    case SB_THREAT_TYPE_SAVED_PASSWORD_REUSE:
    case SB_THREAT_TYPE_SIGNED_IN_NON_SYNC_PASSWORD_REUSE:
      return 2;
    case SB_THREAT_TYPE_CSD_ALLOWLIST:
    case SB_THREAT_TYPE_HIGH_CONFIDENCE_ALLOWLIST:
      return 3;
    case SB_THREAT_TYPE_SUSPICIOUS_SITE:
      return 4;
    case SB_THREAT_TYPE_BILLING:
      return 15;
    case SB_THREAT_TYPE_UNUSED:
    case SB_THREAT_TYPE_SAFE:
      return std::numeric_limits<ThreatSeverity>::max();
    default:
      NOTREACHED_IN_MIGRATION();
      break;
  }
  return std::numeric_limits<ThreatSeverity>::max();
}

}  // namespace

namespace safe_browsing {

BaseUIManager::BaseUIManager() = default;

BaseUIManager::~BaseUIManager() = default;

bool BaseUIManager::IsAllowlisted(const UnsafeResource& resource) {
  NavigationEntry* entry =
      unsafe_resource_util::GetNavigationEntryForResource(resource);

  content::WebContents* web_contents =
      unsafe_resource_util::GetWebContentsForResource(resource);
  // |web_contents| can be null after RenderFrameHost is destroyed.
  if (!web_contents) {
    return false;
  }

  SBThreatType unused_threat_type;
  return IsUrlAllowlistedOrPendingForWebContents(
      resource.url, entry, web_contents, true, &unused_threat_type);
}

// Check if the user has already seen and/or ignored a SB warning for this
// WebContents and top-level domain.
bool BaseUIManager::IsUrlAllowlistedOrPendingForWebContents(
    const GURL& url,
    NavigationEntry* entry,
    WebContents* web_contents,
    bool allowlist_only,
    SBThreatType* threat_type) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  AllowlistUrlSet* site_list = AllowlistUrlSet::FromWebContents(web_contents);
  if (!site_list)
    return false;

  // To cover the case of redirect urls, we set the threat type to the most
  // severe one in the current navigation chain.
  bool any_allowlisted = false;
  ThreatSeverity min_severity = std::numeric_limits<ThreatSeverity>::max();

  for (const auto& redirect : entry && !entry->GetRedirectChain().empty()
                                  ? entry->GetRedirectChain()
                                  : std::vector<GURL>{url}) {
    if (redirect.is_empty()) {
      continue;
    }

    SBThreatType url_threat_type = SBThreatType::SB_THREAT_TYPE_SAFE;
    bool allowlisted = site_list->Contains(redirect, &url_threat_type);
    // We only check if the url is in the non-pending allowlist if
    // allowlist_only is true.
    if (!allowlist_only) {
      allowlisted |= site_list->ContainsPending(redirect, &url_threat_type);
    }
    if (allowlisted) {
      any_allowlisted = true;
      ThreatSeverity severity =
          url_threat_type == SBThreatType::SB_THREAT_TYPE_SAFE
              ? std::numeric_limits<ThreatSeverity>::max()
              : GetThreatSeverity(url_threat_type);
      if (severity > min_severity) {
        continue;
      }
      min_severity = severity;
      *threat_type = std::move(url_threat_type);
    }
  }
  return any_allowlisted;
}

void BaseUIManager::OnBlockingPageDone(
    const std::vector<UnsafeResource>& resources,
    bool proceed,
    WebContents* web_contents,
    const GURL& main_frame_url,
    bool showed_interstitial) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  for (const auto& resource : resources) {
    resource.DispatchCallback(FROM_HERE, proceed, showed_interstitial,
                              false /* has_post_commit_interstitial_skipped */);

    if (proceed) {
      AddToAllowlistUrlSet(main_frame_url, resource.navigation_id, web_contents,
                           false /* Pending -> permanent */,
                           resource.threat_type);
    } else if (web_contents) {
      // |web_contents| doesn't exist if the tab has been closed.
      RemoveAllowlistUrlSet(main_frame_url, resource.navigation_id,
                            web_contents, true /* from_pending_only */);
    }
  }
}

void BaseUIManager::DisplayBlockingPage(const UnsafeResource& resource) {
  using enum SBThreatType;
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  // The tab might have been closed. If it was closed, just act as if "Don't
  // Proceed" had been chosen.
  content::WebContents* web_contents =
      unsafe_resource_util::GetWebContentsForResource(resource);
  if (!web_contents) {
    OnBlockingPageDone(std::vector<UnsafeResource>{resource},
                       false /* proceed */, web_contents, resource.url,
                       false /* showed_interstitial */);
    return;
  }

  // Check if the user has already ignored a SB warning for the same WebContents
  // and top-level domain.
  if (IsAllowlisted(resource)) {
    resource.DispatchCallback(FROM_HERE, true /* proceed */,
                              false /* showed_interstitial */,
                              false /* has_post_commit_interstitial_skipped */);
    return;
  }

  // We don't want to send reports for the same navigation_id. Without this
  // check, duplicate reports can happen for sync and async checks that both
  // call into DisplayBlockingPage. This can happen if:
  // 1. Both checks finish at around the same time and post a task to display a
  //    blocking page at the same time, or
  // 2. The async check finishes between WillProcessResponse and
  //    DidFinishNavigation.
  bool already_reported = resource.navigation_id.has_value() &&
                          base::Contains(report_sent_navigation_ids_,
                                         resource.navigation_id.value());
  if (resource.threat_type != SB_THREAT_TYPE_SAFE &&
      resource.threat_type != SB_THREAT_TYPE_BILLING &&
      resource.threat_type != SB_THREAT_TYPE_MANAGED_POLICY_BLOCK &&
      resource.threat_type != SB_THREAT_TYPE_MANAGED_POLICY_WARN &&
      !already_reported) {
    // TODO(vakh): crbug/883462: The reports for SB_THREAT_TYPE_BILLING should
    // be disabled for M70 but enabled for a later release (M71?).
    CreateAndSendHitReport(resource);
    if (base::FeatureList::IsEnabled(
            safe_browsing::kCreateWarningShownClientSafeBrowsingReports)) {
      CreateAndSendClientSafeBrowsingWarningShownReport(resource);
    }
    if (resource.navigation_id.has_value()) {
      report_sent_navigation_ids_.insert(resource.navigation_id.value());
    }
  }

  AddToAllowlistUrlSet(resource.url, resource.navigation_id, web_contents,
                       true /* A decision is now pending */,
                       resource.threat_type);

  GURL unsafe_url = resource.url;

  // If the top-level navigation is still pending, we just mark the resource
  // unsafe and cancel the load from here, the actual interstitial will be shown
  // from the SafeBrowsingNavigationThrottle when the navigation fails.
  //
  // In other cases, the error interstitial is manually loaded here, after the
  // load is canceled:
  // - Subresources: since only documents load using a navigation, these
  //   won't hit the throttle.
  // - Nested frames and WebContents: The interstitial should be shown in the
  //   top, outer-most frame but the navigation is occurring in a nested
  //   context.
  // - Delayed Warning Experiment: When enabled, this method is only called
  //   after the navigation completes and a user action occurs so the throttle
  //   cannot be used.
  // - Async check: If the check is not able to complete before
  //   DidFinishNavigation, it won't hit the throttle.
  const bool load_post_commit_error_page =
      !AsyncCheckTracker::IsMainPageLoadPending(resource) ||
      resource.is_delayed_warning;
  if (!load_post_commit_error_page) {
    AddUnsafeResource(unsafe_url, resource);
  }

  // `showed_interstitial` is only set to true if the top-document navigation
  // has not yet committed. For other cases, the cancellation doesn't correspond
  // to the navigation that triggers the error page (the call to
  // LoadPostCommitErrorPage creates another navigation).
  resource.DispatchCallback(
      FROM_HERE, false /* proceed */,
      !load_post_commit_error_page /* showed_interstitial */,
      !load_post_commit_error_page /* has_post_commit_interstitial_skipped */);

  if (!base::FeatureList::IsEnabled(safe_browsing::kDelayedWarnings)) {
    DCHECK(!resource.is_delayed_warning);
  }

  if (load_post_commit_error_page) {
    DCHECK(!IsAllowlisted(resource));

    security_interstitials::SecurityInterstitialTabHelper* helper =
        security_interstitials::SecurityInterstitialTabHelper::FromWebContents(
            web_contents);
    if (helper && helper->HasPendingOrActiveInterstitial()) {
      // If a blocking page exists for the current navigation or an interstitial
      // is being displayed, do not create a new error page. This is to ensure
      // at most one blocking page is created for one single page so that the
      // SHOW bucket in the histogram does not log more than once. See
      // https://crbug.com/1195411 for details.
      return;
    }

    // In some cases the interstitial must be loaded here since there will be
    // no navigation to intercept in the throttle.
    std::unique_ptr<security_interstitials::SecurityInterstitialPage>
        blocking_page = base::WrapUnique(CreateBlockingPage(
            web_contents, unsafe_url, resource,
            /*forward_extension_event=*/true,
            AsyncCheckTracker::GetBlockedPageCommittedTimestamp(resource)));
    base::WeakPtr<content::NavigationHandle> error_page_navigation_handle =
        web_contents->GetController().LoadPostCommitErrorPage(
            web_contents->GetPrimaryMainFrame(), unsafe_url,
            blocking_page->GetHTMLContents());
    if (error_page_navigation_handle) {
      blocking_page->CreatedPostCommitErrorPageNavigation(
          error_page_navigation_handle.get());
      security_interstitials::SecurityInterstitialTabHelper::
          AssociateBlockingPage(error_page_navigation_handle.get(),
                                std::move(blocking_page));
    }
  }
}

void BaseUIManager::EnsureAllowlistCreated(WebContents* web_contents) {
  AllowlistUrlSet::CreateForWebContents(web_contents);
}

void BaseUIManager::CreateAndSendHitReport(const UnsafeResource& resource) {}
void BaseUIManager::CreateAndSendClientSafeBrowsingWarningShownReport(
    const UnsafeResource& resource) {}

security_interstitials::SecurityInterstitialPage*
BaseUIManager::CreateBlockingPage(
    content::WebContents* contents,
    const GURL& blocked_url,
    const UnsafeResource& unsafe_resource,
    bool forward_extension_event,
    std::optional<base::TimeTicks> blocked_page_shown_timestamp) {
  // TODO(carlosil): This can be removed once all implementations of SB use
  // committed interstitials. In the meantime, there is no create method for the
  // non-committed implementations, and this code won't be called if committed
  // interstitials are disabled.
  NOTREACHED_IN_MIGRATION();
  return nullptr;
}

// A SafeBrowsing hit is sent after a blocking page for malware/phishing
// or after the warning dialog for download urls, only for extended_reporting
// users who are not in incognito mode.
void BaseUIManager::MaybeReportSafeBrowsingHit(
    std::unique_ptr<HitReport> hit_report,
    content::WebContents* web_contents) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  return;
}

// A client safe browsing report is sent after a blocking page for
// malware/phishing or after the warning dialog for download urls, only for
// extended_reporting users who are not in incognito mode.
void BaseUIManager::MaybeSendClientSafeBrowsingWarningShownReport(
    std::unique_ptr<ClientSafeBrowsingReportRequest> report,
    content::WebContents* web_contents) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  return;
}

// If the user had opted-in to send ThreatDetails, this gets called
// when the report is ready.
void BaseUIManager::SendThreatDetails(
    content::BrowserContext* browser_context,
    std::unique_ptr<ClientSafeBrowsingReportRequest> report) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  return;
}

// If HaTS surveys are enabled, then this gets called when the report is ready.
void BaseUIManager::AttachThreatDetailsAndLaunchSurvey(
    content::BrowserContext* browser_context,
    std::unique_ptr<ClientSafeBrowsingReportRequest> report) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  return;
}

// Record this domain in the given WebContents as either allowlisted or
// pending allowlisted (if an interstitial is currently displayed). If an
// existing AllowlistUrlSet does not yet exist, create a new AllowlistUrlSet.
void BaseUIManager::AddToAllowlistUrlSet(
    const GURL& allowlist_url,
    const std::optional<int64_t> navigation_id,
    WebContents* web_contents,
    bool pending,
    SBThreatType threat_type) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  // A WebContents might not exist if the tab has been closed.
  if (!web_contents)
    return;

  AllowlistUrlSet::CreateForWebContents(web_contents);
  AllowlistUrlSet* site_list = AllowlistUrlSet::FromWebContents(web_contents);

  if (allowlist_url.is_empty())
    return;

  safe_browsing::scheme_logger::LogScheme(
      allowlist_url, "SafeBrowsing.WarningBypassAllowlist.SchemeOnWrite");
  if (pending) {
    site_list->InsertPending(allowlist_url, navigation_id, threat_type);
  } else {
    site_list->Insert(allowlist_url, navigation_id, threat_type);
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

bool BaseUIManager::PopUnsafeResourceForNavigation(
    GURL url,
    int64_t navigation_id,
    security_interstitials::UnsafeResource* resource) {
  for (auto it = unsafe_resources_.begin(); it != unsafe_resources_.end();
       it++) {
    if (it->first == url) {
      bool match_navigation_id =
          it->second.navigation_id.has_value() &&
          it->second.navigation_id.value() == navigation_id;
      base::UmaHistogramBoolean(
          "SafeBrowsing.NavigationIdMatchedInUnsafeResource",
          match_navigation_id);
      // Add the flag check to ensure no behavioral change when the flag is
      // disabled.
      if (match_navigation_id ||
          !base::FeatureList::IsEnabled(kSafeBrowsingAsyncRealTimeCheck)) {
        *resource = it->second;
        unsafe_resources_.erase(it);
        return true;
      }
    }
  }
  return false;
}

ThreatSeverity BaseUIManager::GetSeverestThreatForNavigation(
    content::NavigationHandle* handle,
    security_interstitials::UnsafeResource& severest_resource) {
  // Default is safe
  // Smaller numbers are more severe for ThreatSeverity
  ThreatSeverity min_severity = std::numeric_limits<ThreatSeverity>::max();
  if (!handle)
    return min_severity;

  return GetSeverestThreatForRedirectChain(
      handle->GetRedirectChain(), handle->GetNavigationId(), severest_resource);
}

ThreatSeverity BaseUIManager::GetSeverestThreatForRedirectChain(
    const std::vector<GURL>& redirect_chain,
    int64_t navigation_id,
    security_interstitials::UnsafeResource& severest_resource) {
  // Default is safe
  // Smaller numbers are more severe for ThreatSeverity
  ThreatSeverity min_severity = std::numeric_limits<ThreatSeverity>::max();

  for (auto&& url : redirect_chain) {
    security_interstitials::UnsafeResource resource;
    if (PopUnsafeResourceForNavigation(url, navigation_id, &resource)) {
      ThreatSeverity severity = GetThreatSeverity(resource.threat_type);
      if (severity > min_severity) {
        continue;
      }
      min_severity = severity;
      severest_resource = std::move(resource);
    }
  }
  return min_severity;
}

void BaseUIManager::RemoveAllowlistUrlSet(
    const GURL& allowlist_url,
    const std::optional<int64_t> navigation_id,
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
  AllowlistUrlSet* site_list = AllowlistUrlSet::FromWebContents(web_contents);

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
    site_list->RemovePending(allowlist_url, navigation_id);
  }

  if (!from_pending_only && site_list &&
      site_list->Contains(allowlist_url, nullptr)) {
    site_list->Remove(allowlist_url);
  }

  // Notify security UI that security state has changed.
  web_contents->DidChangeVisibleSecurityState();
}

}  // namespace safe_browsing
