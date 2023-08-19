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
#include "components/no_state_prefetch/browser/no_state_prefetch_contents.h"
#include "components/prefs/pref_service.h"
#include "components/safe_browsing/content/browser/safe_browsing_blocking_page.h"
#include "components/safe_browsing/content/browser/threat_details.h"
#include "components/safe_browsing/core/browser/db/v4_protocol_manager_util.h"
#include "components/safe_browsing/core/browser/ping_manager.h"
#include "components/safe_browsing/core/common/features.h"
#include "components/safe_browsing/core/common/safe_browsing_prefs.h"
#include "components/security_interstitials/content/security_interstitial_tab_helper.h"
#include "components/security_interstitials/content/unsafe_resource_util.h"
#include "components/security_interstitials/core/unsafe_resource.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/global_routing_id.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/web_contents.h"
#include "ipc/ipc_message.h"
#include "url/gurl.h"

using content::BrowserThread;
using content::NavigationEntry;
using content::WebContents;
using safe_browsing::HitReport;
using safe_browsing::SBThreatType;

namespace safe_browsing {

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
      security_interstitials::GetWebContentsForResource(resource);
  DCHECK(web_contents);
  std::unique_ptr<HitReport> hit_report = std::make_unique<HitReport>();
  hit_report->malicious_url = resource.url;
  hit_report->is_subresource = resource.is_subresource;
  hit_report->threat_type = resource.threat_type;
  hit_report->threat_source = resource.threat_source;
  hit_report->population_id = resource.threat_metadata.population_id;

  NavigationEntry* entry = GetNavigationEntryForResource(resource);
  if (entry) {
    hit_report->page_url = entry->GetURL();
    hit_report->referrer_url = entry->GetReferrer().url;
  }

