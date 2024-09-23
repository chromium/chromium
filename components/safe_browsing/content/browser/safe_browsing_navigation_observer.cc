// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/safe_browsing/content/browser/safe_browsing_navigation_observer.h"

#include <memory>

#include "base/metrics/histogram_functions.h"
#include "base/time/time.h"
#include "components/page_info/page_info_ui.h"
#include "components/safe_browsing/content/browser/safe_browsing_navigation_observer_manager.h"
#include "components/sessions/content/session_tab_helper.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/global_routing_id.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/web_contents.h"
#include "net/base/ip_endpoint.h"
#include "url/url_constants.h"

using content::WebContents;

namespace {
const char kWebContentsUserDataKey[] =
    "web_contents_safe_browsing_navigation_observer";
}  // namespace

namespace safe_browsing {

// SafeBrowsingNavigationObserver::NavigationEvent-----------------------------
NavigationEvent::NavigationEvent()
    : source_tab_id(SessionID::InvalidValue()),
      target_tab_id(SessionID::InvalidValue()),
      last_updated(base::Time::Now()),
      navigation_initiation(ReferrerChainEntry::UNDEFINED),
      has_committed(false),
      maybe_launched_by_external_application() {}

NavigationEvent::NavigationEvent(NavigationEvent&& nav_event) = default;
NavigationEvent::NavigationEvent(const NavigationEvent& nav_event) = default;
NavigationEvent& NavigationEvent::operator=(NavigationEvent&& nav_event) =
    default;

NavigationEvent::~NavigationEvent() = default;

// SafeBrowsingNavigationObserver --------------------------------------------

// static
void SafeBrowsingNavigationObserver::MaybeCreateForWebContents(
    content::WebContents* web_contents,
    HostContentSettingsMap* host_content_settings_map,
    SafeBrowsingNavigationObserverManager* observer_manager,
    PrefService* prefs,
    bool has_safe_browsing_service) {
  if (FromWebContents(web_contents))
    return;

  if (safe_browsing::SafeBrowsingNavigationObserverManager::IsEnabledAndReady(
          prefs, has_safe_browsing_service)) {
    web_contents->SetUserData(
        kWebContentsUserDataKey,
        std::make_unique<SafeBrowsingNavigationObserver>(
            web_contents, host_content_settings_map, observer_manager));
  }
}

// static
SafeBrowsingNavigationObserver* SafeBrowsingNavigationObserver::FromWebContents(
    content::WebContents* web_contents) {
  return static_cast<SafeBrowsingNavigationObserver*>(
      web_contents->GetUserData(kWebContentsUserDataKey));
}

SafeBrowsingNavigationObserver::SafeBrowsingNavigationObserver(
    content::WebContents* contents,
    HostContentSettingsMap* host_content_settings_map,
    SafeBrowsingNavigationObserverManager* observer_manager)
    : content::WebContentsObserver(contents),
      observer_manager_(observer_manager) {
  content_settings_observation_.Observe(host_content_settings_map);
}

SafeBrowsingNavigationObserver::~SafeBrowsingNavigationObserver() = default;

void SafeBrowsingNavigationObserver::OnUserInteraction() {
  GetObserverManager()->RecordUserGestureForWebContents(web_contents());
}

// Called when a navigation starts in the WebContents. |navigation_handle|
// parameter is unique to this navigation, which will appear in the following
// DidRedirectNavigation, and DidFinishNavigation too.
void SafeBrowsingNavigationObserver::DidStartNavigation(
    content::NavigationHandle* navigation_handle) {
  // Treat a browser-initiated navigation as a user interaction.
  if (!navigation_handle->IsRendererInitiated())
    OnUserInteraction();

  // Ignores navigation caused by back/forward.
  if (navigation_handle->GetPageTransition() &
      ui::PAGE_TRANSITION_FORWARD_BACK) {
    return;
  }

  // Ignores reloads
  if (ui::PageTransitionCoreTypeIs(navigation_handle->GetPageTransition(),
                                   ui::PAGE_TRANSITION_RELOAD)) {
    return;
  }

  std::unique_ptr<NavigationEvent> nav_event =
      std::make_unique<NavigationEvent>();
  SetNavigationInitiationAndRecordUserGesture(navigation_handle,
                                              nav_event.get());
  // All the other fields are reconstructed based on current content of
  // navigation_handle.
  SetNavigationSourceUrl(navigation_handle, nav_event.get());
  nav_event->original_request_url =
      SafeBrowsingNavigationObserverManager::ClearURLRef(
          navigation_handle->GetURL());
  nav_event->source_tab_id =
      sessions::SessionTabHelper::IdForTab(navigation_handle->GetWebContents());
  SetNavigationSourceMainFrameUrl(navigation_handle, nav_event.get());
  SetNavigationOutermostMainFrameIds(navigation_handle, nav_event.get());

  std::unique_ptr<NavigationEvent> pending_nav_event =
      std::make_unique<NavigationEvent>(*nav_event);
  navigation_handle_map_[navigation_handle] = std::move(nav_event);
  GetObserverManager()->RecordPendingNavigationEvent(
      navigation_handle, std::move(pending_nav_event));
}

void SafeBrowsingNavigationObserver::DidRedirectNavigation(
    content::NavigationHandle* navigation_handle) {
  // Log whether a tel scheme was observed.
  base::UmaHistogramBoolean(
      "SafeBrowsing.NavigationObserver.RedirectForTelScheme",
      navigation_handle->GetURL().SchemeIs(url::kTelScheme));
  // We should have already seen this navigation_handle in DidStartNavigation.
  if (navigation_handle_map_.find(navigation_handle) ==
      navigation_handle_map_.end()) {
    return;
  }
  NavigationEvent* nav_event = navigation_handle_map_[navigation_handle].get();
  nav_event->server_redirect_urls.push_back(
      SafeBrowsingNavigationObserverManager::ClearURLRef(
          navigation_handle->GetURL()));
  nav_event->last_updated = base::Time::Now();

  GetObserverManager()->AddRedirectUrlToPendingNavigationEvent(
      navigation_handle, navigation_handle->GetURL());
}

void SafeBrowsingNavigationObserver::DidFinishNavigation(
    content::NavigationHandle* navigation_handle) {
  if ((navigation_handle->HasCommitted() || navigation_handle->IsDownload()) &&
      !navigation_handle->GetSocketAddress().address().empty()) {
    GetObserverManager()->RecordHostToIpMapping(
        navigation_handle->GetURL().host(),
        navigation_handle->GetSocketAddress().ToStringWithoutPort());
  }

  if (navigation_handle_map_.find(navigation_handle) ==
      navigation_handle_map_.end()) {
    return;
  }

  // If it is an error page, we ignore this navigation.
  if (navigation_handle->IsErrorPage()) {
    navigation_handle_map_.erase(navigation_handle);
    return;
  }
  NavigationEvent* nav_event = navigation_handle_map_[navigation_handle].get();

  nav_event->maybe_launched_by_external_application =
      PageTransitionCoreTypeIs(navigation_handle->GetPageTransition(),
                               ui::PAGE_TRANSITION_AUTO_TOPLEVEL);
  nav_event->has_committed = navigation_handle->HasCommitted();
  nav_event->target_tab_id =
      sessions::SessionTabHelper::IdForTab(navigation_handle->GetWebContents());

  if (nav_event->is_outermost_main_frame && nav_event->has_committed) {
    auto* rfh = navigation_handle->GetRenderFrameHost();
    // We set the outermost main frame id here rather than in DidStartNavigation
    // because in that function, we don't yet have a RenderFrameHost, so if
    // we're the outermost main frame, we won't yet have an id.
    nav_event->outermost_main_frame_id =
        rfh->GetOutermostMainFrame()->GetGlobalId();
  }
  DCHECK(!nav_event->has_committed || nav_event->outermost_main_frame_id);

  nav_event->last_updated = base::Time::Now();

  GetObserverManager()->RecordNavigationEvent(
      navigation_handle, std::move(navigation_handle_map_[navigation_handle]));
  navigation_handle_map_.erase(navigation_handle);
}

void SafeBrowsingNavigationObserver::DidGetUserInteraction(
    const blink::WebInputEvent& event) {
  OnUserInteraction();
}

void SafeBrowsingNavigationObserver::WebContentsDestroyed() {
  GetObserverManager()->OnWebContentDestroyed(web_contents());
  web_contents()->RemoveUserData(kWebContentsUserDataKey);
  // web_contents is null after this function.
}

void SafeBrowsingNavigationObserver::DidOpenRequestedURL(
    content::WebContents* new_contents,
    content::RenderFrameHost* source_render_frame_host,
    const GURL& url,
    const content::Referrer& referrer,
    WindowOpenDisposition disposition,
    ui::PageTransition transition,
    bool started_from_context_menu,
    bool renderer_initiated) {
  GetObserverManager()->RecordNewWebContents(
      web_contents(), source_render_frame_host, url, transition, new_contents,
      renderer_initiated);
}

void SafeBrowsingNavigationObserver::OnContentSettingChanged(
    const ContentSettingsPattern& primary_pattern,
    const ContentSettingsPattern& secondary_pattern,
    ContentSettingsTypeSet content_type_set) {
  // For all the content settings that can be changed via page info UI, we
  // assume there is a user gesture associated with the content setting change.
  if (web_contents() && !primary_pattern.MatchesAllHosts() &&
      primary_pattern.Matches(web_contents()->GetLastCommittedURL()) &&
      (content_type_set.ContainsAllTypes() ||
       PageInfoUI::ContentSettingsTypeInPageInfo(content_type_set.GetType()))) {
    OnUserInteraction();
  }
}

void SafeBrowsingNavigationObserver::
    SetNavigationInitiationAndRecordUserGesture(
        content::NavigationHandle* navigation_handle,
        NavigationEvent* nav_event) {
  auto it = navigation_handle_map_.find(navigation_handle);
  // It is possible to see multiple DidStartNavigation(..) with the same
  // navigation_handle (e.g. cross-process transfer). If that's the case,
  // we need to copy the navigation_initiation field.
  if (it != navigation_handle_map_.end() &&
      it->second->navigation_initiation != ReferrerChainEntry::UNDEFINED) {
    nav_event->navigation_initiation = it->second->navigation_initiation;
  } else {
    // If this is the first time we see this navigation_handle, decide if it is
    // triggered by user.
    if (!navigation_handle->IsRendererInitiated()) {
      nav_event->navigation_initiation = ReferrerChainEntry::BROWSER_INITIATED;
    } else if (GetObserverManager()->HasUnexpiredUserGesture(web_contents())) {
      nav_event->navigation_initiation =
          ReferrerChainEntry::RENDERER_INITIATED_WITH_USER_GESTURE;
    } else {
      nav_event->navigation_initiation =
          ReferrerChainEntry::RENDERER_INITIATED_WITHOUT_USER_GESTURE;
    }
    GetObserverManager()->OnUserGestureConsumed(web_contents());
  }
}

void SafeBrowsingNavigationObserver::SetNavigationSourceUrl(
    content::NavigationHandle* navigation_handle,
    NavigationEvent* nav_event) {
  // For browser initiated navigation (e.g. from address bar or bookmark), we
  // don't fill the source_url to prevent attributing navigation to the last
  // committed navigation.
  if (!navigation_handle->IsRendererInitiated())
    return;

  if (navigation_handle->IsInPrerenderedMainFrame()) {
    // A prerendered page can only be initiated by the primary page, so we will
    // grab the last committed URL from the primary main frame directly. Note
    // that this cannot be obtained from the previous RFH since this is empty
    // for prerenders. While this differs from the semantics for source_url
    // (which is indeed related to the last committed URL in the frame into
    // which we are navigating), it matches what source_url would have been had
    // we been navigating normally since we will activate in the primary main
    // frame.
    nav_event->source_url = SafeBrowsingNavigationObserverManager::ClearURLRef(
        navigation_handle->GetWebContents()
            ->GetPrimaryMainFrame()
            ->GetLastCommittedURL());
  } else {
    // If there was a URL previously committed in the current RenderFrameHost,
    // set it as the source url of this navigation. Otherwise, this is the
    // first url going to commit in this frame.
    content::RenderFrameHost* current_frame_host =
        content::RenderFrameHost::FromID(
            navigation_handle->GetPreviousRenderFrameHostId());
    if (current_frame_host &&
        current_frame_host->GetLastCommittedURL().is_valid()) {
      nav_event->source_url =
          SafeBrowsingNavigationObserverManager::ClearURLRef(
              current_frame_host->GetLastCommittedURL());
    }
  }
}

void SafeBrowsingNavigationObserver::SetNavigationSourceMainFrameUrl(
    content::NavigationHandle* navigation_handle,
    NavigationEvent* nav_event) {
  if (!navigation_handle->GetParentFrameOrOuterDocument()) {
    nav_event->source_main_frame_url = nav_event->source_url;
  } else {
    nav_event->source_main_frame_url =
        SafeBrowsingNavigationObserverManager::ClearURLRef(
            navigation_handle->GetParentFrameOrOuterDocument()
                ->GetOutermostMainFrame()
                ->GetLastCommittedURL());
  }
}

void SafeBrowsingNavigationObserver::SetNavigationOutermostMainFrameIds(
    content::NavigationHandle* navigation_handle,
    NavigationEvent* nav_event) {
  auto* outer_rfh = navigation_handle->GetParentFrameOrOuterDocument();
  nav_event->is_outermost_main_frame = !outer_rfh;

  if (outer_rfh) {
    nav_event->outermost_main_frame_id =
        outer_rfh->GetOutermostMainFrame()->GetGlobalId();
  }

  if (navigation_handle->IsRendererInitiated()) {
    auto* initiator_frame_host =
        navigation_handle->GetInitiatorFrameToken().has_value()
            ? content::RenderFrameHost::FromFrameToken(
                  content::GlobalRenderFrameHostToken(
                      navigation_handle->GetInitiatorProcessId(),
                      navigation_handle->GetInitiatorFrameToken().value()))
            : nullptr;
    if (initiator_frame_host) {
      nav_event->initiator_outermost_main_frame_id =
          initiator_frame_host->GetOutermostMainFrame()->GetGlobalId();
    }
  }
}

SafeBrowsingNavigationObserverManager*
SafeBrowsingNavigationObserver::GetObserverManager() {
  return observer_manager_;
}

void SafeBrowsingNavigationObserver::SetObserverManagerForTesting(
    SafeBrowsingNavigationObserverManager* observer_manager) {
  observer_manager_ = observer_manager;
}

}  // namespace safe_browsing
