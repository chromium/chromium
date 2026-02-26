// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/tabs/tab_list_bridge.h"

#include <cstddef>
#include <optional>

#include "base/check_op.h"
#include "base/notimplemented.h"
#include "chrome/browser/resource_coordinator/tab_lifecycle_unit_external.h"
#include "chrome/browser/tab_group_sync/tab_group_sync_service_factory.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface_iterator.h"
#include "chrome/browser/ui/tabs/tab_enums.h"
#include "chrome/browser/ui/tabs/tab_group_model.h"
#include "chrome/browser/ui/tabs/tab_model.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/tabs/tab_strip_model_delegate.h"
#include "components/saved_tab_groups/public/tab_group_sync_service.h"
#include "components/tab_groups/tab_group_visual_data.h"
#include "components/tabs/public/tab_group.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/browser/web_contents.h"

namespace {

// Returns the browser with the corresponding `target_session_id` if and only
// if the profile also matches `restrict_to_profile`.
BrowserWindowInterface* GetBrowserWithSessionId(
    const SessionID& target_session_id,
    const Profile* restrict_to_profile) {
  std::vector<BrowserWindowInterface*> all_browsers =
      GetAllBrowserWindowInterfaces();
  for (auto* browser : all_browsers) {
    if (browser->GetProfile() == restrict_to_profile &&
        browser->GetSessionID() == target_session_id) {
      return browser;
    }
  }

  return nullptr;
}

// Returns if the given `index` is in the middle of a tab group in the provided
// `tab_strip_model`.
bool IsInMiddleOfTabGroup(TabStripModel& tab_strip_model, int index) {
  std::optional<tab_groups::TabGroupId> target_group =
      tab_strip_model.GetTabGroupForTab(index);
  std::optional<tab_groups::TabGroupId> adjacent_group =
      tab_strip_model.GetTabGroupForTab(index - 1);
  return target_group.has_value() && target_group == adjacent_group;
}

// Returns the closest valid index to `initial_index` that is not in the middle
// of a tab group in the provided `tab_strip` based on `index_to_check`.
// Note that `index_to_check` may be different from `initial_index` to
// compensate for a tab group within `tab_strip` moving to the right.
int GetClosestValidIndexBetweenTabGroups(TabStripModel& tab_strip,
                                         int initial_index,
                                         int index_to_check) {
  size_t closest_valid_index = initial_index;

  if (IsInMiddleOfTabGroup(tab_strip, index_to_check)) {
    std::optional<tab_groups::TabGroupId> target_group_id =
        tab_strip.GetTabGroupForTab(index_to_check);
    CHECK(target_group_id.has_value());
    gfx::Range target_tabs =
        tab_strip.group_model()->GetTabGroup(*target_group_id)->ListTabs();
    int target_start = target_tabs.start();
    int target_end = target_tabs.end();

    // Check that `index_to_check` is fully within the tab group.
    CHECK_GT(index_to_check, target_start);
    CHECK_GT(target_end, index_to_check);

    // Check that `target_start` and `target_end` are not in the middle of any
    // group.
    DCHECK(!IsInMiddleOfTabGroup(tab_strip, target_start));
    DCHECK(!IsInMiddleOfTabGroup(tab_strip, target_end));

    // Find the offset between `index_to_check` and `target_start` and
    // `target_end` and apply the lesser of the offsets to
    // `closest_valid_index`. Note that `index_to_check` compensates for the
    // displacement of tabs and groups to the right of `group_id` whereas
    // `closest_valid_index`.
    int start_offset = index_to_check - target_start;
    int end_offset = target_end - index_to_check;
    if (end_offset < start_offset) {
      // Increment `closest_valid_index` so it points to the end of the tab
      // group.
      closest_valid_index += end_offset;
    } else {
      // Decrement `closest_valid_index` so it points to the start of the tab
      // group.
      closest_valid_index -= start_offset;
    }
  }

  return closest_valid_index;
}

}  // namespace

TabListBridge::TabListBridge(TabStripModel& tab_strip_model,
                             ui::UnownedUserDataHost& unowned_user_data_host)
    : tab_strip_(tab_strip_model),
      scoped_data_holder_(unowned_user_data_host, *this) {
  tab_strip_->AddObserver(this);
}

