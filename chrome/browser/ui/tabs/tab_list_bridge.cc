// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/tabs/tab_list_bridge.h"

#include "base/check_op.h"
#include "base/notimplemented.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface_iterator.h"
#include "chrome/browser/ui/tabs/tab_enums.h"
#include "chrome/browser/ui/tabs/tab_group_model.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/tabs/tab_strip_model_delegate.h"
#include "components/tabs/public/tab_group.h"
#include "components/tabs/public/tab_interface.h"

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

}  // namespace

DEFINE_USER_DATA(TabListBridge);

TabListBridge::TabListBridge(TabStripModel& tab_strip_model,
                             ui::UnownedUserDataHost& unowned_user_data_host)
    : tab_strip_(tab_strip_model),
      scoped_data_holder_(unowned_user_data_host, *this) {
  tab_strip_->AddObserver(this);
}

// Note: TabStripObserver already implements RemoveObserver() calls; no need to
// remove this object as an observer here.
TabListBridge::~TabListBridge() = default;

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

tabs::TabInterface* TabListBridge::OpenTab(const GURL& url, int index) {
  NOTIMPLEMENTED();
  return nullptr;
}

void TabListBridge::DiscardTab(tabs::TabHandle tab) {}

tabs::TabInterface* TabListBridge::DuplicateTab(tabs::TabHandle tab) {
  // TODO(dpenning): It's a bit of a code smell to reach in and grab the
  // delegate from TabStripModel, but it avoids introducing new dependencies
  // here.
  TabStripModelDelegate* delegate = tab_strip_->delegate();

  const int index = GetIndexOfTab(tab);
  CHECK_NE(index, TabStripModel::kNoTab);

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

  ui::ListSelectionModel selected_tabs = tab_strip_->selection_model();
  for (const auto& tab_handle : tabs) {
    auto index = tab_strip_->GetIndexOfTab(tab_handle.Get());
    CHECK_NE(index, TabStripModel::kNoTab)
        << "Trying to highlight a non-existent tab.";

    selected_tabs.AddIndexToSelection(index);
  }

  selected_tabs.set_active(tab_strip_->GetIndexOfTab(tab_to_activate.Get()));
  tab_strip_->SetSelectionFromModel(std::move(selected_tabs));
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
  // We don't know the group size because all we have here is a group id...
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
  const size_t index_to_check =
      target_index > tabs.start() ? target_index + tabs.length() : target_index;

  std::optional<tab_groups::TabGroupId> target_group =
      tab_strip_->GetTabGroupForTab(index_to_check);
  std::optional<tab_groups::TabGroupId> adjacent_group =
      tab_strip_->GetTabGroupForTab(index_to_check - 1);

  // TODO(crbug.com/460650221) As mentioned in the comment in TabListInterface,
  // use the closest valid index if `index_to_check` falls in the middle of a
  // group.
  CHECK(!target_group.has_value() || target_group != adjacent_group)
      << "Cannot move tab group into the middle of another group.";

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
                                         int destination_index) {}

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
          observer.OnTabAdded(tab, web_contents_and_index.index);
        }
      }
      break;
    }
    case TabStripModelChange::kRemoved:
    case TabStripModelChange::kMoved:
    case TabStripModelChange::kReplaced:
    case TabStripModelChange::kSelectionOnly:
      break;
  }

  if (selection.active_tab_changed()) {
    tabs::TabInterface* tab = tab_strip_->GetActiveTab();
    if (tab) {
      for (auto& observer : observers_) {
        observer.OnActiveTabChanged(tab);
      }
    }
  }
}

// static
// From //chrome/browser/ui/tabs/tab_list_interface.h
TabListInterface* TabListInterface::From(
    BrowserWindowInterface* browser_window_interface) {
  return ui::ScopedUnownedUserData<TabListBridge>::Get(
      browser_window_interface->GetUnownedUserDataHost());
}
