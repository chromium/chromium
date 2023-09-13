// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/tabs/saved_tab_groups/saved_tab_group_model_listener.h"

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
#include "components/saved_tab_groups/saved_tab_group_model.h"
#include "components/tab_groups/tab_group_id.h"

namespace content {
class WebContents;
}

SavedTabGroupModelListener::SavedTabGroupModelListener() = default;

SavedTabGroupModelListener::SavedTabGroupModelListener(
    SavedTabGroupModel* model,
    Profile* profile)
    : model_(model), profile_(profile) {
  DCHECK(model);
  DCHECK(profile);
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

    // Ignored because closing empty groups is handled when the last tab is
    // removed in TabGroupedStateChanged.
    case TabGroupChange::kClosed:
    // Ignored because contents changes are handled in TabGroupedStateChanged.
    case TabGroupChange::kContentsChanged:
    // Ignored because we explicitly add the TabGroupId to the saved tab group
    // outside of the observer flow.
    case TabGroupChange::kCreated:
    // kEditorOpened doesn't affect the SavedTabGroup.
    case TabGroupChange::kEditorOpened:
    // kMoved doesn't affect the order of the saved tab groups.
    case TabGroupChange::kMoved: {
      return;
    }
  }
}

void SavedTabGroupModelListener::TabGroupedStateChanged(
    absl::optional<tab_groups::TabGroupId> new_local_group_id,
    content::WebContents* contents,
    int index) {
  // Remove `contents` from its current saved group, if it's in one.
  for (auto& [local_group_id, listener] : local_tab_group_listeners_) {
    if (local_group_id != new_local_group_id) {
      if (listener.MaybeRemoveWebContentsFromLocal(contents) ==
          LocalTabGroupListener::Liveness::kGroupDeleted) {
        // If this emptied the group, the saved group was removed, so we must
        // stop listening to `local_group_id`.
        DisconnectLocalTabGroup(local_group_id);
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
    listener.AddWebContentsFromLocal(contents, browser->tab_strip_model(),
                                     index);
  }
}

void SavedTabGroupModelListener::OnTabStripModelChanged(
    TabStripModel* tab_strip_model,
    const TabStripModelChange& change,
    const TabStripSelectionChange& selection) {
  switch (change.type()) {
    case TabStripModelChange::kReplaced: {
      absl::optional<tab_groups::TabGroupId> local_id =
          tab_strip_model->GetTabGroupForTab(change.GetReplace()->index);

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

      local_tab_group_listener.OnReplaceWebContents(
          change.GetReplace()->old_contents, change.GetReplace()->new_contents);
      return;
    }
    case TabStripModelChange::kMoved: {
      absl::optional<tab_groups::TabGroupId> local_id =
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
      DisconnectLocalTabGroup(group_id);
    }
  }
}

void SavedTabGroupModelListener::ConnectToLocalTabGroup(
    const SavedTabGroup& saved_tab_group,
    std::map<content::WebContents*, base::Uuid> web_contents_map) {
  const tab_groups::TabGroupId local_group_id =
      saved_tab_group.local_group_id().value();

  // `web_contents_map` should have one entry per tab in the local group. This
  // may not equal the saved group's size, if the saved group contains invalid
  // URLs.
  const size_t local_group_size =
      SavedTabGroupUtils::GetTabGroupWithId(local_group_id)->tab_count();
  CHECK_EQ(local_group_size, web_contents_map.size());

  auto [iterator, success] = local_tab_group_listeners_.try_emplace(
      local_group_id, local_group_id, saved_tab_group.saved_guid(), model_,
      web_contents_map);
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

void SavedTabGroupModelListener::DisconnectLocalTabGroup(
    tab_groups::TabGroupId tab_group_id) {
  model_->OnGroupClosedInTabStrip(tab_group_id);
  local_tab_group_listeners_.erase(tab_group_id);
}

void SavedTabGroupModelListener::RemoveLocalGroupFromSync(
    tab_groups::TabGroupId local_group_id) {
  if (!base::Contains(local_tab_group_listeners_, local_group_id)) {
    return;
  }

  local_tab_group_listeners_.at(local_group_id).GroupRemovedFromSync();
  DisconnectLocalTabGroup(local_group_id);
}

void SavedTabGroupModelListener::UpdateLocalGroupFromSync(
    tab_groups::TabGroupId local_group_id) {
  if (!base::Contains(local_tab_group_listeners_, local_group_id)) {
    return;
  }

  if (local_tab_group_listeners_.at(local_group_id).UpdateFromSync() ==
      LocalTabGroupListener::Liveness::kGroupDeleted) {
    DisconnectLocalTabGroup(local_group_id);
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