// Note: TabStripObserver already implements RemoveObserver() calls; no need to
// remove this object as an observer here.
TabListBridge::~TabListBridge() {
  for (auto& observer : observers_) {
    observer.OnTabListDestroyed(*this);
  }
}

void TabListBridge::AddTabListInterfaceObserver(
    TabListInterfaceObserver* observer) {
  observers_.AddObserver(observer);
}

void TabListBridge::RemoveTabListInterfaceObserver(
    TabListInterfaceObserver* observer) {
  observers_.RemoveObserver(observer);
}

int TabListBridge::GetTabCount() const {
  return tab_strip_->count();
}

int TabListBridge::GetActiveIndex() const {
  return tab_strip_->active_index();
}

tabs::TabInterface* TabListBridge::GetActiveTab() {
  return tab_strip_->GetActiveTab();
}

void TabListBridge::ActivateTab(tabs::TabHandle tab) {
  const int index = GetIndexOfTab(tab);
  CHECK_NE(index, TabStripModel::kNoTab);
  tab_strip_->ActivateTabAt(index);
}

tabs::TabInterface* TabListBridge::OpenTab(const GURL& url, int index) {
  // If `index` is specified as `TabStripModel::kNoTab`, then the tab is added
  // to the end of the tab strip.
  CHECK(index == TabStripModel::kNoTab || tab_strip_->ContainsIndex(index));

  // TODO(crbug.com/460650221): It's a bit of a code smell to reach in and grab
  // the delegate from TabStripModel, but it avoids introducing new dependencies
  // here.
  TabStripModelDelegate* delegate = tab_strip_->delegate();
  delegate->AddTabAt(url, index, /*foreground=*/true);
  int index_to_retrieve =
      index == TabStripModel::kNoTab ? tab_strip_->count() - 1 : index;
  return tab_strip_->GetTabAtIndex(index_to_retrieve);
}

void TabListBridge::SetOpenerForTab(tabs::TabHandle target,
                                    tabs::TabHandle opener) {
  const int target_index = GetIndexOfTab(target);
  CHECK_NE(target_index, TabStripModel::kNoTab);

  tab_strip_->SetOpenerOfTabAt(target_index, opener.Get());
}

tabs::TabInterface* TabListBridge::GetOpenerForTab(tabs::TabHandle target) {
  const int target_index = GetIndexOfTab(target);
  CHECK_NE(target_index, TabStripModel::kNoTab);
  return tab_strip_->GetOpenerOfTabAt(target_index);
}

tabs::TabInterface* TabListBridge::InsertWebContentsAt(
    int index,
    std::unique_ptr<content::WebContents> web_contents,
    bool should_pin,
    std::optional<tab_groups::TabGroupId> group) {
  AddTabTypes add_types =
      should_pin ? AddTabTypes::ADD_PINNED : AddTabTypes::ADD_NONE;
  int new_index = tab_strip_->InsertWebContentsAt(
      index, std::move(web_contents), add_types, group);
  return tab_strip_->GetTabAtIndex(new_index);
}

content::WebContents* TabListBridge::DiscardTab(tabs::TabHandle tab) {
  content::WebContents* contents = tab.Get()->GetContents();
  if (!contents) {
    return nullptr;
  }

  resource_coordinator::TabLifecycleUnitExternal* tab_lifecycle_unit_external =
      resource_coordinator::TabLifecycleUnitExternal::FromWebContents(contents);
  CHECK(tab_lifecycle_unit_external);
  if (tab_lifecycle_unit_external->DiscardTab(
          mojom::LifecycleUnitDiscardReason::EXTERNAL)) {
    return tab_lifecycle_unit_external->GetWebContents();
  }

  return nullptr;
}

