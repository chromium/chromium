// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/tabs/saved_tab_groups/local_tab_group_listener.h"

#include <unordered_map>

#include "base/feature_list.h"
#include "base/token.h"
#include "chrome/browser/tab_group_sync/tab_group_sync_utils.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/public/tab_interface.h"
#include "chrome/browser/ui/tabs/saved_tab_groups/saved_tab_group_utils.h"
#include "chrome/browser/ui/tabs/saved_tab_groups/saved_tab_group_web_contents_listener.h"
#include "chrome/browser/ui/tabs/saved_tab_groups/tab_group_sync_service_proxy.h"
#include "chrome/browser/ui/tabs/tab_group.h"
#include "chrome/browser/ui/tabs/tab_group_model.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "components/saved_tab_groups/public/features.h"
#include "components/saved_tab_groups/public/saved_tab_group_tab.h"
#include "components/saved_tab_groups/public/utils.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/web_contents.h"

namespace tab_groups {

LocalTabGroupListener::LocalTabGroupListener(
    const tab_groups::TabGroupId local_id,
    const base::Uuid saved_guid,
    TabGroupSyncService* service,
    std::map<tabs::TabInterface*, base::Uuid>& tab_guid_mapping)
    : service_(service), local_id_(local_id), saved_guid_(saved_guid) {
  for (const auto& [local_tab, saved_tab_guid] : tab_guid_mapping) {
    const LocalTabID local_tab_id = local_tab->GetTabHandle();
    tab_listener_mapping_.try_emplace(local_tab, service_, local_tab);

    std::optional<SavedTabGroup> group = service_->GetGroup(saved_guid);
    const SavedTabGroupTab tab(*group->GetTab(saved_tab_guid));
    service_->UpdateLocalTabId(local_id, tab.saved_tab_guid(), local_tab_id);
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

    const auto map_entry = tab_listener_mapping_.find(local_tab);
    CHECK(map_entry != tab_listener_mapping_.end());

    const SavedTabGroupWebContentsListener& listener = map_entry->second;

    const auto is_local_tab = [&listener](const SavedTabGroupTab& saved_tab) {
      return saved_tab.local_tab_id() == listener.local_tab_id();
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

  const std::optional<int> tabstrip_index_of_first_tab_in_group =
      tab_strip_model->group_model()->GetTabGroup(local_id_)->GetFirstTab();
  CHECK(tabstrip_index_of_first_tab_in_group.has_value());

  const int relative_index_of_tab_in_group =
      tab_strip_model->GetIndexOfTab(local_tab) -
      tabstrip_index_of_first_tab_in_group.value();

  LocalTabID local_tab_id = local_tab->GetTabHandle();

  // Create a new SavedTabGroupTab linked to `local_tab_id`.
  SavedTabGroupTab tab =
      SavedTabGroupUtils::CreateSavedTabGroupTabFromWebContents(
          local_tab->GetContents(), saved_guid_);
  if (!IsURLValidForSavedTabGroups(tab.url())) {
    tab.SetURL(GURL(chrome::kChromeUINewTabURL));
  }

  service_->AddTab(local_id_, local_tab_id, tab.title(), tab.url(),
                   relative_index_of_tab_in_group);

  // Link `web_contents` to `local_tab_id`.
  tab_listener_mapping_.try_emplace(local_tab, service_, local_tab);
}

void LocalTabGroupListener::MoveWebContentsFromLocal(
    TabStripModel* tab_strip_model,
    content::WebContents* web_contents,
    int tabstrip_index_of_moved_tab) {
  if (paused_) {
    return;
  }

  tabs::TabInterface* local_tab =
      tab_strip_model->GetTabForWebContents(web_contents);

  // It is possible that the listener does not track the webcontents. The tab
  // should get added correctly in `service_->model()` only after being tracked
  // by the listener. See (b/343519257)
  if (!tab_listener_mapping_.contains(local_tab)) {
    return;
  }

  // Ex:        0 1   2 3 4
  //  TabStrip: A B [ C D E ]
  //  TabGroup:       0 1 2
  // C represents the first tab in the group. For the tabstrip this means C is
  // at index 2. For the tab group, C is at index 0.
  // Moving C to index 4 in the tabstrip means it will now have an index of 2 in
  // the tab group and SavedTabGroupModel.
  const std::optional<int> tabstrip_index_of_first_tab_in_group =
      tab_strip_model->group_model()->GetTabGroup(local_id_)->GetFirstTab();
  CHECK(tabstrip_index_of_first_tab_in_group.has_value());

  // Count the number of tabs that are actually in the group between
  // `tabstrip_index_of_first_tab_in_group` and `tabstrip_index_of_moved_tab`.
  // We must do this because a tab group may not be contiguous in intermediate
  // states such as when dragging a group by its header.
  int index_in_group = 0;
  for (int i = tabstrip_index_of_first_tab_in_group.value();
       i < tabstrip_index_of_moved_tab; i++) {
    if (tab_strip_model->GetTabGroupForTab(i) == local_id_) {
      index_in_group++;
    }
  }
  CHECK_GE(index_in_group, 0);

  const SavedTabGroupWebContentsListener& web_contents_listener =
      tab_listener_mapping_.at(local_tab);

  service_->MoveTab(local_id_, web_contents_listener.local_tab_id(),
                    index_in_group);
}

LocalTabGroupListener::Liveness
LocalTabGroupListener::MaybeRemoveWebContentsFromLocal(
    content::WebContents* web_contents) {
  if (paused_) {
    return Liveness::kGroupExists;
  }

  // Find the tab listener entry from the map corresponding to the
  // `web_contents`. Remove it from the map. If it's the last tab, the group
  // listener itself should be deleted.
  const auto tab_guid_pair_iter =
      std::find_if(tab_listener_mapping_.begin(), tab_listener_mapping_.end(),
                   [web_contents](auto& pair) {
                     return pair.first->GetContents() == web_contents;
                   });

  if (tab_guid_pair_iter == tab_listener_mapping_.end()) {
    // The tab doesn't belong to the group, just return. This is natural
    // since this method is invoked for every LocalTabGroupListener in the
    // service.
    return Liveness::kGroupExists;
  }

  const LocalTabID local_tab_id = tab_guid_pair_iter->second.local_tab_id();
  const std::optional<SavedTabGroup> saved_group =
      service_->GetGroup(saved_guid_);
  const base::Uuid tab_guid =
      saved_group->GetTab(local_tab_id)->saved_tab_guid();
  CHECK(saved_group->local_group_id().has_value());

  tab_listener_mapping_.erase(tab_guid_pair_iter->first);

  // This object is deleted by the time we have reached here. This means
  // saved_guid_ gives us a garbage value and cannot be used anymore to query.
  // Since we are removing 1 tab, we can check if the group would have been
  // deleted if the last tab was removed.
  // TODO(crbug.com/352802808): Use a PostTask to prevent re-entrancy when the
  // group is removed.
  const bool was_last_tab_in_group = saved_group->saved_tabs().size() == 1;
  service_->RemoveTab(local_id_, local_tab_id);
  return was_last_tab_in_group ? Liveness::kGroupDeleted
                               : Liveness::kGroupExists;
}

void LocalTabGroupListener::GroupRemovedFromSync() {
  PauseTracking();

  // Remove every currently tracked tab; this will also close the local group.
  std::vector<tabs::TabInterface*> tabs;
  for (auto& [tab, listener] : tab_listener_mapping_) {
    tabs.push_back(tab);
  }
  for (tabs::TabInterface* const tab : tabs) {
    RemoveTabFromSync(tab,
                      /*should_close_tab=*/base::FeatureList::IsEnabled(
                          tab_groups::kTabGroupsSaveV2));
  }

  ResumeTracking();
}

LocalTabGroupListener::Liveness LocalTabGroupListener::UpdateFromSync() {
  PauseTracking();

  RemoveLocalWebContentsNotInSavedGroup();

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
  local_tab_group->SetVisualData(
      tab_groups::TabGroupVisualData(saved_group->title(), saved_group->color(),
                                     is_collapsed),
      /*is_customized=*/true);

  std::unordered_map<LocalTabID, tabs::TabInterface*> saved_id_tab_mapping;
  for (auto& [tabs, listener] : tab_listener_mapping_) {
    saved_id_tab_mapping[listener.local_tab_id()] = tabs;
  }

  // Add, navigate, and reorder local tabs to match saved tabs.
  const gfx::Range group_index_range = local_tab_group->ListTabs();
  CHECK_LE(group_index_range.length(), saved_group->saved_tabs().size());

  // Parallel iterate over saved tabs and local indices. For each saved tab and
  // index, ensure the corresponding local tab is at that index and in the
  // correct state.
  int next_index_in_tab_strip = group_index_range.start();
  for (const SavedTabGroupTab& saved_tab : saved_group->saved_tabs()) {
    tabs::TabInterface* tab =
        saved_tab.local_tab_id().has_value()
            ? saved_id_tab_mapping[saved_tab.local_tab_id().value()]
            : nullptr;
    MatchLocalTabToSavedTab(saved_tab, tab, tab_strip_model,
                            next_index_in_tab_strip);
    next_index_in_tab_strip++;
  }

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
      tab_listener_mapping_.at(local_tab).NavigateToUrl(saved_tab.url());
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
  LocalTabID local_tab_id = local_tab->GetTabHandle();
  service_->UpdateLocalTabId(local_id_, tab.saved_tab_guid(), local_tab_id);
  tab_listener_mapping_.try_emplace(local_tab, service_, local_tab);
}

void LocalTabGroupListener::RemoveLocalWebContentsNotInSavedGroup() {
  const std::optional<SavedTabGroup> saved_group =
      service_->GetGroup(saved_guid_);
  CHECK(saved_group.has_value());
  const std::vector<tabs::TabInterface*> tabs_in_local_group =
      SavedTabGroupUtils::GetTabsInGroup(local_id_);
  for (tabs::TabInterface* const local_tab : tabs_in_local_group) {
    const auto& it = tab_listener_mapping_.find(local_tab);
    CHECK(it != tab_listener_mapping_.end());
    if (!saved_group->ContainsTab(it->second.local_tab_id())) {
      RemoveTabFromSync(local_tab, /*should_close_tab=*/true);
    }
  }
}

void LocalTabGroupListener::RemoveTabFromSync(tabs::TabInterface* local_tab,
                                              bool should_close_tab) {
  tab_listener_mapping_.erase(local_tab);

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
