// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/safe_browsing/content/browser/ui_manager.h"

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/metrics/histogram_macros.h"
#include "base/observer_list.h"
#include "base/threading/thread.h"
#include "base/threading/thread_restrictions.h"
#include "build/build_config.h"
#include "components/no_state_prefetch/browser/no_state_prefetch_contents.h"
#include "components/prefs/pref_service.h"
#include "components/safe_browsing/content/browser/async_check_tracker.h"
#include "components/safe_browsing/content/browser/client_report_util.h"
#include "components/safe_browsing/content/browser/safe_browsing_blocking_page.h"
#include "components/safe_browsing/content/browser/threat_details.h"
#include "components/safe_browsing/content/browser/unsafe_resource_util.h"
#include "components/safe_browsing/core/browser/db/v4_protocol_manager_util.h"
#include "components/safe_browsing/core/browser/ping_manager.h"
#include "components/safe_browsing/core/common/features.h"
#include "components/safe_browsing/core/common/safe_browsing_prefs.h"
#include "components/security_interstitials/content/security_interstitial_tab_helper.h"
#include "components/security_interstitials/core/unsafe_resource.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/disallow_activation_reason.h"
#include "content/public/browser/global_routing_id.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/web_contents.h"
#include "ipc/ipc_message.h"
#include "url/gurl.h"

using content::BrowserThread;
using content::NavigationEntry;
using content::WebContents;
using safe_browsing::ClientSafeBrowsingReportRequest;
using safe_browsing::HitReport;
using safe_browsing::SBThreatType;

namespace safe_browsing {

using enum ExtendedReportingLevel;

SafeBrowsingUIManager::SafeBrowsingUIManager(
    std::unique_ptr<Delegate> delegate,
    std::unique_ptr<SafeBrowsingBlockingPageFactory> blocking_page_factory,
    const GURL& default_safe_page)
    : delegate_(std::move(delegate)),
      blocking_page_factory_(std::move(blocking_page_factory)),
      default_safe_page_(default_safe_page) {}

SafeBrowsingUIManager::~SafeBrowsingUIManager() {}

void SafeBrowsingUIManager::Stop(bool shutdown) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  if (shutdown) {
    shut_down_ = true;
  }
}

void SafeBrowsingUIManager::CreateAndSendHitReport(
    const UnsafeResource& resource) {
  WebContents* web_contents =
      unsafe_resource_util::GetWebContentsForResource(resource);
  DCHECK(web_contents);
  std::unique_ptr<HitReport> hit_report = std::make_unique<HitReport>();
  hit_report->malicious_url = resource.url;
  hit_report->is_subresource = false;
  hit_report->threat_type = resource.threat_type;
  hit_report->threat_source = resource.threat_source;

  NavigationEntry* entry =
      unsafe_resource_util::GetNavigationEntryForResource(resource);
  if (entry) {
    hit_report->page_url = entry->GetURL();
    hit_report->referrer_url = entry->GetReferrer().url;
  }

  // When resource.original_url is not the same as the resource.url, that means
  // we have a redirect from resource.original_url to resource.url. Also, at
  // this point, page_url points to the _previous_ page that we were on. We
  // replace page_url with resource.original_url and referrer with page_url.
  if (!resource.original_url.is_empty() &&
      resource.original_url != resource.url) {
    hit_report->referrer_url = hit_report->page_url;
    hit_report->page_url = resource.original_url;
  }

  const auto& prefs = *delegate_->GetPrefs(web_contents->GetBrowserContext());

  hit_report->extended_reporting_level = GetExtendedReportingLevel(prefs);
  hit_report->is_enhanced_protection = IsEnhancedProtectionEnabled(prefs);
  hit_report->is_metrics_reporting_active =
      delegate_->IsMetricsAndCrashReportingEnabled();

  MaybeReportSafeBrowsingHit(std::move(hit_report), web_contents);

  for (Observer& observer : observer_list_)
    observer.OnSafeBrowsingHit(resource);
}