tabs::TabInterface* TabListBridge::DuplicateTab(tabs::TabHandle tab) {
  const int index = GetIndexOfTab(tab);
  CHECK_NE(index, TabStripModel::kNoTab);

  // TODO(crbug.com/460650221): It's a bit of a code smell to reach in and grab
  // the delegate from TabStripModel, but it avoids introducing new dependencies
  // here.
  TabStripModelDelegate* delegate = tab_strip_->delegate();
  if (!delegate->CanDuplicateContentsAt(index)) {
    return nullptr;
  }

  content::WebContents* new_contents = delegate->DuplicateContentsAt(index);
  if (!new_contents) {
    return nullptr;
  }

  return tabs::TabInterface::MaybeGetFromContents(new_contents);
}

tabs::TabInterface* TabListBridge::GetTab(int index) {
  return tab_strip_->GetTabAtIndex(index);
}

int TabListBridge::GetIndexOfTab(tabs::TabHandle tab) {
  return tab_strip_->GetIndexOfTab(tab.Get());
}

void TabListBridge::HighlightTabs(tabs::TabHandle tab_to_activate,
                                  const std::set<tabs::TabHandle>& tabs) {
  CHECK(tabs.contains(tab_to_activate))
      << "Tab to activate is not included in tabs to highlight.";

  tabs::TabStripModelSelectionState selection_state(&(*tab_strip_));

  for (const auto& tab_handle : tabs) {
    auto index = tab_strip_->GetIndexOfTab(tab_handle.Get());
    CHECK_NE(index, TabStripModel::kNoTab)
        << "Trying to highlight a non-existent tab.";

    selection_state.AddTabToSelection(tab_handle.Get());
  }

  tabs::TabInterface* active_tab = tab_to_activate.Get();
  selection_state.SetActiveTab(active_tab);
  if (!selection_state.anchor_tab()) {
    selection_state.SetAnchorTab(active_tab);
  }

  tab_strip_->SetSelectionFromModel(selection_state);
}

void TabListBridge::MoveTab(tabs::TabHandle tab, int index) {
  int current_index = GetIndexOfTab(tab);
  CHECK_NE(index, TabStripModel::kNoTab)
      << "Trying to move a non-existent tab.";
  tab_strip_->MoveWebContentsAt(current_index, index,
                                /*select_after_move=*/false);
}

void TabListBridge::CloseTab(tabs::TabHandle tab) {
  const int index = GetIndexOfTab(tab);
  CHECK_NE(index, TabStripModel::kNoTab)
      << "Trying to close a tab that doesn't exist in this tab list.";
  tab_strip_->CloseWebContentsAt(index, TabCloseTypes::CLOSE_NONE);
}

std::vector<tabs::TabInterface*> TabListBridge::GetAllTabs() {
  std::vector<tabs::TabInterface*> all_tabs;
  size_t tab_count = tab_strip_->count();
  all_tabs.reserve(tab_count);
  for (tabs::TabInterface* tab : *tab_strip_) {
    all_tabs.push_back(tab);
  }
  return all_tabs;
}

void TabListBridge::PinTab(tabs::TabHandle tab) {
  int index = GetIndexOfTab(tab);
  CHECK_NE(index, TabStripModel::kNoTab)
      << "Trying to pin a tab that doesn't exist in this tab list.";
  tab_strip_->SetTabPinned(index, true);
}

void TabListBridge::UnpinTab(tabs::TabHandle tab) {
  int index = GetIndexOfTab(tab);
  CHECK_NE(index, TabStripModel::kNoTab)
      << "Trying to unpin a tab that doesn't exist in this tab list.";
  tab_strip_->SetTabPinned(index, false);
}

bool TabListBridge::ContainsTabGroup(tab_groups::TabGroupId group_id) {
  // Not all browsers support tab groups.
  if (!tab_strip_->group_model()) {
    return false;
  }
  return tab_strip_->group_model()->ContainsTabGroup(group_id);
}

std::vector<tab_groups::TabGroupId> TabListBridge::ListTabGroups() {
  // Not all browsers support tab groups.
  if (!tab_strip_->group_model()) {
    return {};
  }
  return tab_strip_->group_model()->ListTabGroups();
}

