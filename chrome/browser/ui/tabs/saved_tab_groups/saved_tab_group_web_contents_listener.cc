// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/tabs/saved_tab_groups/saved_tab_group_web_contents_listener.h"

#include "chrome/browser/favicon/favicon_utils.h"
#include "chrome/browser/tab_group_sync/tab_group_sync_tab_state.h"
#include "chrome/browser/ui/tabs/saved_tab_groups/saved_tab_group_keyed_service.h"
#include "chrome/browser/ui/tabs/saved_tab_groups/saved_tab_group_utils.h"
#include "components/favicon/content/content_favicon_driver.h"
#include "components/saved_tab_groups/saved_tab_group.h"
#include "components/saved_tab_groups/saved_tab_group_model.h"
#include "components/saved_tab_groups/saved_tab_group_tab.h"
#include "content/public/browser/favicon_status.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/web_contents.h"
#include "ui/base/page_transition_types.h"

namespace tab_groups {
namespace {

bool IsSaveableNavigation(content::NavigationHandle* navigation_handle) {
  ui::PageTransition page_transition = navigation_handle->GetPageTransition();
  if (navigation_handle->IsPost()) {
    return false;
  }
  if (!ui::IsValidPageTransitionType(page_transition)) {
    return false;
  }
  if (ui::PageTransitionIsRedirect(page_transition)) {
    return false;
  }

  if (!ui::PageTransitionIsMainFrame(page_transition)) {
    return false;
  }

  if (!navigation_handle->HasCommitted()) {
    return false;
  }

  if (!navigation_handle->ShouldUpdateHistory()) {
    return false;
  }

  // For renderer initiated navigation, in most cases these navigations will be
  // auto triggered on restoration. So there is no need to save them.
  if (navigation_handle->IsRendererInitiated() &&
      !navigation_handle->HasUserGesture()) {
    return false;
  }

  return SavedTabGroupUtils::IsURLValidForSavedTabGroups(
      navigation_handle->GetURL());
}

// Returns whether this navigation is user triggered main frame navigation.
bool IsUserTriggeredMainFrameNavigation(
    content::NavigationHandle* navigation_handle) {
  // If this is not a primary frame, it shouldn't impact the state of the
  // tab.
  if (!navigation_handle->IsInPrimaryMainFrame()) {
    return false;
  }

  // For renderer initiated navigation, we shouldn't change the existing
  // tab state.
  if (navigation_handle->IsRendererInitiated()) {
    return false;
  }

  // For forward/backward or reload navigations, don't clear tab state if they
  // are be triggered by scripts.
  if (!navigation_handle->HasUserGesture()) {
    if (navigation_handle->GetPageTransition() &
        ui::PAGE_TRANSITION_FORWARD_BACK) {
      return false;
    }

    if (navigation_handle->GetPageTransition() & ui::PAGE_TRANSITION_RELOAD) {
      return false;
    }
  }

  return true;
}

// Returns whether URL is in a redirect chain.
bool IsURLInRedirectChain(const GURL& url,
                          const std::vector<GURL>& redirect_chain) {
  for (const auto& redirect_url : redirect_chain) {
    if (redirect_url.GetWithoutRef().spec() == url.GetWithoutRef().spec()) {
      return true;
    }
  }
  return false;
}

}  // namespace

SavedTabGroupWebContentsListener::SavedTabGroupWebContentsListener(
    content::WebContents* web_contents,
    base::Token token,
    SavedTabGroupKeyedService* service)
    : token_(token),
      web_contents_(web_contents),
      favicon_driver_(
          favicon::ContentFaviconDriver::FromWebContents(web_contents)),
      service_(service) {
  Observe(web_contents_);
  if (favicon_driver_) {
    favicon_driver_->AddObserver(this);
  }
}

SavedTabGroupWebContentsListener::SavedTabGroupWebContentsListener(
    content::WebContents* web_contents,
    content::NavigationHandle* navigation_handle,
    base::Token token,
    SavedTabGroupKeyedService* service)
    : token_(token),
      web_contents_(web_contents),
      favicon_driver_(
          favicon::ContentFaviconDriver::FromWebContents(web_contents)),
      service_(service),
      handle_from_sync_update_(navigation_handle) {
  Observe(web_contents_);
  if (favicon_driver_) {
    favicon_driver_->AddObserver(this);
  }
}

SavedTabGroupWebContentsListener::~SavedTabGroupWebContentsListener() {
  if (favicon_driver_) {
    favicon_driver_->RemoveObserver(this);
  }
  TabGroupSyncTabState::Reset(web_contents());
}

void SavedTabGroupWebContentsListener::NavigateToUrl(const GURL& url) {
  if (!url.is_valid()) {
    return;
  }

  // If the URL is inside current tab URL's redirect chain, there is no need to
  // navigate as the navigation will end up with the current tab URL.
  if (IsURLInRedirectChain(url, tab_redirect_chain_)) {
    return;
  }

  // Dont navigate to the new URL if its not valid for sync.
  if (!SavedTabGroupUtils::IsURLValidForSavedTabGroups(url)) {
    return;
  }

  content::NavigationHandle* navigation_handle =
      web_contents()
          ->GetController()
          .LoadURLWithParams(content::NavigationController::LoadURLParams(url))
          .get();
  handle_from_sync_update_ = navigation_handle;
}

void SavedTabGroupWebContentsListener::DidFinishNavigation(
    content::NavigationHandle* navigation_handle) {
  UpdateTabRedirectChain(navigation_handle);

  // If the navigation was the result of a sync update we don't want to update
  // the SavedTabGroupModel.
  if (navigation_handle == handle_from_sync_update_) {
    handle_from_sync_update_ = nullptr;
    // Create a tab state to indicate that the tab is restricted.
    TabGroupSyncTabState::Create(web_contents());
    return;
  }

  if (IsUserTriggeredMainFrameNavigation(navigation_handle)) {
    // Once the tab state is remove, restrictions will be removed from it.
    TabGroupSyncTabState::Reset(web_contents());
  }

  if (!IsSaveableNavigation(navigation_handle)) {
    return;
  }

  SavedTabGroup* group = service_->model()->GetGroupContainingTab(token_);
  CHECK(group);

  service_->UpdateAttributions(group->local_group_id().value(), token_);

  SavedTabGroupTab* tab = group->GetTab(token_);
  tab->SetTitle(web_contents_->GetTitle());
  tab->SetURL(web_contents_->GetURL());
  tab->SetFavicon(favicon::TabFaviconFromWebContents(web_contents_));
  service_->model()->UpdateTabInGroup(group->saved_guid(), *tab);
  service_->OnTabNavigatedLocally(group->saved_guid(), tab->saved_tab_guid());
}

void SavedTabGroupWebContentsListener::DidGetUserInteraction(
    const blink::WebInputEvent& event) {
  TabGroupSyncTabState::Reset(web_contents());
}

void SavedTabGroupWebContentsListener::TitleWasSet(
    content::NavigationEntry* entry) {
  SavedTabGroup* group = service_->model()->GetGroupContainingTab(token_);
  CHECK(group);

  // Don't update the title if the URL should not be synced.
  if (!SavedTabGroupUtils::IsURLValidForSavedTabGroups(entry->GetURL())) {
    return;
  }

  SavedTabGroupTab* tab = group->GetTab(token_);
  tab->SetTitle(entry->GetTitleForDisplay());
  service_->model()->UpdateTabInGroup(group->saved_guid(), *tab);
}

void SavedTabGroupWebContentsListener::OnFaviconUpdated(
    favicon::FaviconDriver* favicon_driver,
    FaviconDriverObserver::NotificationIconType notification_icon_type,
    const GURL& icon_url,
    bool icon_url_changed,
    const gfx::Image& image) {
  SavedTabGroup* group = service_->model()->GetGroupContainingTab(token_);
  CHECK(group);

  // Don't update the favicon if the URL should not be synced.
  if (!SavedTabGroupUtils::IsURLValidForSavedTabGroups(
          favicon_driver->GetActiveURL())) {
    return;
  }

  SavedTabGroupTab* tab = group->GetTab(token_);
  tab->SetFavicon(image);
  service_->model()->UpdateTabInGroup(group->saved_guid(), *tab);
}

void SavedTabGroupWebContentsListener::UpdateTabRedirectChain(
    content::NavigationHandle* navigation_handle) {
  if (!ui::PageTransitionIsMainFrame(navigation_handle->GetPageTransition())) {
    return;
  }

  tab_redirect_chain_.clear();
  for (const auto& url : navigation_handle->GetRedirectChain()) {
    tab_redirect_chain_.emplace_back(url);
  }
}

}  // namespace tab_groups