void SafeBrowsingUIManager::CreateAndSendClientSafeBrowsingWarningShownReport(
    const UnsafeResource& resource) {
  WebContents* web_contents =
      unsafe_resource_util::GetWebContentsForResource(resource);
  DCHECK(web_contents);
  std::unique_ptr<ClientSafeBrowsingReportRequest> report =
      std::make_unique<ClientSafeBrowsingReportRequest>();
  client_report_utils::FillReportBasicResourceDetails(report.get(), resource);

  // When resource.original_url is not the same as the resource.url, that means
  // we have a redirect from resource.original_url to resource.url. Also, at
  // this point, page_url points to the _previous_ page that we were on. We
  // replace page_url with resource.original_url and referrer with page_url.
  if (!resource.original_url.is_empty() &&
      resource.original_url != resource.url) {
    report->set_referrer_url(report->page_url());
    report->set_page_url(resource.original_url.spec());
  }

  report->set_type(ClientSafeBrowsingReportRequest::WARNING_SHOWN);
  report->set_warning_shown_timestamp_msec(
      base::Time::Now().InMillisecondsSinceUnixEpoch());
  report->mutable_warning_shown_info()->set_warning_type(
      client_report_utils::GetWarningUXTypeFromSBThreatType(
          resource.threat_type));
  MaybeSendClientSafeBrowsingWarningShownReport(std::move(report),
                                                web_contents);
}

void SafeBrowsingUIManager::StartDisplayingBlockingPage(
    const security_interstitials::UnsafeResource& resource) {
  content::WebContents* web_contents =
      unsafe_resource_util::GetWebContentsForResource(resource);

  if (!web_contents) {
    // Tab is gone.
    resource.DispatchCallback(FROM_HERE, false /*proceed*/,
                              false /*showed_interstitial*/,
                              false /* has_post_commit_interstitial_skipped */);
    return;
  }

  prerender::NoStatePrefetchContents* no_state_prefetch_contents =
      delegate_->GetNoStatePrefetchContentsIfExists(web_contents);
  if (no_state_prefetch_contents) {
    no_state_prefetch_contents->Destroy(prerender::FINAL_STATUS_SAFE_BROWSING);
    // Tab is being prerendered.
    resource.DispatchCallback(FROM_HERE, false /*proceed*/,
                              false /*showed_interstitial*/,
                              false /* has_post_commit_interstitial_skipped */);
    return;
  }

  // Whether we have a FrameTreeNode id or a RenderFrameHost id depends on
  // whether SB was triggered for a frame navigation or a document's subresource
  // load respectively. We consider both cases here. Also, we need to cancel
  // corresponding prerenders for both case.
  content::RenderFrameHost* rfh = nullptr;
  if (resource.render_frame_token) {
    rfh = content::RenderFrameHost::FromFrameToken(
        content::GlobalRenderFrameHostToken(
            resource.render_process_id,
            blink::LocalFrameToken(resource.render_frame_token.value())));
  }

  // Handle subresource load in prerendered pages.
  if (rfh && rfh->GetLifecycleState() ==
                 content::RenderFrameHost::LifecycleState::kPrerendering) {
    // Cancel prerenders directly when its subresource or its subframe’s
    // subresource is unsafe.
    bool is_inactive = rfh->IsInactiveAndDisallowActivation(
        content::DisallowActivationReasonId::kSafeBrowsingUnsafeSubresource);
    CHECK(is_inactive);

    resource.DispatchCallback(FROM_HERE, false /*proceed*/,
                              false /*showed_interstitial*/,
                              false /* has_post_commit_interstitial_skipped */);
    return;
  }

  // Handle main frame or its sub frame navigation in prerendered pages.
  // TODO(crbug.com/40912417): For latter case, the cancellation of prerender is
  // currently done by canceling them with BLOCKED_BY_CLIENT in loader
  // throttle, because current implementation of Prerender cancels
  // prerenders when the navigation of prerender's subframes (not only the
  // main frame) are canceled with BLOCKED_BY_CLIENT, as the TODO comment
  // below also mentions. We plan to change the cancellation way from using
  // BLOCKED_BY_CLIENT as the subresource load case.
  if (web_contents->IsPrerenderedFrame(
          content::FrameTreeNodeId(resource.frame_tree_node_id))) {
    // TODO(mcnee): If we were to indicate that this does not show an
    // interstitial, the loader throttle would cancel with ERR_ABORTED to
    // suppress an error page, instead of blocking using ERR_BLOCKED_BY_CLIENT.
    // Prerendering code needs to distiguish these cases, so we pretend that
    // we've shown an interstitial to get a meaningful error code.
    // Given that the only thing the |showed_interstitial| parameter is used for
    // is controlling the error code, perhaps this should be renamed to better
    // indicate its purpose.
    resource.DispatchCallback(FROM_HERE, false /*proceed*/,
                              true /*showed_interstitial*/,
                              false /* has_post_commit_interstitial_skipped */);
    return;
  }

  // We don't show interstitials for extension triggered SB errors, since they
  // might not be visible, and cause the extension to hang. The request is just
  // cancelled instead.
  if (delegate_->IsHostingExtension(web_contents)) {
    resource.DispatchCallback(FROM_HERE, false /* proceed */,
                              false /* showed_interstitial */,
                              false /* has_post_commit_interstitial_skipped */);
    return;
  }

  // If the main frame load is still pending, we need to get the navigation URL
  // and referrer URL from the navigation entry now, since they are required for
  // threat reporting, and the entry will be destroyed once the request is
  // failed.
  if (AsyncCheckTracker::IsMainPageLoadPending(resource)) {
    content::NavigationEntry* entry =
        unsafe_resource_util::GetNavigationEntryForResource(resource);
    if (entry) {
      security_interstitials::UnsafeResource resource_copy(resource);
      resource_copy.navigation_url = entry->GetURL();
      resource_copy.referrer_url = entry->GetReferrer().url;
      DisplayBlockingPage(resource_copy);
      return;
    }
  }
  DisplayBlockingPage(resource);
}