std::optional<tab_groups::TabGroupVisualData>
TabListBridge::GetTabGroupVisualData(tab_groups::TabGroupId group_id) {
  // Not all browsers support tab groups.
  if (!tab_strip_->group_model()) {
    return std::nullopt;
  }
  TabGroup* tab_group = tab_strip_->group_model()->GetTabGroup(group_id);
  if (!tab_group) {
    return std::nullopt;
  }
  const tab_groups::TabGroupVisualData* visual_data = tab_group->visual_data();
  if (!visual_data) {
    return std::nullopt;
  }
  return *visual_data;
}

gfx::Range TabListBridge::GetTabGroupTabIndices(
    tab_groups::TabGroupId group_id) {
  // Not all browsers support tab groups.
  if (!tab_strip_->group_model()) {
    return {};
  }
  TabGroup* tab_group = tab_strip_->group_model()->GetTabGroup(group_id);
  if (!tab_group) {
    return {};
  }
  return tab_group->ListTabs();
}

std::optional<tab_groups::TabGroupId> TabListBridge::CreateTabGroup(
    const std::vector<tabs::TabHandle>& tabs) {
  // Not all browsers support tab groups.
  if (!tab_strip_->group_model()) {
    return std::nullopt;
  }

  // TabStripModel::AddToNewGroup() operates on indices.
  std::vector<int> tab_indices;
  tab_indices.reserve(tabs.size());
  for (const auto& tab_handle : tabs) {
    int index = GetIndexOfTab(tab_handle);
    if (index != TabStripModel::kNoTab) {
      tab_indices.push_back(index);
    }
  }

  return tab_strip_->AddToNewGroup(std::move(tab_indices));
}

void TabListBridge::SetTabGroupVisualData(
    tab_groups::TabGroupId group_id,
    const tab_groups::TabGroupVisualData& visual_data) {
  // Not all browsers support tab groups.
  if (!tab_strip_->group_model()) {
    return;
  }
  // Use ChangeTabGroupsVisuals() to ensure observers are notified.
  tab_strip_->ChangeTabGroupVisuals(group_id, visual_data);
}

std::optional<tab_groups::TabGroupId> TabListBridge::AddTabsToGroup(
    std::optional<tab_groups::TabGroupId> group_id,
    const std::set<tabs::TabHandle>& tabs) {
  std::vector<int> tab_indices;
  tab_indices.reserve(tabs.size());

  for (const auto& tab_handle : tabs) {
    auto index = tab_strip_->GetIndexOfTab(tab_handle.Get());
    CHECK_NE(index, TabStripModel::kNoTab)
        << "Trying to add a non-existent tab to a group.";

    tab_indices.push_back(index);
  }

  std::sort(tab_indices.begin(), tab_indices.end());
  if (group_id.has_value()) {
    // No-op if the specified tab group does not exist.
    if (!tab_strip_->group_model()->ContainsTabGroup(*group_id)) {
      return std::nullopt;
    }

    tab_strip_->AddToExistingGroup(std::move(tab_indices), *group_id);
    return group_id;
  }

  return tab_strip_->AddToNewGroup(std::move(tab_indices));
}

void TabListBridge::Ungroup(const std::set<tabs::TabHandle>& tabs) {
  std::vector<int> tab_indices;
  tab_indices.reserve(tabs.size());

  for (const auto& tab_handle : tabs) {
    auto index = tab_strip_->GetIndexOfTab(tab_handle.Get());
    CHECK_NE(index, TabStripModel::kNoTab)
        << "Trying to remove a non-existent tab from a group.";

    tab_indices.push_back(index);
  }

  std::sort(tab_indices.begin(), tab_indices.end());
  tab_strip_->RemoveFromGroup(tab_indices);
}

