// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/tabs/saved_tab_groups/saved_tab_group_web_contents_listener.h"

#include "chrome/browser/favicon/favicon_utils.h"
#include "chrome/browser/tab_group_sync/tab_group_sync_tab_state.h"
#include "chrome/browser/ui/tabs/saved_tab_groups/saved_tab_group_keyed_service.h"
#include "chrome/browser/ui/tabs/saved_tab_groups/saved_tab_group_utils.h"
#include "chrome/browser/ui/tabs/saved_tab_groups/tab_group_service_wrapper.h"
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
    TabGroupServiceWrapper* wrapper_service)
    : token_(token),
      web_contents_(web_contents),
      wrapper_service_(wrapper_service) {
  Observe(web_contents_);
}

SavedTabGroupWebContentsListener::SavedTabGroupWebContentsListener(
    content::WebContents* web_contents,
    content::NavigationHandle* navigation_handle,
    base::Token token,
    TabGroupServiceWrapper* wrapper_service)
    : token_(token),
      web_contents_(web_contents),
      wrapper_service_(wrapper_service),
      handle_from_sync_update_(navigation_handle) {
  Observe(web_contents_);
}

SavedTabGroupWebContentsListener::~SavedTabGroupWebContentsListener() {
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

  std::optional<SavedTabGroup> group = saved_group();
  SavedTabGroupTab* tab = group->GetTab(token_);
  CHECK(tab);

  wrapper_service_->SetFaviconForTab(
      group->local_group_id().value(), token_,
      favicon::TabFaviconFromWebContents(web_contents_));
  wrapper_service_->UpdateTab(group->local_group_id().value(), token_,
                              web_contents_->GetTitle(),
                              web_contents_->GetURL(), /*position=*/
                              std::nullopt);
}

void SavedTabGroupWebContentsListener::DidGetUserInteraction(
    const blink::WebInputEvent& event) {
  TabGroupSyncTabState::Reset(web_contents());
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

const std::optional<SavedTabGroup>
SavedTabGroupWebContentsListener::saved_group() {
  std::vector<SavedTabGroup> all_groups = wrapper_service_->GetAllGroups();
  auto iter = base::ranges::find_if(
      all_groups, [&](const SavedTabGroup& potential_group) {
        return potential_group.ContainsTab(token_);
      });
  CHECK(iter != all_groups.end());

  return *iter;
}

}  // namespace tab_groups