bool SafeBrowsingUIManager::ShouldSendHitReport(HitReport* hit_report,
                                                WebContents* web_contents) {
  return web_contents &&
         hit_report->extended_reporting_level != SBER_LEVEL_OFF &&
         !web_contents->GetBrowserContext()->IsOffTheRecord() &&
         delegate_->IsSendingOfHitReportsEnabled();
}

bool SafeBrowsingUIManager::ShouldSendClientSafeBrowsingWarningShownReport(
    WebContents* web_contents) {
  if (!web_contents || !web_contents->GetBrowserContext()) {
    return false;
  }
  const auto& prefs = *delegate_->GetPrefs(web_contents->GetBrowserContext());
  return GetExtendedReportingLevel(prefs) != SBER_LEVEL_OFF &&
         !web_contents->GetBrowserContext()->IsOffTheRecord();
}

// A SafeBrowsing hit is sent after a blocking page for malware/phishing
// or after the warning dialog for download urls, only for
// extended-reporting users.
void SafeBrowsingUIManager::MaybeReportSafeBrowsingHit(
    std::unique_ptr<HitReport> hit_report,
    WebContents* web_contents) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  // Send report if user opted-in to extended reporting and is not in
  //  incognito mode.
  if (!ShouldSendHitReport(hit_report.get(), web_contents)) {
    return;
  }

  if (shut_down_)
    return;

  DVLOG(1) << "ReportSafeBrowsingHit: " << hit_report->malicious_url << " "
           << hit_report->page_url << " " << hit_report->referrer_url << " "
           << hit_report->is_subresource << " "
           << static_cast<int>(hit_report->threat_type);
  delegate_->GetPingManager(web_contents->GetBrowserContext())
      ->ReportSafeBrowsingHit(std::move(hit_report));
}

void SafeBrowsingUIManager::MaybeSendClientSafeBrowsingWarningShownReport(
    std::unique_ptr<ClientSafeBrowsingReportRequest> report,
    WebContents* web_contents) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  // Send report if user opted-in to extended reporting and is not in incognito
  // mode.
  if (ShouldSendClientSafeBrowsingWarningShownReport(web_contents)) {
    SendThreatDetails(web_contents->GetBrowserContext(), std::move(report));
  }
}

// Static.
void SafeBrowsingUIManager::CreateAllowlistForTesting(
    content::WebContents* web_contents) {
  EnsureAllowlistCreated(web_contents);
}