void TabListBridge::MoveGroupTo(tab_groups::TabGroupId group_id, int index) {
  TabGroup* tab_group = tab_strip_->group_model()->GetTabGroup(group_id);
  CHECK(tab_group) << "Tab group does not exist";

  gfx::Range tabs = tab_group->ListTabs();
  CHECK_GT(tabs.length(), 0u) << "Tab group is empty";

  // Clamp the index to move for the first tab in the group to a valid index
  // within the tab strip: the group should be after all pinned tabs and before
  // the end of the tab strip.
  size_t target_index =
      std::clamp(static_cast<size_t>(index),
                 static_cast<size_t>(tab_strip_->IndexOfFirstNonPinnedTab()),
                 tab_strip_->count() - tabs.length());
  // Return early if the index to move to is the same as the group's current
  // index.
  if (tabs.start() == target_index) {
    return;
  }

  // Check that the index to move to is not in the middle of a tab group by
  // checking whether the tab at `index_to_check` and the tab to its left is in
  // the same tab group. Note that `GetTabGroupForTab` returns std::nullopt if
  // the index is out of bounds.

  // If the group will move to the right, compensate for the displacement of
  // other tabs or tab groups to the right of the group before the move by
  // adding the group's size to `target_index`. Basically, before MoveGroupTo is
  // called:
  // `index_to_check` points to the tab that will be to the right of the group
  // after the move.
  // `index_to_check` - 1 points to the tab that will be to the left of the
  // group after the move.
  const int index_to_check =
      target_index > tabs.start() ? target_index + tabs.length() : target_index;

  // If `index_to_check` is in the middle of a tab group, then find the closest
  // valid index to move to.
  target_index = GetClosestValidIndexBetweenTabGroups(
      tab_strip_.get(), target_index, index_to_check);

  // Check that `target_index` is still within bounds.
  CHECK_GE(static_cast<int>(target_index),
           tab_strip_->IndexOfFirstNonPinnedTab());
  CHECK_GE(tab_strip_->count() - tabs.length(), target_index);

  tab_strip_->MoveGroupTo(group_id, target_index);
}

void TabListBridge::MoveTabToWindow(tabs::TabHandle tab,
                                    SessionID destination_window_id,
                                    int destination_index) {
  int source_index = GetIndexOfTab(tab);
  CHECK_NE(source_index, TabStripModel::kNoTab);

  BrowserWindowInterface* target_window =
      GetBrowserWithSessionId(destination_window_id, tab_strip_->profile());
  CHECK(target_window);
  TabListInterface* target_list_interface =
      TabListInterface::From(target_window);
  CHECK(target_list_interface);
  // This is the only implementation on these platforms, so this cast is safe.
  TabListBridge* target_bridge =
      static_cast<TabListBridge*>(target_list_interface);

  std::unique_ptr<tabs::TabModel> detached_tab =
      tab_strip_->DetachTabAtForInsertion(source_index);
  if (!detached_tab) {
    return;
  }

  target_bridge->tab_strip_->InsertDetachedTabAt(
      destination_index, std::move(detached_tab), AddTabTypes::ADD_NONE);
}

void TabListBridge::MoveTabGroupToWindow(tab_groups::TabGroupId group_id,
                                         SessionID destination_window_id,
                                         int destination_index) {
  BrowserWindowInterface* target_window =
      GetBrowserWithSessionId(destination_window_id, tab_strip_->profile());
  CHECK(target_window);
  TabListInterface* target_list_interface =
      TabListInterface::From(target_window);
  CHECK(target_list_interface);
  // This is the only implementation on these platforms, so this cast is safe.
  TabListBridge* target_bridge =
      static_cast<TabListBridge*>(target_list_interface);
  auto target_tab_strip = target_bridge->tab_strip_;
  if (!target_tab_strip->SupportsTabGroups()) {
    return;
  }

  // Handle the case where `destination_window_id` points to the same window.
  if (this == target_bridge) {
    MoveGroupTo(group_id, destination_index);
    return;
  }

  TabGroup* tab_group = tab_strip_->group_model()->GetTabGroup(group_id);
  CHECK(tab_group) << "Tab group does not exist";
  CHECK_GT(tab_group->ListTabs().length(), 0u) << "Tab group is empty";

  // Clamp the index for the destination window so that the tab group will be
  // after the pinned tabs.
  int target_index = std::clamp(destination_index,
                                target_tab_strip->IndexOfFirstNonPinnedTab(),
                                target_tab_strip->count());

  // If `target_index` is in the middle of a tab group, then find the closest
  // valid index to move to.
  target_index = GetClosestValidIndexBetweenTabGroups(
      target_tab_strip.get(), target_index, /*index_to_check=*/target_index);

  // Check that `target_index` is still within bounds.
  CHECK_GE(static_cast<int>(target_index),
           target_tab_strip->IndexOfFirstNonPinnedTab());
  CHECK_GE(target_tab_strip->count(), target_index);

  // When moving a group between windows, Saved Tab Groups must pause listening
  // since the group is in an invalid state.
  tab_groups::TabGroupSyncService* tab_group_sync_service =
      tab_groups::TabGroupSyncServiceFactory::GetForProfile(
          target_tab_strip->profile());
  std::unique_ptr<tab_groups::ScopedLocalObservationPauser>
      tab_groups_sync_movement_observation;
  if (tab_group_sync_service) {
    tab_groups_sync_movement_observation =
        tab_group_sync_service->CreateScopedLocalObserverPauser();
  }

  std::unique_ptr<DetachedTabCollection> detached_group =
      tab_strip_->DetachTabGroupForInsertion(group_id);
  target_tab_strip->InsertDetachedTabGroupAt(std::move(detached_group),
                                             target_index);
}

