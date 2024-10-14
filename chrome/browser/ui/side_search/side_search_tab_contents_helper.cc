// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/side_search/side_search_tab_contents_helper.h"

#include "base/feature_list.h"
#include "base/metrics/histogram_functions.h"
#include "chrome/browser/extensions/chrome_extension_web_contents_observer.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/task_manager/web_contents_tags.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/prefs/prefs_tab_helper.h"
#include "chrome/browser/ui/side_search/side_search_config.h"
#include "chrome/browser/ui/side_search/side_search_metrics.h"
#include "chrome/browser/ui/side_search/side_search_utils.h"
#include "chrome/browser/ui/side_search/unified_side_search_helper.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/uma_browsing_activity_observer.h"
#include "components/sessions/content/session_tab_helper.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/browser/web_contents.h"
#include "net/http/http_request_headers.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/simple_url_loader.h"
#include "third_party/blink/public/mojom/frame/user_activation_notification_type.mojom.h"
#include "ui/views/controls/webview/web_contents_set_background_color.h"

#if BUILDFLAG(ENABLE_EXTENSIONS)
#include "chrome/browser/extensions/tab_helper.h"
#endif  // BUILDFLAG(ENABLE_EXTENSIONS)

SideSearchTabContentsHelper::~SideSearchTabContentsHelper() {
  // Record the number of times we navigated back to a previous SRP before
  // closing the tab. Only record this value if we actually navigated to a
  // search page URL at some point during the life of the tab.
  if (last_search_url_)
    RecordSideSearchNumTimesReturnedBackToSRP(returned_to_previous_srp_count_);
}

void SideSearchTabContentsHelper::NavigateInTabContents(
    const content::OpenURLParams& params) {
  web_contents()->GetPrimaryMainFrame()->NotifyUserActivation(
      blink::mojom::UserActivationNotificationType::kInteraction);
  web_contents()->GetController().LoadURLWithParams(
      content::NavigationController::LoadURLParams(params));
}

void SideSearchTabContentsHelper::LastSearchURLUpdated(const GURL& url) {
  DCHECK(GetConfig()->ShouldNavigateInSidePanel(url));
  last_search_url_ = url;
}

bool SideSearchTabContentsHelper::HandleKeyboardEvent(
    content::WebContents* source,
    const input::NativeWebKeyboardEvent& event) {
  return delegate_ ? delegate_->HandleKeyboardEvent(source, event) : false;
}

content::WebContents* SideSearchTabContentsHelper::GetTabWebContents() {
  return web_contents();
}

void SideSearchTabContentsHelper::CarryOverSideSearchStateToNewTab(
    const GURL& search_url,
    content::WebContents* new_web_contents) {
  DCHECK(new_web_contents);

  // Ensure this function is called on a search result page.
  if (GetConfig()->ShouldNavigateInSidePanel(search_url)) {
    auto* new_helper =
        SideSearchTabContentsHelper::FromWebContents(new_web_contents);

    // "Open link in incognito window" yields a null new_helper.
    if (new_helper) {
      new_helper->last_search_url_ = search_url;
    }
  }
}

content::WebContents* SideSearchTabContentsHelper::OpenURLFromTab(
    content::WebContents* source,
    const content::OpenURLParams& params,
    base::OnceCallback<void(content::NavigationHandle&)>
        navigation_handle_callback) {
  return delegate_ ? delegate_->OpenURLFromTab(
                         source, params, std::move(navigation_handle_callback))
                   : nullptr;
}

void SideSearchTabContentsHelper::DidOpenRequestedURL(
    content::WebContents* new_contents,
    content::RenderFrameHost* source_render_frame_host,
    const GURL& url,
    const content::Referrer& referrer,
    WindowOpenDisposition disposition,
    ui::PageTransition transition,
    bool started_from_context_menu,
    bool renderer_initiated) {
  const GURL& current_url = GetTabWebContents()->GetLastCommittedURL();
  CarryOverSideSearchStateToNewTab(current_url, new_contents);
}

void SideSearchTabContentsHelper::DidFinishNavigation(
    content::NavigationHandle* navigation_handle) {
  if (!navigation_handle->IsInPrimaryMainFrame() ||
      navigation_handle->IsSameDocument() ||
      !navigation_handle->HasCommitted()) {
    return;
  }

  const auto& url = navigation_handle->GetURL();
  auto* config = GetConfig();

  if (config->ShouldNavigateInSidePanel(url)) {
    // Keep track of how many times a user returned to the `last_search_url_`
    // via back-navigation. Reset the count if navigating to a new SRP or
    // forward through history to an existing SRP.
    if (navigation_handle->GetNavigationEntryOffset() < 0 &&
        url == last_search_url_) {
      ++returned_to_previous_srp_count_;
    } else {
      // Record the number of times the user navigated to the previous SRP
      // before resetting the value. Do not do so if this is the first
      // navigation to a SRP in this tab.
      if (last_search_url_.has_value()) {
        RecordSideSearchNumTimesReturnedBackToSRP(
            returned_to_previous_srp_count_);
      }

      returned_to_previous_srp_count_ = 0;
    }

    // Capture the URL here in case the side contents is closed before the
    // navigation completes.
    last_search_url_ = url;

    if (side_panel_contents_)
      UpdateSideContentsNavigation();
  }

  // Trigger the timer only when the side panel first becomes available. The
  // timer should only be cleared when the side panel is no longer available.
  if (!could_show_for_last_committed_navigation_ &&
      CanShowSidePanelForCommittedNavigation()) {
    available_timer_ = base::ElapsedTimer();
  } else if (!CanShowSidePanelForCommittedNavigation()) {
    available_timer_.reset();
  }
  could_show_for_last_committed_navigation_ =
      CanShowSidePanelForCommittedNavigation();
}

