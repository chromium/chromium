// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/tabs/saved_tab_groups/local_tab_group_listener.h"

#include <unordered_map>

#include "base/feature_list.h"
#include "base/token.h"
#include "base/types/pass_key.h"
#include "chrome/browser/tab_group_sync/tab_group_sync_utils.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window/public/browser_window_features.h"
#include "chrome/browser/ui/tabs/public/tab_features.h"
#include "chrome/browser/ui/tabs/saved_tab_groups/most_recent_shared_tab_update_store.h"
#include "chrome/browser/ui/tabs/saved_tab_groups/saved_tab_group_utils.h"
#include "chrome/browser/ui/tabs/saved_tab_groups/saved_tab_group_web_contents_listener.h"
#include "chrome/browser/ui/tabs/tab_group_model.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/tabs/tab_strip_model_observer.h"
#include "chrome/common/webui_url_constants.h"
#include "components/saved_tab_groups/public/features.h"
#include "components/saved_tab_groups/public/saved_tab_group_tab.h"
#include "components/saved_tab_groups/public/tab_group_sync_service.h"
#include "components/saved_tab_groups/public/utils.h"
#include "components/tabs/public/tab_group.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/web_contents.h"

namespace tab_groups {

LocalTabGroupListener::LocalTabGroupListener(
    const tab_groups::TabGroupId local_id,
    const base::Uuid saved_guid,
    TabGroupSyncService* service,
    std::map<tabs::TabInterface*, base::Uuid>& tab_guid_mapping)
    : service_(service), local_id_(local_id), saved_guid_(saved_guid) {
  std::optional<SavedTabGroup> group = service_->GetGroup(saved_guid);
  CHECK(group);
  for (const auto& [local_tab, saved_tab_guid] : tab_guid_mapping) {
    const LocalTabID local_tab_id = local_tab->GetHandle().raw_value();
    service_->UpdateLocalTabId(local_id, saved_tab_guid, local_tab_id);
  }
}

LocalTabGroupListener::~LocalTabGroupListener() = default;

void LocalTabGroupListener::PauseTracking() {
  // We can't handle nested multi-step operations. Crash to avoid data loss.
  CHECK(!paused_);

  paused_ = true;
}

void LocalTabGroupListener::ResumeTracking() {
  paused_ = false;

  // Thoroughly check for consistency between the data structures we're linking.
  // The saved tabs and the local tabs must match up 1:1, but it's OK if they
  // are in a different order.
  const std::optional<SavedTabGroup> saved_group =
      service_->GetGroup(saved_guid_);
  const std::vector<SavedTabGroupTab>& saved_tabs =
      saved_group.has_value() ? saved_group->saved_tabs()
                              : std::vector<SavedTabGroupTab>();

  const std::vector<tabs::TabInterface*> local_tabs =
      SavedTabGroupUtils::GetTabsInGroup(local_id_);

  CHECK_EQ(saved_tabs.size(), local_tabs.size());
  for (size_t i = 0; i < saved_tabs.size(); ++i) {
    tabs::TabInterface* const local_tab = local_tabs[i];

    const auto is_local_tab = [local_tab](const SavedTabGroupTab& saved_tab) {
      return saved_tab.local_tab_id() == local_tab->GetHandle().raw_value();
    };
    CHECK(std::find_if(saved_tabs.begin(), saved_tabs.end(), is_local_tab) !=
          saved_tabs.end());
  }
}

bool LocalTabGroupListener::IsTrackingPaused() const {
  return paused_;
}

void LocalTabGroupListener::UpdateVisualDataFromLocal(
    const TabGroupChange::VisualsChange* visual_change) {
  if (paused_) {
    return;
  }

  if (*(visual_change->old_visuals) == *(visual_change->new_visuals)) {
    return;
  }

  service_->UpdateVisualData(local_id_, visual_change->new_visuals);
}

void LocalTabGroupListener::AddTabFromLocal(tabs::TabInterface* local_tab,
                                            TabStripModel* tab_strip_model,
                                            int index) {
  if (paused_) {
    return;
  }

  CHECK(service_->GetGroup(saved_guid_).has_value());
  CHECK(tab_strip_model->group_model()->ContainsTabGroup(local_id_));

  tabs::TabInterface* first_tab_in_group =
      tab_strip_model->group_model()->GetTabGroup(local_id_)->GetFirstTab();
  CHECK(first_tab_in_group);
  int tabstrip_index_of_first_tab_in_group =
      tab_strip_model->GetIndexOfTab(first_tab_in_group);
  CHECK_NE(tabstrip_index_of_first_tab_in_group, TabStripModel::kNoTab);

  const int relative_index_of_tab_in_group =
      tab_strip_model->GetIndexOfTab(local_tab) -
      tabstrip_index_of_first_tab_in_group;

  LocalTabID local_tab_id = local_tab->GetHandle().raw_value();

  // Create a new SavedTabGroupTab linked to `local_tab_id`.
  SavedTabGroupTab tab =
      SavedTabGroupUtils::CreateSavedTabGroupTabFromWebContents(
          local_tab->GetContents(), saved_guid_);
  // Non-empty URLs will be saved into the tab group, and will be converted
  // to an unsupported URL later when sending to sync.
  if (tab.url().is_empty()) {
    tab.SetURL(GURL(chrome::kChromeUINewTabURL));
  }

  service_->AddTab(local_id_, local_tab_id, tab.title(), tab.url(),
                   relative_index_of_tab_in_group);

  MostRecentSharedTabUpdateStore* most_recent_shared_tab_update_store =
      local_tab->GetBrowserWindowInterface()
          ->GetFeatures()
          .most_recent_shared_tab_update_store();
  if (most_recent_shared_tab_update_store) {
    most_recent_shared_tab_update_store->SetLastUpdatedTab(local_id_,
                                                           local_tab_id);
  }
}

void LocalTabGroupListener::MoveWebContentsFromLocal(
    TabStripModel* tab_strip_model,
    content::WebContents* web_contents,
    int tabstrip_index_of_moved_tab) {
  if (paused_) {
    return;
  }

  const LocalTabID local_tab_id =
      tab_strip_model->GetTabForWebContents(web_contents)
          ->GetHandle()
          .raw_value();
  auto saved_group = service_->GetGroup(local_id_);
  CHECK(saved_group);
  const auto* saved_tab = saved_group->GetTab(local_tab_id);
  if (!saved_tab) {
    // This is probably the case where the tab is moved into the group from
    // outside. Hence, it doesn't exist in the destination saved tab group yet.
    // Ignore this, since it will be handled separately as two events (removal
    // from the old group, addition to the new group). See (crbug.com/343519257)
    return;
  }

  // Ex:        0 1   2 3 4
  //  TabStrip: A B [ C D E ]
  //  TabGroup:       0 1 2
  // C represents the first tab in the group. For the tabstrip this means C is
  // at index 2. For the tab group, C is at index 0.
  // Moving C to index 4 in the tabstrip means it will now have an index of 2 in
  // the tab group and SavedTabGroupModel.
  tabs::TabInterface* first_tab_in_group =
      tab_strip_model->group_model()->GetTabGroup(local_id_)->GetFirstTab();
  CHECK(first_tab_in_group);
  int tabstrip_index_of_first_tab_in_group =
      tab_strip_model->GetIndexOfTab(first_tab_in_group);
  CHECK_NE(tabstrip_index_of_first_tab_in_group, TabStripModel::kNoTab);

  // Count the number of tabs that are actually in the group between
  // `tabstrip_index_of_first_tab_in_group` and `tabstrip_index_of_moved_tab`.
  // We must do this because a tab group may not be contiguous in intermediate
  // states such as when dragging a group by its header.
  int index_in_group = 0;
  for (int i = tabstrip_index_of_first_tab_in_group;
       i < tabstrip_index_of_moved_tab; i++) {
    if (tab_strip_model->GetTabGroupForTab(i) == local_id_) {
      index_in_group++;
    }
  }
  CHECK_GE(index_in_group, 0);

  service_->MoveTab(local_id_, local_tab_id, index_in_group);

  MostRecentSharedTabUpdateStore* most_recent_shared_tab_update_store =
      tab_strip_model->GetTabForWebContents(web_contents)
          ->GetBrowserWindowInterface()
          ->GetFeatures()
          .most_recent_shared_tab_update_store();
  if (most_recent_shared_tab_update_store) {
    most_recent_shared_tab_update_store->SetLastUpdatedTab(local_id_,
                                                           local_tab_id);
  }
}

LocalTabGroupListener::Liveness
LocalTabGroupListener::MaybeRemoveWebContentsFromLocal(
    content::WebContents* web_contents) {
  if (paused_) {
    return Liveness::kGroupExists;
  }

  tabs::TabInterface* local_tab =
      tabs::TabInterface::GetFromContents(web_contents);

  // Remove the tab from the service. If it's the last tab, the group
  // listener itself should be deleted.
  const LocalTabID local_tab_id = local_tab->GetHandle().raw_value();
  const std::optional<SavedTabGroup> saved_group =
      service_->GetGroup(saved_guid_);
  if (!saved_group) {
    // This can happen if the saved group was removed before the tab.
    return Liveness::kGroupDeleted;
  }

  const SavedTabGroupTab* saved_tab = saved_group->GetTab(local_tab_id);
  if (!saved_tab) {
    // The tab that was removed didn't belong to this group. This is natural
    // since this method is invoked for every LocalTabGroupListener in the
    // service. Return.
    return Liveness::kGroupExists;
  }

  CHECK(saved_group->local_group_id().has_value());

  // Get controller before tab is removed.
  MostRecentSharedTabUpdateStore* most_recent_shared_tab_update_store =
      local_tab->GetBrowserWindowInterface()
          ->GetFeatures()
          .most_recent_shared_tab_update_store();

  // This object is deleted by the time we have reached here. This means
  // saved_guid_ gives us a garbage value and cannot be used anymore to query.
  // Since we are removing 1 tab, we can check if the group would have been
  // deleted if the last tab was removed.
  // TODO(crbug.com/352802808): Use a PostTask to prevent re-entrancy when the
  // group is removed.
  const bool was_last_tab_in_group = saved_group->saved_tabs().size() == 1;
  service_->RemoveTab(local_id_, local_tab_id);

  if (most_recent_shared_tab_update_store) {
    most_recent_shared_tab_update_store->SetLastUpdatedTab(local_id_,
                                                           std::nullopt);
  }

  return was_last_tab_in_group ? Liveness::kGroupDeleted
                               : Liveness::kGroupExists;
}

LocalTabGroupListener::Liveness LocalTabGroupListener::UpdateFromSync() {
  PauseTracking();

  const std::optional<SavedTabGroup> saved_group =
      service_->GetGroup(saved_guid_);
  CHECK(saved_group.has_value());
  TabStripModel* const tab_strip_model =
      SavedTabGroupUtils::GetBrowserWithTabGroupId(local_id_)
          ->tab_strip_model();
  CHECK(tab_strip_model);

  // Update the group to use the saved title and color.
  TabGroup* local_tab_group =
      tab_strip_model->group_model()->GetTabGroup(local_id_);
  CHECK(local_tab_group);
  const bool is_collapsed = local_tab_group->visual_data()->is_collapsed();
  tab_strip_model->ChangeTabGroupVisuals(
      local_id_,
      tab_groups::TabGroupVisualData(saved_group->title(), saved_group->color(),
                                     is_collapsed),
      /*is_customized=*/true);

  // Add, navigate, and reorder local tabs to match saved tabs.
  const gfx::Range group_index_range = local_tab_group->ListTabs();

  // Parallel iterate over saved tabs and local indices. For each saved tab and
  // index, ensure the corresponding local tab is at that index and in the
  // correct state.
  int next_index_in_tab_strip = group_index_range.start();
  for (const SavedTabGroupTab& saved_tab : saved_group->saved_tabs()) {
    tabs::TabInterface* tab =
        saved_tab.local_tab_id().has_value()
            ? tabs::TabHandle(saved_tab.local_tab_id().value()).Get()
            : nullptr;
    MatchLocalTabToSavedTab(saved_tab, tab, tab_strip_model,
                            next_index_in_tab_strip);
    next_index_in_tab_strip++;
  }

  RemoveLocalWebContentsNotInSavedGroup();
  CHECK_EQ(local_tab_group->ListTabs().length(),
           saved_group->saved_tabs().size());

  ResumeTracking();

  return service_->GetGroup(saved_guid_).has_value() ? Liveness::kGroupExists
                                                     : Liveness::kGroupDeleted;
}

void LocalTabGroupListener::MatchLocalTabToSavedTab(
    SavedTabGroupTab saved_tab,
    tabs::TabInterface* local_tab,
    TabStripModel* tab_strip_model,
    int target_index_in_tab_strip) {
  if (saved_tab.local_tab_id().has_value()) {
    CHECK(local_tab);
    // Reorder if needed. This approach corresponds to selection sort.
    // N.B.: this approach will do N reorders for a tab that was moved N spots
    // to the left.
    const int current_index = tab_strip_model->GetIndexOfTab(local_tab);
    CHECK_EQ(local_id_,
             tab_strip_model->GetTabGroupForTab(current_index).value());
    tab_strip_model->MoveWebContentsAt(current_index, target_index_in_tab_strip,
                                       false);

    // Navigate if needed.
    if (saved_tab.url() != local_tab->GetContents()->GetURL()) {
      SavedTabGroupWebContentsListener* listener =
          local_tab->GetTabFeatures()->saved_tab_group_web_contents_listener();
      listener->NavigateToUrl(base::PassKey<LocalTabGroupListener>(),
                              saved_tab.url());
    }
  } else {
    OpenWebContentsFromSync(
        saved_tab, SavedTabGroupUtils::GetBrowserWithTabGroupId(local_id_),
        target_index_in_tab_strip);
  }
}

void LocalTabGroupListener::OpenWebContentsFromSync(SavedTabGroupTab tab,
                                                    Browser* browser,
                                                    int index_in_tabstrip) {
  GURL url_to_open = tab.url();
  if (!IsURLValidForSavedTabGroups(url_to_open)) {
    url_to_open = GURL(chrome::kChromeUINewTabURL);
  }

  content::NavigationHandle* navigation_handle =
      SavedTabGroupUtils::OpenTabInBrowser(
          url_to_open, browser, browser->profile(),
          WindowOpenDisposition::NEW_BACKGROUND_TAB, index_in_tabstrip,
          local_id_);
  content::WebContents* opened_contents =
      navigation_handle ? navigation_handle->GetWebContents() : nullptr;

  tabs::TabInterface* local_tab =
      browser->tab_strip_model()->GetTabForWebContents(opened_contents);

  // Listen to navigations.
  service_->UpdateLocalTabId(local_id_, tab.saved_tab_guid(),
                             local_tab->GetHandle().raw_value());
}

void LocalTabGroupListener::RemoveLocalWebContentsNotInSavedGroup() {
  const std::optional<SavedTabGroup> saved_group =
      service_->GetGroup(saved_guid_);
  CHECK(saved_group.has_value());
  const std::vector<tabs::TabInterface*> tabs_in_local_group =
      SavedTabGroupUtils::GetTabsInGroup(local_id_);
  for (tabs::TabInterface* const local_tab : tabs_in_local_group) {
    if (!saved_group->ContainsTab(local_tab->GetHandle().raw_value())) {
      RemoveTabFromSync(local_tab, /*should_close_tab=*/true);
    }
  }
}

void LocalTabGroupListener::RemoveTabFromSync(tabs::TabInterface* local_tab,
                                              bool should_close_tab) {
  Browser* const browser =
      SavedTabGroupUtils::GetBrowserWithTabGroupId(local_id_);
  CHECK(browser);
  CHECK(browser->tab_strip_model());
  int index = browser->tab_strip_model()->GetIndexOfTab(local_tab);
  CHECK(index != TabStripModel::kNoTab);

  // Unload listeners can delay or prevent a tab closing. Remove the tab from
  // the group first so the local and saved groups can be consistent even if
  // this happens.
  browser->tab_strip_model()->RemoveFromGroup({index});

  if (should_close_tab) {
    // Removing the tab from the group may have moved the tab to maintain group
    // contiguity. Find the tab again and close it.
    index = browser->tab_strip_model()->GetIndexOfTab(local_tab);
    browser->tab_strip_model()->CloseWebContentsAt(
        index, TabCloseTypes::CLOSE_CREATE_HISTORICAL_TAB);
  }
}

}  // namespace tab_groups