// static
std::string SafeBrowsingUIManager::GetThreatTypeStringForInterstitial(
    safe_browsing::SBThreatType threat_type) {
  using enum SBThreatType;

  switch (threat_type) {
    case SB_THREAT_TYPE_URL_PHISHING:
    case SB_THREAT_TYPE_URL_CLIENT_SIDE_PHISHING:
      return "SOCIAL_ENGINEERING";
    case SB_THREAT_TYPE_URL_MALWARE:
      return "MALWARE";
    case SB_THREAT_TYPE_URL_UNWANTED:
      return "UNWANTED_SOFTWARE";
    case SB_THREAT_TYPE_BILLING:
      return "THREAT_TYPE_UNSPECIFIED";
    case SB_THREAT_TYPE_MANAGED_POLICY_WARN:
      return "MANAGED_POLICY_WARN";
    case SB_THREAT_TYPE_MANAGED_POLICY_BLOCK:
      return "MANAGED_POLICY_BLOCK";
    case SB_THREAT_TYPE_UNUSED:
    case SB_THREAT_TYPE_SAFE:
    case SB_THREAT_TYPE_URL_BINARY_MALWARE:
    case SB_THREAT_TYPE_EXTENSION:
    case SB_THREAT_TYPE_API_ABUSE:
    case SB_THREAT_TYPE_SUBRESOURCE_FILTER:
    case SB_THREAT_TYPE_CSD_ALLOWLIST:
    case DEPRECATED_SB_THREAT_TYPE_URL_PASSWORD_PROTECTION_PHISHING:
    case DEPRECATED_SB_THREAT_TYPE_URL_CLIENT_SIDE_MALWARE:
    case SB_THREAT_TYPE_SAVED_PASSWORD_REUSE:
    case SB_THREAT_TYPE_SIGNED_IN_SYNC_PASSWORD_REUSE:
    case SB_THREAT_TYPE_SIGNED_IN_NON_SYNC_PASSWORD_REUSE:
    case SB_THREAT_TYPE_AD_SAMPLE:
    case SB_THREAT_TYPE_BLOCKED_AD_POPUP:
    case SB_THREAT_TYPE_BLOCKED_AD_REDIRECT:
    case SB_THREAT_TYPE_SUSPICIOUS_SITE:
    case SB_THREAT_TYPE_ENTERPRISE_PASSWORD_REUSE:
    case SB_THREAT_TYPE_APK_DOWNLOAD:
    case SB_THREAT_TYPE_HIGH_CONFIDENCE_ALLOWLIST:
      NOTREACHED_IN_MIGRATION();
      break;
  }
  return std::string();
}
void SafeBrowsingUIManager::AddObserver(Observer* observer) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  observer_list_.AddObserver(observer);
}

void SafeBrowsingUIManager::RemoveObserver(Observer* observer) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  observer_list_.RemoveObserver(observer);
}

const std::string SafeBrowsingUIManager::app_locale() const {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  return delegate_->GetApplicationLocale();
}

history::HistoryService* SafeBrowsingUIManager::history_service(
    content::WebContents* web_contents) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  return delegate_->GetHistoryService(web_contents->GetBrowserContext());
}

const GURL SafeBrowsingUIManager::default_safe_page() const {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  return default_safe_page_;
}

// If the user had opted-in to send ThreatDetails, this gets called
// when the report is ready.
void SafeBrowsingUIManager::SendThreatDetails(
    content::BrowserContext* browser_context,
    std::unique_ptr<ClientSafeBrowsingReportRequest> report) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  if (shut_down_)
    return;

  DVLOG(1) << "Sending threat details.";
  delegate_->GetPingManager(browser_context)
      ->ReportThreatDetails(std::move(report));
}

// If HaTS surveys are enabled, then this gets called when the report is ready.
void SafeBrowsingUIManager::AttachThreatDetailsAndLaunchSurvey(
    content::BrowserContext* browser_context,
    std::unique_ptr<ClientSafeBrowsingReportRequest> report) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  if (shut_down_) {
    return;
  }

  DVLOG(1) << "Adding threat details to survey response payload.";
  delegate_->GetPingManager(browser_context)
      ->AttachThreatDetailsAndLaunchSurvey(std::move(report));
}