void SideSearchTabContentsHelper::OnSideSearchConfigChanged() {
  ClearHelperState();
}

void SideSearchTabContentsHelper::SidePanelProcessGone() {
  ClearSidePanelContents();
  // For state-per-tab we want to toggle the helper closed to ensure its toggled
  // state is updated correctly in the case the renderer crashes but it is not
  // currently being hosted in the side panel.
  toggled_open_ = false;
  if (delegate_)
    delegate_->SidePanelAvailabilityChanged(true);
}

content::WebContents* SideSearchTabContentsHelper::GetSidePanelContents() {
  if (!side_panel_contents_)
    CreateSidePanelContents();

  DCHECK(side_panel_contents_);
  UpdateSideContentsNavigation();
  return side_panel_contents_.get();
}

void SideSearchTabContentsHelper::SetAutoTriggered(bool auto_triggered) {
  if (!side_panel_contents_)
    return;
  GetSideContentsHelper()->set_auto_triggered(auto_triggered);
}

void SideSearchTabContentsHelper::ClearSidePanelContents() {
  // It is safe to reset this here as any views::WebViews hosting this
  // WebContents will clear their reference to this away during its destruction.
  side_panel_contents_.reset();
}

bool SideSearchTabContentsHelper::CanShowSidePanelForCommittedNavigation() {
  const GURL& url = web_contents()->GetLastCommittedURL();
  return last_search_url_ && GetConfig()->CanShowSidePanelForURL(url);
}

void SideSearchTabContentsHelper::
    MaybeRecordDurationSidePanelAvailableToFirstOpen() {
  if (!available_timer_)
    return;
  base::UmaHistogramMediumTimes(
      "SideSearch.TimeSinceSidePanelAvailableToFirstOpen",
      available_timer_->Elapsed());
  available_timer_.reset();
}

void SideSearchTabContentsHelper::SetDelegate(
    base::WeakPtr<Delegate> delegate) {
  delegate_ = std::move(delegate);
}

void SideSearchTabContentsHelper::SetSidePanelContentsForTesting(
    std::unique_ptr<content::WebContents> side_panel_contents) {
  side_panel_contents_ = std::move(side_panel_contents);
  SideSearchSideContentsHelper::CreateForWebContents(
      side_panel_contents_.get());
  GetSideContentsHelper()->SetDelegate(this);
}

SideSearchTabContentsHelper::SideSearchTabContentsHelper(
    content::WebContents* web_contents)
    : content::WebContentsObserver(web_contents),
      content::WebContentsUserData<SideSearchTabContentsHelper>(*web_contents) {
  config_observation_.Observe(GetConfig());
  CreateUnifiedSideSearchController(this, web_contents);
}

SideSearchSideContentsHelper*
SideSearchTabContentsHelper::GetSideContentsHelper() {
  DCHECK(side_panel_contents_);
  return SideSearchSideContentsHelper::FromWebContents(
      side_panel_contents_.get());
}

void SideSearchTabContentsHelper::CreateSidePanelContents() {
  DCHECK(!side_panel_contents_);
  side_panel_contents_ =
      content::WebContents::Create(content::WebContents::CreateParams(
          web_contents()->GetBrowserContext(), nullptr));

  // Apply a transparent background color so that we fallback to the hosting
  // side panel view's background color.
  views::WebContentsSetBackgroundColor::CreateForWebContentsWithColor(
      side_panel_contents_.get(), SK_ColorTRANSPARENT);

  task_manager::WebContentsTags::CreateForTabContents(
      side_panel_contents_.get());

  // Sets helpers required for the side contents.
  PrefsTabHelper::CreateForWebContents(side_panel_contents_.get());
#if BUILDFLAG(ENABLE_EXTENSIONS)
  extensions::TabHelper::CreateForWebContents(side_panel_contents_.get());
#endif  // BUILDFLAG(ENABLE_EXTENSIONS)
  chrome::UMABrowsingActivityObserver::TabHelper::CreateForWebContents(
      side_panel_contents_.get());

  SideSearchSideContentsHelper::CreateForWebContents(
      side_panel_contents_.get());
  GetSideContentsHelper()->SetDelegate(this);
}

void SideSearchTabContentsHelper::UpdateSideContentsNavigation() {
  DCHECK(side_panel_contents_);
  // Only update the side panel contents with the latest `last_search_url_` if
  // present.
  if (last_search_url_) {
    GetSideContentsHelper()->LoadURL(last_search_url_.value());
    side_search::MaybeSaveSideSearchTabSessionData(web_contents());
  }
}

void SideSearchTabContentsHelper::ClearHelperState() {
  toggled_open_ = false;
  last_search_url_.reset();
  returned_to_previous_srp_count_ = 0;
  toggled_open_ = false;

  // Notify the side panel after resetting the above state but before clearing
  // away the side panel WebContents. This will close the side panel if it's
  // currently open.
  if (delegate_)
    delegate_->SidePanelAvailabilityChanged(true);

  ClearSidePanelContents();
}

SideSearchConfig* SideSearchTabContentsHelper::GetConfig() {
  return SideSearchConfig::Get(web_contents()->GetBrowserContext());
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(SideSearchTabContentsHelper);
