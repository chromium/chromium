// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/tabs/saved_tab_groups/saved_tab_group_web_contents_listener.h"

#include "base/functional/bind.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/favicon/favicon_service_factory.h"
#include "chrome/browser/favicon/favicon_utils.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/renderer_host/chrome_navigation_ui_data.h"
#include "chrome/browser/resource_coordinator/tab_manager.h"
#include "chrome/browser/tab_group_sync/tab_group_sync_tab_state.h"
#include "chrome/browser/tab_group_sync/tab_group_sync_utils.h"
#include "chrome/browser/ui/browser_window/public/browser_window_features.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/tabs/saved_tab_groups/most_recent_shared_tab_update_store.h"
#include "chrome/browser/ui/tabs/saved_tab_groups/saved_tab_group_utils.h"
#include "chrome/browser/ui/tabs/tab_change_type.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "components/data_sharing/public/features.h"
#include "components/favicon/core/favicon_service.h"
#include "components/favicon_base/favicon_callback.h"
#include "components/favicon_base/favicon_types.h"
#include "components/saved_tab_groups/internal/saved_tab_group_model.h"
#include "components/saved_tab_groups/public/features.h"
#include "components/saved_tab_groups/public/saved_tab_group.h"
#include "components/saved_tab_groups/public/saved_tab_group_tab.h"
#include "components/saved_tab_groups/public/utils.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/web_contents.h"
#include "ui/base/models/image_model.h"
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

bool IsMainFrameRendererNavigation(
    content::NavigationHandle* navigation_handle) {
  return navigation_handle->IsInPrimaryMainFrame() &&
         navigation_handle->IsRendererInitiated();
}

bool WasNavigationInitiatedFromSync(
    content::NavigationHandle* navigation_handle) {
  if (!navigation_handle) {
    return false;
  }
  ChromeNavigationUIData* ui_data = static_cast<ChromeNavigationUIData*>(
      navigation_handle->GetNavigationUIData());
  return ui_data && ui_data->navigation_initiated_from_sync();
}

}  // namespace

DeferredTabState::DeferredTabState(tabs::TabInterface* local_tab,
                                   const GURL& url,
                                   const std::u16string& title,
                                   favicon::FaviconService* favicon_service)
    : local_tab_(local_tab), url_(url), title_(title) {
  if (favicon_service) {
    favicon_tracker_ = std::make_unique<base::CancelableTaskTracker>();
    favicon_service->GetFaviconImageForPageURL(
        url_,
        base::BindOnce(&DeferredTabState::OnGetFaviconImageResult,
                       base::Unretained(this)),
        favicon_tracker_.get());
  }
}
DeferredTabState::~DeferredTabState() = default;

void DeferredTabState::OnGetFaviconImageResult(
    const favicon_base::FaviconImageResult& result) {
  if (result.image.IsEmpty()) {
    return;
  }

  if (!local_tab_) {
    return;
  }

  BrowserWindowInterface* browser_window =
      local_tab_->GetBrowserWindowInterface();
  if (!browser_window) {
    return;
  }

  favicon_ = ui::ImageModel::FromImage(result.image);
  browser_window->GetTabStripModel()->NotifyTabChanged(local_tab_,
                                                       TabChangeType::kAll);
}

void SavedTabGroupWebContentsListener::OnTabDiscarded(
    tabs::TabInterface* tab_interface,
    content::WebContents* old_content,
    content::WebContents* new_content) {
  Observe(new_content);

  tab_foregrounded_subscription_ =
      tab_interface->RegisterDidActivate(base::BindRepeating(
          &SavedTabGroupWebContentsListener::OnTabEnteredForeground,
          base::Unretained(this)));
}

SavedTabGroupWebContentsListener::SavedTabGroupWebContentsListener(
    TabGroupSyncService* service,
    tabs::TabInterface* local_tab)
    : service_(service), local_tab_(local_tab) {
  tab_discard_subscription_ = local_tab->RegisterWillDiscardContents(
      base::BindRepeating(&SavedTabGroupWebContentsListener::OnTabDiscarded,
                          base::Unretained(this)));
  Observe(local_tab->GetContents());

  tab_foregrounded_subscription_ =
      local_tab->RegisterDidActivate(base::BindRepeating(
          &SavedTabGroupWebContentsListener::OnTabEnteredForeground,
          base::Unretained(this)));
}

SavedTabGroupWebContentsListener::~SavedTabGroupWebContentsListener() {
  TabGroupSyncTabState::Reset(contents());
}

void SavedTabGroupWebContentsListener::NavigateToUrl(
    base::PassKey<LocalTabGroupListener>,
    const GURL& url) {
  NavigateToUrlInternal(url);
}

void SavedTabGroupWebContentsListener::NavigateToUrlForTest(const GURL& url) {
  NavigateToUrlInternal(url);
}

