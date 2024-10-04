// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/tabs/saved_tab_groups/saved_tab_group_model_listener.h"

#include "base/check.h"
#include "base/containers/contains.h"
#include "base/memory/raw_ptr.h"
#include "base/ranges/algorithm.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/tabs/saved_tab_groups/local_tab_group_listener.h"
#include "chrome/browser/ui/tabs/saved_tab_groups/saved_tab_group_utils.h"
#include "chrome/browser/ui/tabs/tab_group.h"
#include "chrome/browser/ui/tabs/tab_group_model.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "components/saved_tab_groups/internal/saved_tab_group_model.h"
#include "components/saved_tab_groups/public/features.h"
#include "components/saved_tab_groups/public/tab_group_sync_service.h"
#include "components/tab_groups/tab_group_id.h"

namespace content {
class WebContents;
}

namespace tab_groups {

SavedTabGroupModelListener::SavedTabGroupModelListener() = default;

SavedTabGroupModelListener::SavedTabGroupModelListener(
    TabGroupSyncService* service,
    Profile* profile)
    : service_(service), profile_(profile) {
  CHECK(service);
  CHECK(profile);

  for (Browser* browser : *BrowserList::GetInstance()) {
    OnBrowserAdded(browser);
  }

  BrowserList::GetInstance()->AddObserver(this);
}

SavedTabGroupModelListener::~SavedTabGroupModelListener() {
  BrowserList::GetInstance()->RemoveObserver(this);
  for (Browser* browser : *BrowserList::GetInstance()) {
    OnBrowserRemoved(browser);
  }
}

void SavedTabGroupModelListener::OnTabGroupAdded(
    const tab_groups::TabGroupId& group_id) {
  if (!tab_groups::IsTabGroupSyncServiceDesktopMigrationEnabled()) {
    return;
  }

  if (!tab_groups::IsTabGroupsSaveV2Enabled()) {
    return;
  }

  if (local_tab_group_listeners_.contains(group_id)) {
    return;
  }

  auto group_and_tab_guid_mapping = CreateSavedTabGroupAndTabMapping(group_id);

  SavedTabGroup copy_group = group_and_tab_guid_mapping.first;
  std::map<tabs::TabModel*, base::Uuid>& tab_guid_mapping =
      group_and_tab_guid_mapping.second;
  service_->AddGroup(std::move(copy_group));

  std::optional<SavedTabGroup> group = service_->GetGroup(group_id);
  ConnectToLocalTabGroup(group.value(), tab_guid_mapping);
}

void SavedTabGroupModelListener::OnTabGroupWillBeRemoved(
    const tab_groups::TabGroupId& group_id) {
  if (!tab_groups::IsTabGroupSyncServiceDesktopMigrationEnabled()) {
    return;
  }

  if (!local_tab_group_listeners_.contains(group_id)) {
    return;
  }

  DisconnectLocalTabGroup(group_id, ClosingSource::kDeletedByUser);
}

void SavedTabGroupModelListener::OnTabGroupChanged(
    const TabGroupChange& change) {
  if (!local_tab_group_listeners_.contains(change.group)) {
    return;
  }

  switch (change.type) {
    // Called when a group's title or color changes.
    case TabGroupChange::kVisualsChanged: {
      local_tab_group_listeners_.at(change.group)
          .UpdateVisualDataFromLocal(change.GetVisualsChange());
      return;
    }

    // We should never get close notifications, because we destroy the
    // LocalTabGroupListener when the last tab is closed, which happens before
    // this event is sent out.
    case TabGroupChange::kClosed:
    // We should never get created notifications, because we only are connected
    // to the local group after it has been created and populated.
    case TabGroupChange::kCreated: {
      // The exception to both of these is when the group is being moved between
      // browser windows, as it gets created in the new window and destroyed in
      // the old window. In these cases, tracking must be paused, as otherwise
      // the saved group will get emptied out during the move.
      CHECK(local_tab_group_listeners_.at(change.group).IsTrackingPaused());
      return;
    }

    // Ignored because contents changes are handled in TabGroupedStateChanged.
    case TabGroupChange::kContentsChanged:
    // kEditorOpened doesn't affect the SavedTabGroup.
    case TabGroupChange::kEditorOpened:
    // kMoved doesn't affect the order of the saved tab groups.
    case TabGroupChange::kMoved: {
      return;
    }
  }
}

void SavedTabGroupModelListener::TabGroupedStateChanged(
    std::optional<tab_groups::TabGroupId> new_local_group_id,
    tabs::TabModel* tab,
    int index) {
  // Remove `contents` from its current saved group, if it's in one.
  for (auto& [local_group_id, listener] : local_tab_group_listeners_) {
    if (local_group_id != new_local_group_id) {
      if (listener.MaybeRemoveWebContentsFromLocal(tab->contents()) ==
          LocalTabGroupListener::Liveness::kGroupDeleted) {
        // If this emptied the group, the saved group was removed, so we must
        // stop listening to `local_group_id`.
        DisconnectLocalTabGroup(local_group_id, ClosingSource::kUnknown);
        // Not only did we find our old group, we also concurrently modified the
        // data structure we're iterating over. Abort, abort.
        break;
      }
    }
  }

  // Add it to its new group.
  if (new_local_group_id.has_value() &&
      base::Contains(local_tab_group_listeners_, new_local_group_id.value())) {
    LocalTabGroupListener& listener =
        local_tab_group_listeners_.at(new_local_group_id.value());
    const Browser* const browser = SavedTabGroupUtils::GetBrowserWithTabGroupId(
        new_local_group_id.value());
    CHECK(browser);
    listener.AddTabFromLocal(tab, browser->tab_strip_model(), index);
  }
}

void SavedTabGroupModelListener::OnTabStripModelChanged(
    TabStripModel* tab_strip_model,
    const TabStripModelChange& change,
    const TabStripSelectionChange& selection) {
  switch (change.type()) {
    case TabStripModelChange::kMoved: {
      std::optional<tab_groups::TabGroupId> local_id =
          tab_strip_model->GetTabGroupForTab(change.GetMove()->to_index);

      // Do nothing if the tab is no longer in a group.
      if (!local_id.has_value()) {
        return;
      }

      // Do nothing if the tab is not part of a saved group.
      if (!local_tab_group_listeners_.contains(local_id.value())) {
        return;
      }

      LocalTabGroupListener& local_tab_group_listener =
          local_tab_group_listeners_.at(local_id.value());

      local_tab_group_listener.MoveWebContentsFromLocal(
          tab_strip_model, change.GetMove()->contents,
          change.GetMove()->to_index);

      return;
    }
    case TabStripModelChange::kSelectionOnly:
    case TabStripModelChange::kInserted:
    case TabStripModelChange::kReplaced:
    case TabStripModelChange::kRemoved: {
      return;
    }
  }
}

void SavedTabGroupModelListener::WillCloseAllTabs(
    TabStripModel* tab_strip_model) {
  CHECK(tab_strip_model);
  if (!tab_strip_model->group_model()) {
    return;
  }

  for (const tab_groups::TabGroupId& group_id :
       tab_strip_model->group_model()->ListTabGroups()) {
    if (base::Contains(local_tab_group_listeners_, group_id)) {
      DisconnectLocalTabGroup(group_id, ClosingSource::kCloseAllTabs);
    }
  }
}

bool SavedTabGroupModelListener::IsTrackingLocalTabGroup(
    const tab_groups::TabGroupId& group_id) {
  return local_tab_group_listeners_.contains(group_id);
}

void SavedTabGroupModelListener::ConnectToLocalTabGroup(
    const SavedTabGroup& saved_tab_group,
    std::map<tabs::TabModel*, base::Uuid> tab_guid_mapping) {
  const tab_groups::TabGroupId local_group_id =
      saved_tab_group.local_group_id().value();

  // `tab_guid_mapping` should have one entry per tab in the local group. This
  // may not equal the saved group's size, if the saved group contains invalid
  // URLs.
  const size_t local_group_size =
      SavedTabGroupUtils::GetTabGroupWithId(local_group_id)->tab_count();
  CHECK_EQ(local_group_size, tab_guid_mapping.size());

  auto [iterator, success] = local_tab_group_listeners_.try_emplace(
      local_group_id, local_group_id, saved_tab_group.saved_guid(), service_,
      tab_guid_mapping);
  CHECK(success);
}

void SavedTabGroupModelListener::PauseTrackingLocalTabGroup(
    const tab_groups::TabGroupId& group_id) {
  if (!base::Contains(local_tab_group_listeners_, group_id)) {
    return;
  }
  local_tab_group_listeners_.at(group_id).PauseTracking();
}

void SavedTabGroupModelListener::ResumeTrackingLocalTabGroup(
    const tab_groups::TabGroupId& group_id) {
  if (!base::Contains(local_tab_group_listeners_, group_id)) {
    return;
  }
  local_tab_group_listeners_.at(group_id).ResumeTracking();
}

void SavedTabGroupModelListener::PauseLocalObservation() {
  for (auto& pair : local_tab_group_listeners_) {
    LocalTabGroupListener& listener = pair.second;
    listener.PauseTracking();
  }
}

void SavedTabGroupModelListener::ResumeLocalObservation() {
  for (auto& pair : local_tab_group_listeners_) {
    LocalTabGroupListener& listener = pair.second;
    listener.ResumeTracking();
  }
}

void SavedTabGroupModelListener::DisconnectLocalTabGroup(
    tab_groups::TabGroupId tab_group_id,
    ClosingSource closing_source) {
  service_->RemoveLocalTabGroupMapping(tab_group_id, closing_source);
  local_tab_group_listeners_.erase(tab_group_id);
}

void SavedTabGroupModelListener::RemoveLocalGroupFromSync(
    tab_groups::TabGroupId local_group_id) {
  if (!base::Contains(local_tab_group_listeners_, local_group_id)) {
    return;
  }

  local_tab_group_listeners_.at(local_group_id).GroupRemovedFromSync();
  DisconnectLocalTabGroup(local_group_id, ClosingSource::kDeletedFromSync);
}

void SavedTabGroupModelListener::UpdateLocalGroupFromSync(
    tab_groups::TabGroupId local_group_id) {
  if (!base::Contains(local_tab_group_listeners_, local_group_id)) {
    return;
  }

  if (local_tab_group_listeners_.at(local_group_id).UpdateFromSync() ==
      LocalTabGroupListener::Liveness::kGroupDeleted) {
    DisconnectLocalTabGroup(local_group_id, ClosingSource::kDeletedFromSync);
  }
}

void SavedTabGroupModelListener::OnBrowserAdded(Browser* browser) {
  if (profile_ != browser->profile()) {
    return;
  }

  browser->tab_strip_model()->AddObserver(this);
}

void SavedTabGroupModelListener::OnBrowserRemoved(Browser* browser) {
  if (profile_ != browser->profile()) {
    return;
  }

  browser->tab_strip_model()->RemoveObserver(this);
}

std::pair<SavedTabGroup, std::map<tabs::TabModel*, base::Uuid>>
SavedTabGroupModelListener::CreateSavedTabGroupAndTabMapping(
    const tab_groups::TabGroupId& group_id) {
  Browser* browser =
      tab_groups::SavedTabGroupUtils::GetBrowserWithTabGroupId(group_id);
  CHECK(browser);
  TabStripModel* tab_strip_model = browser->tab_strip_model();
  CHECK(tab_strip_model);
  CHECK(tab_strip_model->SupportsTabGroups());

  TabGroup* tab_group = tab_strip_model->group_model()->GetTabGroup(group_id);

  tab_groups::SavedTabGroup saved_tab_group(
      tab_group->visual_data()->title(), tab_group->visual_data()->color(), {},
      std::nullopt, std::nullopt, group_id);
  saved_tab_group.SetPinned(
      /*pinned=*/tab_groups::SavedTabGroupUtils::ShouldAutoPinNewTabGroups(
          profile_));

  const gfx::Range tab_range = tab_group->ListTabs();
  std::map<tabs::TabModel*, base::Uuid> tab_guid_mapping;
  for (auto i = tab_range.start(); i < tab_range.end(); ++i) {
    tabs::TabModel* tab = tab_strip_model->GetTabAtIndex(i);
    CHECK(tab);

    tab_groups::SavedTabGroupTab saved_tab_group_tab =
        tab_groups::SavedTabGroupUtils::CreateSavedTabGroupTabFromWebContents(
            tab->contents(), saved_tab_group.saved_guid());

    tab_guid_mapping.emplace(tab, saved_tab_group_tab.saved_tab_guid());
    saved_tab_group.AddTabLocally(std::move(saved_tab_group_tab));
  }

  return std::pair(std::move(saved_tab_group), std::move(tab_guid_mapping));
}

}  // namespace tab_groups
