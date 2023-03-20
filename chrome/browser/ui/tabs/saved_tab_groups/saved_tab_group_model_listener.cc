// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/tabs/saved_tab_groups/saved_tab_group_model_listener.h"

#include "base/memory/raw_ptr.h"
#include "base/ranges/algorithm.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/tabs/saved_tab_groups/local_tab_group_listener.h"
#include "chrome/browser/ui/tabs/tab_group.h"
#include "chrome/browser/ui/tabs/tab_group_model.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "components/saved_tab_groups/saved_tab_group_model.h"
#include "components/tab_groups/tab_group_id.h"

namespace content {
class WebContents;
}

void SavedTabGroupModelListener::OnTabGroupChanged(
    const TabGroupChange& change) {
  const TabStripModel* tab_strip_model = change.model;
  if (!model_->Contains(change.group)) {
    return;
  }

  const TabGroup* group =
      tab_strip_model->group_model()->GetTabGroup(change.group);
  switch (change.type) {
    // Called when a group's title or color changes.
    case TabGroupChange::kVisualsChanged: {
      const tab_groups::TabGroupVisualData* visual_data = group->visual_data();
      model_->UpdateVisualData(change.group, visual_data);
      return;
    }

    // Called when the last tab in the groups is removed.
    case TabGroupChange::kClosed: {
      model_->OnGroupClosedInTabStrip(change.group);
      return;
    }

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
      listener.RemoveWebContentsIfPresent(contents);
    }
  }

  // Add it to its new group.
  if (new_local_group_id.has_value() &&
      local_tab_group_listeners_.contains(new_local_group_id.value())) {
    LocalTabGroupListener& listener =
        local_tab_group_listeners_.at(new_local_group_id.value());
    const Browser* const browser =
        GetBrowserWithTabGroupId(new_local_group_id.value());
    CHECK(browser);
    listener.AddWebContents(contents, browser->tab_strip_model(), index);
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
    if (local_tab_group_listeners_.contains(group_id)) {
      DisconnectLocalTabGroup(group_id);
    }
  }
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

Browser* SavedTabGroupModelListener::GetBrowserWithTabGroupId(
    tab_groups::TabGroupId group_id) const {
  for (Browser* browser : *BrowserList::GetInstance()) {
    if (browser->tab_strip_model()->group_model()->ContainsTabGroup(group_id)) {
      return browser;
    }
  }
  return nullptr;
}

void SavedTabGroupModelListener::ConnectToLocalTabGroup(
    const SavedTabGroup& saved_tab_group,
    std::vector<std::pair<content::WebContents*, base::GUID>> mapping) {
  const tab_groups::TabGroupId local_group_id =
      saved_tab_group.local_group_id().value();

  // `mapping` should have one entry per tab in the local group. This may not
  // equal the saved group's size, if the saved group contains invalid URLs.
  const size_t local_group_size = GetBrowserWithTabGroupId(local_group_id)
                                      ->tab_strip_model()
                                      ->group_model()
                                      ->GetTabGroup(local_group_id)
                                      ->tab_count();
  CHECK_EQ(local_group_size, mapping.size());

  auto [iterator, success] = local_tab_group_listeners_.try_emplace(
      local_group_id, local_group_id, saved_tab_group.saved_guid(), model_,
      mapping);
  CHECK(success);
}

void SavedTabGroupModelListener::DisconnectLocalTabGroup(
    tab_groups::TabGroupId tab_group_id) {
  local_tab_group_listeners_.erase(tab_group_id);
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