void TabListBridge::OnTabStripModelChanged(
    TabStripModel* tab_strip_model,
    const TabStripModelChange& change,
    const TabStripSelectionChange& selection) {
  switch (change.type()) {
    case TabStripModelChange::kInserted: {
      // See comment on TabStripModelChange::Insert for notes about the format
      // of `contents`.
      // NOTE: This might be *unsafe* if callers mutate the tab strip model
      // synchronously from this event.
      for (const auto& web_contents_and_index : change.GetInsert()->contents) {
        // This will (correctly) crash if `tab` is not found. Since we just
        // inserted the tab, we know it should exist.
        tabs::TabInterface* tab = web_contents_and_index.tab.get();
        for (auto& observer : observers_) {
          observer.OnTabAdded(*this, tab, web_contents_and_index.index);
        }
      }
      break;
    }
    case TabStripModelChange::kRemoved:
      for (const auto& removed_tab : change.GetRemove()->contents) {
        tabs::TabInterface* tab = removed_tab.tab.get();
        TabRemovedReason reason = removed_tab.remove_reason;
        for (auto& observer : observers_) {
          observer.OnTabRemoved(*this, tab, reason);
        }
      }
      break;
    case TabStripModelChange::kMoved:
      for (auto& observer : observers_) {
        observer.OnTabMoved(*this, change.GetMove()->tab.get(),
                            change.GetMove()->from_index,
                            change.GetMove()->to_index);
      }
      break;
    case TabStripModelChange::kReplaced:
    case TabStripModelChange::kSelectionOnly:
      break;
  }

  if (selection.active_tab_changed()) {
    tabs::TabInterface* tab = tab_strip_->GetActiveTab();
    if (tab) {
      for (auto& observer : observers_) {
        observer.OnActiveTabChanged(*this, tab);
      }
    }
  }

  if (selection.selection_changed()) {
    std::set<tabs::TabInterface*> selected_tabs(
        tab_strip_->selection_model().selected_tabs().begin(),
        tab_strip_->selection_model().selected_tabs().end());
    for (auto& observer : observers_) {
      observer.OnHighlightedTabsChanged(*this, selected_tabs);
    }
  }
}

bool TabListBridge::IsThisTabListEditable() {
  TabStripModelDelegate* delegate = tab_strip_->delegate();
  return delegate->IsTabStripEditable();
}

bool TabListBridge::IsClosingAllTabs() {
  return tab_strip_->closing_all();
}

void TabListBridge::WillCloseAllTabs(TabStripModel* model) {
  for (auto& observer : observers_) {
    observer.OnAllTabsAreClosing(*this);
  }
}

// static
// From //chrome/browser/tab_list/tab_list_interface.h
bool TabListInterface::CanEditTabList(Profile& profile) {
  std::vector<BrowserWindowInterface*> all_browsers =
      GetAllBrowserWindowInterfaces();
  for (auto* browser : all_browsers) {
    if (browser->GetProfile() != &profile) {
      continue;
    }

    TabListInterface* tab_list = From(browser);
    if (!tab_list) {
      continue;
    }

    if (!tab_list->IsThisTabListEditable()) {
      return false;
    }
  }

  return true;
}