  // When the malicious url is on the main frame, and resource.original_url
  // is not the same as the resource.url, that means we have a redirect from
  // resource.original_url to resource.url.
  // Also, at this point, page_url points to the _previous_ page that we
  // were on. We replace page_url with resource.original_url and referrer
  // with page_url.
  if (!resource.is_subresource && !resource.original_url.is_empty() &&
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

void SafeBrowsingUIManager::StartDisplayingBlockingPage(
    const security_interstitials::UnsafeResource& resource) {
  content::WebContents* web_contents =
      security_interstitials::GetWebContentsForResource(resource);

  if (!web_contents) {
    // Tab is gone.
    resource.DispatchCallback(FROM_HERE, false /*proceed*/,
                              false /*showed_interstitial*/);
    return;
  }

  prerender::NoStatePrefetchContents* no_state_prefetch_contents =
      delegate_->GetNoStatePrefetchContentsIfExists(web_contents);
  if (no_state_prefetch_contents) {
    no_state_prefetch_contents->Destroy(prerender::FINAL_STATUS_SAFE_BROWSING);
    // Tab is being prerendered.
    resource.DispatchCallback(FROM_HERE, false /*proceed*/,
                              false /*showed_interstitial*/);
    return;
  }

  // Whether we have a FrameTreeNode id or a RenderFrameHost id depends on
  // whether SB was triggered for a frame navigation or a document's subresource
  // load respectively. We consider both cases here.
  const content::GlobalRenderFrameHostId rfh_id(resource.render_process_id,
                                                resource.render_frame_id);
  content::RenderFrameHost* rfh = content::RenderFrameHost::FromID(rfh_id);
  const bool is_prerender =
      web_contents->IsPrerenderedFrame(resource.frame_tree_node_id) ||
      (rfh && rfh->GetLifecycleState() ==
                  content::RenderFrameHost::LifecycleState::kPrerendering);

  if (is_prerender) {
    // TODO(mcnee): If we were to indicate that this does not show an
    // interstitial, the loader throttle would cancel with ERR_ABORTED to
    // suppress an error page, instead of blocking using ERR_BLOCKED_BY_CLIENT.
    // Prerendering code needs to distiguish these cases, so we pretend that
    // we've shown an interstitial to get a meaningful error code.
    // Given that the only thing the |showed_interstitial| parameter is used for
    // is controlling the error code, perhaps this should be renamed to better
    // indicate its purpose.
    resource.DispatchCallback(FROM_HERE, false /*proceed*/,
                              true /*showed_interstitial*/);
    return;
  }

  // We don't show interstitials for extension triggered SB errors, since they
  // might not be visible, and cause the extension to hang. The request is just
  // cancelled instead.
  if (delegate_->IsHostingExtension(web_contents)) {
    resource.DispatchCallback(FROM_HERE, false /* proceed */,
                              false /* showed_interstitial */);
    return;
  }

  // With committed interstitials, if this is a main frame load, we need to
  // get the navigation URL and referrer URL from the navigation entry now,
  // since they are required for threat reporting, and the entry will be
  // destroyed once the request is failed.
  if (resource.IsMainPageLoadBlocked()) {
    content::NavigationEntry* entry =
        security_interstitials::GetNavigationEntryForResource(resource);
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

void SafeBrowsingUIManager::CheckLookupMechanismExperimentEligibility(
    security_interstitials::UnsafeResource resource,
    base::OnceCallback<void(bool)> callback,
    scoped_refptr<base::SequencedTaskRunner> callback_task_runner) {
  content::WebContents* web_contents =
      security_interstitials::GetWebContentsForResource(resource);
  auto determine_if_is_prerender = [resource, web_contents]() {
    const content::GlobalRenderFrameHostId rfh_id(resource.render_process_id,
                                                  resource.render_frame_id);
    content::RenderFrameHost* rfh = content::RenderFrameHost::FromID(rfh_id);
    return web_contents->IsPrerenderedFrame(resource.frame_tree_node_id) ||
           (rfh && rfh->GetLifecycleState() ==
                       content::RenderFrameHost::LifecycleState::kPrerendering);
  };
  // These checks parallel the ones performed by StartDisplayingBlockingPage to
  // determine if a blocking page would be shown for a mainframe URL. The
  // experiment is only eligible if the blocking page would be shown.
  bool is_ineligible =
      !web_contents ||
      delegate_->GetNoStatePrefetchContentsIfExists(web_contents) ||
      determine_if_is_prerender() ||
      delegate_->IsHostingExtension(web_contents) || IsAllowlisted(resource);
  callback_task_runner->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), !is_ineligible));
}

void SafeBrowsingUIManager::CheckExperimentEligibilityAndStartBlockingPage(
    security_interstitials::UnsafeResource resource,
    base::OnceCallback<void(bool)> callback,
    scoped_refptr<base::SequencedTaskRunner> callback_task_runner) {
  CheckLookupMechanismExperimentEligibility(resource, std::move(callback),
                                            callback_task_runner);
  StartDisplayingBlockingPage(resource);
}

bool SafeBrowsingUIManager::ShouldSendHitReport(HitReport* hit_report,
                                                WebContents* web_contents) {
  return web_contents &&
         hit_report->extended_reporting_level != SBER_LEVEL_OFF &&
         !web_contents->GetBrowserContext()->IsOffTheRecord() &&
         delegate_->IsSendingOfHitReportsEnabled();
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
           << hit_report->is_subresource << " " << hit_report->threat_type;
  delegate_->GetPingManager(web_contents->GetBrowserContext())
      ->ReportSafeBrowsingHit(std::move(hit_report));
}

// Static.
void SafeBrowsingUIManager::CreateAllowlistForTesting(
    content::WebContents* web_contents) {
  EnsureAllowlistCreated(web_contents);
}

// static
std::string SafeBrowsingUIManager::GetThreatTypeStringForInterstitial(
    safe_browsing::SBThreatType threat_type) {
  switch (threat_type) {
    case safe_browsing::SB_THREAT_TYPE_URL_PHISHING:
    case safe_browsing::SB_THREAT_TYPE_URL_CLIENT_SIDE_PHISHING:
      return "SOCIAL_ENGINEERING";
    case safe_browsing::SB_THREAT_TYPE_URL_MALWARE:
    case safe_browsing::SB_THREAT_TYPE_URL_CLIENT_SIDE_MALWARE:
      return "MALWARE";
    case safe_browsing::SB_THREAT_TYPE_URL_UNWANTED:
      return "UNWANTED_SOFTWARE";
    case safe_browsing::SB_THREAT_TYPE_BILLING:
      return "THREAT_TYPE_UNSPECIFIED";
    case safe_browsing::SB_THREAT_TYPE_MANAGED_POLICY_WARN:
      return "MANAGED_POLICY_WARN";
    case safe_browsing::SB_THREAT_TYPE_MANAGED_POLICY_BLOCK:
      return "MANAGED_POLICY_BLOCK";
    case safe_browsing::SB_THREAT_TYPE_UNUSED:
    case safe_browsing::SB_THREAT_TYPE_SAFE:
    case safe_browsing::SB_THREAT_TYPE_URL_BINARY_MALWARE:
    case safe_browsing::SB_THREAT_TYPE_EXTENSION:
    case safe_browsing::SB_THREAT_TYPE_BLOCKLISTED_RESOURCE:
    case safe_browsing::SB_THREAT_TYPE_API_ABUSE:
    case safe_browsing::SB_THREAT_TYPE_SUBRESOURCE_FILTER:
    case safe_browsing::SB_THREAT_TYPE_CSD_ALLOWLIST:
    case safe_browsing::
        DEPRECATED_SB_THREAT_TYPE_URL_PASSWORD_PROTECTION_PHISHING:
    case safe_browsing::SB_THREAT_TYPE_SAVED_PASSWORD_REUSE:
    case safe_browsing::SB_THREAT_TYPE_SIGNED_IN_SYNC_PASSWORD_REUSE:
    case safe_browsing::SB_THREAT_TYPE_SIGNED_IN_NON_SYNC_PASSWORD_REUSE:
    case safe_browsing::SB_THREAT_TYPE_AD_SAMPLE:
    case safe_browsing::SB_THREAT_TYPE_BLOCKED_AD_POPUP:
    case safe_browsing::SB_THREAT_TYPE_BLOCKED_AD_REDIRECT:
    case safe_browsing::SB_THREAT_TYPE_SUSPICIOUS_SITE:
    case safe_browsing::SB_THREAT_TYPE_ENTERPRISE_PASSWORD_REUSE:
    case safe_browsing::SB_THREAT_TYPE_APK_DOWNLOAD:
    case safe_browsing::SB_THREAT_TYPE_HIGH_CONFIDENCE_ALLOWLIST:
      NOTREACHED();
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
    if (base::FeatureList::IsEnabled((kRealTimeUrlFilteringForEnterprise)) &&
        resources[0].threat_type ==
            safe_browsing::SB_THREAT_TYPE_MANAGED_POLICY_WARN) {
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

// Static.
GURL SafeBrowsingUIManager::GetMainFrameAllowlistUrlForResourceForTesting(
    const security_interstitials::UnsafeResource& resource) {
  return GetMainFrameAllowlistUrlForResource(resource);
}

BaseBlockingPage* SafeBrowsingUIManager::CreateBlockingPageForSubresource(
    content::WebContents* contents,
    const GURL& blocked_url,
    const UnsafeResource& unsafe_resource) {
  SafeBrowsingBlockingPage* blocking_page =
      blocking_page_factory_->CreateSafeBrowsingPage(
          this, contents, blocked_url, {unsafe_resource},
          /*should_trigger_reporting=*/true);

  // Report that we showed an interstitial.
  ForwardSecurityInterstitialShownExtensionEventToEmbedder(
      contents, blocked_url,
      GetThreatTypeStringForInterstitial(unsafe_resource.threat_type),
      /*net_error_code=*/0);

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
