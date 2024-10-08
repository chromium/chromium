// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/tabs/saved_tab_groups/saved_tab_group_web_contents_listener.h"

#include "base/functional/bind.h"
#include "chrome/browser/favicon/favicon_utils.h"
#include "chrome/browser/tab_group_sync/tab_group_sync_tab_state.h"
#include "chrome/browser/tab_group_sync/tab_group_sync_utils.h"
#include "chrome/browser/ui/tabs/saved_tab_groups/saved_tab_group_keyed_service.h"
#include "chrome/browser/ui/tabs/saved_tab_groups/saved_tab_group_utils.h"
#include "chrome/browser/ui/tabs/saved_tab_groups/tab_group_sync_service_proxy.h"
#include "chrome/browser/ui/tabs/tab_model.h"
#include "components/favicon/content/content_favicon_driver.h"
#include "components/saved_tab_groups/internal/saved_tab_group_model.h"
#include "components/saved_tab_groups/public/features.h"
#include "components/saved_tab_groups/public/saved_tab_group.h"
#include "components/saved_tab_groups/public/saved_tab_group_tab.h"
#include "components/saved_tab_groups/public/utils.h"
#include "content/public/browser/favicon_status.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/web_contents.h"
#include "ui/base/page_transition_types.h"

namespace tab_groups {
namespace {

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

}  // namespace

void SavedTabGroupWebContentsListener::OnTabDiscarded(
    tabs::TabInterface* tab_interface,
    content::WebContents* old_content,
    content::WebContents* new_content) {
  Observe(new_content);
}

SavedTabGroupWebContentsListener::SavedTabGroupWebContentsListener(
    TabGroupSyncService* service,
    const LocalTabID& saved_tab_group_tab_id,
    tabs::TabModel* local_tab,
    content::NavigationHandle* navigation_handle)
    : service_(service),
      saved_tab_group_tab_id_(saved_tab_group_tab_id),
      local_tab_(local_tab),
      handle_from_sync_update_(navigation_handle) {
  tab_discard_subscription_ = local_tab->RegisterWillDiscardContents(
      base::BindRepeating(&SavedTabGroupWebContentsListener::OnTabDiscarded,
                          base::Unretained(this)));
  Observe(local_tab->contents());
}

SavedTabGroupWebContentsListener::~SavedTabGroupWebContentsListener() {
  TabGroupSyncTabState::Reset(contents());
}

void SavedTabGroupWebContentsListener::NavigateToUrl(const GURL& url) {
  if (!url.is_valid()) {
    return;
  }

  std::optional<SavedTabGroup> group = saved_group();
  SavedTabGroupTab* saved_tab = group->GetTab(saved_tab_group_tab_id_);
  CHECK(saved_tab);

  // If the URL is inside current tab URL's redirect chain, there is no need to
  // navigate as the navigation will end up with the current tab URL.
  if (saved_tab->IsURLInRedirectChain(url)) {
    return;
  }

  // Dont navigate to the new URL if its not valid for sync.
  if (!IsURLValidForSavedTabGroups(url)) {
    return;
  }

  content::NavigationHandle* navigation_handle =
      contents()
          ->GetController()
          .LoadURLWithParams(content::NavigationController::LoadURLParams(url))
          .get();
  handle_from_sync_update_ = navigation_handle;
}

content::WebContents* SavedTabGroupWebContentsListener::contents() const {
  return local_tab_->contents();
}

void SavedTabGroupWebContentsListener::DidFinishNavigation(
    content::NavigationHandle* navigation_handle) {
  UpdateTabRedirectChain(navigation_handle);
  std::optional<SavedTabGroup> group = saved_group();
  if (group) {
    TabGroupSyncUtils::RecordSavedTabGroupNavigationUkmMetrics(
        saved_tab_group_tab_id_,
        group->collaboration_id() ? SavedTabGroupType::SHARED
                                  : SavedTabGroupType::SYNCED,
        navigation_handle, service_);
  }

  // If the navigation was the result of a sync update we don't want to update
  // the SavedTabGroupModel.
  if (navigation_handle == handle_from_sync_update_) {
    handle_from_sync_update_ = nullptr;
    // Create a tab state to indicate that the tab is restricted.
    TabGroupSyncTabState::Create(contents());
    return;
  }

  if (IsUserTriggeredMainFrameNavigation(navigation_handle)) {
    // Once the tab state is remove, restrictions will be removed from it.
    TabGroupSyncTabState::Reset(contents());
  }

  if (!TabGroupSyncUtils::IsSaveableNavigation(navigation_handle)) {
    return;
  }

  SavedTabGroupTab* tab = group->GetTab(saved_tab_group_tab_id_);
  CHECK(tab);

  if (!tab_groups::IsTabGroupSyncServiceDesktopMigrationEnabled()) {
    // TODO(crbug.com/359715038): Implement in TGSS then remove cast.
    static_cast<TabGroupSyncServiceProxy*>(service_)->SetFaviconForTab(
        group->local_group_id().value(), saved_tab_group_tab_id_,
        favicon::TabFaviconFromWebContents(contents()));
  }

  SavedTabGroupTabBuilder tab_builder;
  tab_builder.SetURL(contents()->GetURL());
  tab_builder.SetTitle(contents()->GetTitle());

  service_->UpdateTab(group->local_group_id().value(), saved_tab_group_tab_id_,
                      std::move(tab_builder));
}

void SavedTabGroupWebContentsListener::DidGetUserInteraction(
    const blink::WebInputEvent& event) {
  TabGroupSyncTabState::Reset(contents());
}

void SavedTabGroupWebContentsListener::UpdateTabRedirectChain(
    content::NavigationHandle* navigation_handle) {
  if (!ui::PageTransitionIsMainFrame(navigation_handle->GetPageTransition())) {
    return;
  }

  std::optional<SavedTabGroup> group = saved_group();
  SavedTabGroupTabBuilder tab_builder;
  tab_builder.SetRedirectURLChain(navigation_handle->GetRedirectChain());
  service_->UpdateTab(group->local_group_id().value(), saved_tab_group_tab_id_,
                      tab_builder);
}

const SavedTabGroup SavedTabGroupWebContentsListener::saved_group() {
  std::vector<SavedTabGroup> all_groups = service_->GetAllGroups();
  auto iter = base::ranges::find_if(
      all_groups, [&](const SavedTabGroup& potential_group) {
        return potential_group.ContainsTab(saved_tab_group_tab_id_);
      });
  CHECK(iter != all_groups.end());

  return *iter;
}

}  // namespace tab_groups