void SavedTabGroupWebContentsListener::NavigateToUrlInternal(const GURL& url) {
  if (!url.is_valid()) {
    return;
  }

  std::optional<SavedTabGroup> group = saved_group();
  CHECK(group);
  SavedTabGroupTab* saved_tab = group->GetTab(local_tab_id());
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

  // If deferring remote navigations is enabled (sharing) and the tab is in the
  // background, then dont actually perform the navigation, instead cache the
  // URL for performing the navigation later.
  if (!data_sharing::features::IsDataSharingFunctionalityEnabled() ||
      local_tab_->IsActivated()) {
    PerformNavigation(url);
  } else {
    favicon::FaviconService* favicon_service =
        FaviconServiceFactory::GetForProfile(
            local_tab_->GetBrowserWindowInterface()->GetProfile(),
            ServiceAccessType::EXPLICIT_ACCESS);

    g_browser_process->GetTabManager()->DiscardTabByExtension(
        local_tab_->GetContents());
    deferred_tab_state_.emplace(local_tab_, url, saved_tab->title(),
                                favicon_service);
  }
}

void SavedTabGroupWebContentsListener::PerformNavigation(const GURL& url) {
  // Start loading the URL. Mark the navigation as sync initiated to avoid ping
  // pong issues.
  content::NavigationController::LoadURLParams params(url);
  auto navigation_ui_data = std::make_unique<ChromeNavigationUIData>();
  navigation_ui_data->set_navigation_initiated_from_sync(true);
  params.navigation_ui_data = std::move(navigation_ui_data);

  contents()->GetController().LoadURLWithParams(params).get();
}

LocalTabID SavedTabGroupWebContentsListener::local_tab_id() const {
  return local_tab_->GetHandle().raw_value();
}

content::WebContents* SavedTabGroupWebContentsListener::contents() const {
  return local_tab_->GetContents();
}

void SavedTabGroupWebContentsListener::DidFinishNavigation(
    content::NavigationHandle* navigation_handle) {
  // Skip navigations that are not in tab groups.
  if (!local_tab_->GetGroup()) {
    return;
  }

  std::optional<SavedTabGroup> group = saved_group();
  if (!group) {
    // This could be a tab in a group where auto-save isn't enabled.
    return;
  }

  TabGroupSyncUtils::RecordSavedTabGroupNavigationUkmMetrics(
      local_tab_id(),
      group->collaboration_id() ? SavedTabGroupType::SHARED
                                : SavedTabGroupType::SYNCED,
      navigation_handle, service_);

  // If the navigation was the result of a sync update we don't want to update
  // the SavedTabGroupModel.
  if (WasNavigationInitiatedFromSync(navigation_handle)) {
    // Update the redirect chain if the navigation is from sync, so that the
    // sync update of the same URL later will be ignored.
    UpdateTabRedirectChain(navigation_handle);
    // Create a tab state to indicate that the tab is restricted.
    TabGroupSyncTabState::Create(contents());
    return;
  }

  const bool is_user_triggered =
      IsUserTriggeredMainFrameNavigation(navigation_handle);
  if (is_user_triggered) {
    // Once the tab state is remove, restrictions will be removed from it.
    TabGroupSyncTabState::Reset(contents());
  }

  if (!TabGroupSyncUtils::IsSaveableNavigation(navigation_handle)) {
    return;
  }

  // Only update the redirect chain for navigations that are sent to sync so
  // that the redirect chain will match the entry stored in sync db. This will
  // prevent the case that if a renderer initiated navigation changes the tab
  // URL, a sync update will not reload the page.
  UpdateTabRedirectChain(navigation_handle);

  SavedTabGroupTab* tab = group->GetTab(local_tab_id());
  CHECK(tab);

  service_->NavigateTab(group->local_group_id().value(), local_tab_id(),
                        contents()->GetURL(), contents()->GetTitle());

  if (is_user_triggered || IsMainFrameRendererNavigation(navigation_handle)) {
    // We additionally want to record the last share update if it is due
    // to a renderer navigation in the main frame.
    // Note: this does not overlap with the conditions checked in
    // IsUserTriggeredMainFrameNavigation.
    if (MostRecentSharedTabUpdateStore* most_recent_shared_tab_update_store =
            local_tab_->GetBrowserWindowInterface()
                ->GetFeatures()
                .most_recent_shared_tab_update_store()) {
      most_recent_shared_tab_update_store->SetLastUpdatedTab(
          group->local_group_id().value(), local_tab_id());
    }
  }
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
  CHECK(group);

  SavedTabGroupTabBuilder tab_builder;
  tab_builder.SetRedirectURLChain(navigation_handle->GetRedirectChain());
  service_->UpdateTabProperties(group->local_group_id().value(), local_tab_id(),
                                tab_builder);
}

std::optional<SavedTabGroup> SavedTabGroupWebContentsListener::saved_group() {
  std::optional<tab_groups::TabGroupId> local_group_id = local_tab_->GetGroup();
  return local_group_id ? service_->GetGroup(*local_group_id) : std::nullopt;
}

void SavedTabGroupWebContentsListener::OnTabEnteredForeground(
    tabs::TabInterface* tab_interface) {
  if (deferred_tab_state_.has_value()) {
    PerformNavigation(deferred_tab_state_.value().url());
    deferred_tab_state_.reset();
  }
}

}  // namespace tab_groups