void SafeBrowsingUIManager::OnBlockingPageDone(
    const std::vector<UnsafeResource>& resources,
    bool proceed,
    content::WebContents* web_contents,
    const GURL& main_frame_url,
    bool showed_interstitial) {
  BaseUIManager::OnBlockingPageDone(resources, proceed, web_contents,
                                    main_frame_url, showed_interstitial);
  if (proceed && !resources.empty()) {
#if !BUILDFLAG(IS_ANDROID)
    if (resources[0].threat_type ==
        SBThreatType::SB_THREAT_TYPE_MANAGED_POLICY_WARN) {
      delegate_->TriggerUrlFilteringInterstitialExtensionEventIfDesired(
          web_contents, main_frame_url, "ENTERPRISE_WARNED_BYPASS",
          resources[0].rt_lookup_response);
      return;
    }
#endif
    delegate_->TriggerSecurityInterstitialProceededExtensionEventIfDesired(
        web_contents, main_frame_url,
        GetThreatTypeStringForInterstitial(resources[0].threat_type),
        /*net_error_code=*/0);
  }
}

security_interstitials::SecurityInterstitialPage*
SafeBrowsingUIManager::CreateBlockingPage(
    content::WebContents* contents,
    const GURL& blocked_url,
    const UnsafeResource& unsafe_resource,
    bool forward_extension_event,
    std::optional<base::TimeTicks> blocked_page_shown_timestamp) {
  security_interstitials::SecurityInterstitialPage* blocking_page = nullptr;
#if !BUILDFLAG(IS_ANDROID)
  if (unsafe_resource.threat_type ==
      SBThreatType::SB_THREAT_TYPE_MANAGED_POLICY_WARN) {
    blocking_page = blocking_page_factory_->CreateEnterpriseWarnPage(
        this, contents, blocked_url, {unsafe_resource});

    // Report that we showed an interstitial.
    if (forward_extension_event) {
      ForwardUrlFilteringInterstitialExtensionEventToEmbedder(
          contents, blocked_url, "ENTERPRISE_WARNED_SEEN",
          unsafe_resource.rt_lookup_response);
    }
    return blocking_page;
  } else if (unsafe_resource.threat_type ==
             SBThreatType::SB_THREAT_TYPE_MANAGED_POLICY_BLOCK) {
    blocking_page = blocking_page_factory_->CreateEnterpriseBlockPage(
        this, contents, blocked_url, {unsafe_resource});

    // Report that we showed an interstitial.
    if (forward_extension_event) {
      ForwardUrlFilteringInterstitialExtensionEventToEmbedder(
          contents, blocked_url, "ENTERPRISE_BLOCKED_SEEN",
          unsafe_resource.rt_lookup_response);
    }
    return blocking_page;
  }
#endif  // !BUILDFLAG(IS_ANDROID)
  blocking_page = blocking_page_factory_->CreateSafeBrowsingPage(
      this, contents, blocked_url, {unsafe_resource},
      /*should_trigger_reporting=*/true, blocked_page_shown_timestamp);

  // Report that we showed an interstitial.
  if (forward_extension_event) {
    ForwardSecurityInterstitialShownExtensionEventToEmbedder(
        contents, blocked_url,
        GetThreatTypeStringForInterstitial(unsafe_resource.threat_type),
        /*net_error_code=*/0);
  }
  return blocking_page;
}

void SafeBrowsingUIManager::
    ForwardSecurityInterstitialShownExtensionEventToEmbedder(
        content::WebContents* web_contents,
        const GURL& page_url,
        const std::string& reason,
        int net_error_code) {
  delegate_->TriggerSecurityInterstitialShownExtensionEventIfDesired(
      web_contents, page_url, reason, net_error_code);
}
#if !BUILDFLAG(IS_ANDROID)
void SafeBrowsingUIManager::
    ForwardUrlFilteringInterstitialExtensionEventToEmbedder(
        content::WebContents* web_contents,
        const GURL& page_url,
        const std::string& threat_type,
        safe_browsing::RTLookupResponse rt_lookup_response) {
  delegate_->TriggerUrlFilteringInterstitialExtensionEventIfDesired(
      web_contents, page_url, threat_type, rt_lookup_response);
}
#endif
}  // namespace safe_browsing
