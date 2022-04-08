// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/tabs/saved_tab_groups/saved_tab_group_model_listener.h"

#include "base/containers/flat_set.h"
#include "base/memory/raw_ptr.h"
#include "base/ranges/algorithm.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/tabs/saved_tab_groups/saved_tab_group_model.h"
#include "chrome/browser/ui/tabs/tab_group.h"
#include "chrome/browser/ui/tabs/tab_group_model.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"

SavedTabGroupModelListener::SavedTabGroupModelListener() = default;

SavedTabGroupModelListener::SavedTabGroupModelListener(
    SavedTabGroupModel* model)
    : model_(model) {
  BrowserList::GetInstance()->AddObserver(this);
  for (Browser* browser : *BrowserList::GetInstance())
    OnBrowserAdded(browser);
}

SavedTabGroupModelListener::~SavedTabGroupModelListener() {
  BrowserList::GetInstance()->RemoveObserver(this);
  for (Browser* browser : *BrowserList::GetInstance()) {
    // Note: Can no longer call OnBrowserRemoved here because model_ is already
    // destroyed.
    observed_browsers_.erase(browser);
    browser->tab_strip_model()->RemoveObserver(this);
  }
}

TabStripModel* SavedTabGroupModelListener::GetTabStripModelWithTabGroupId(
    tab_groups::TabGroupId group_id) {
  auto contains_tab_group = [&](TabStripModel* model) {
    return model->group_model()->ContainsTabGroup(group_id);
  };
  base::flat_set<raw_ptr<Browser>>::iterator it = base::ranges::find_if(
      observed_browsers_, contains_tab_group, &Browser::tab_strip_model);
  return it != observed_browsers_.end() ? it->get()->tab_strip_model()
                                        : nullptr;
}

void SavedTabGroupModelListener::OnBrowserAdded(Browser* browser) {
  if (model_->profile() != browser->profile())
    return;
  observed_browsers_.insert(browser);
  browser->tab_strip_model()->AddObserver(this);
}

void SavedTabGroupModelListener::OnBrowserRemoved(Browser* browser) {
  if (model_->profile() != browser->profile())
    return;
  observed_browsers_.erase(browser);
  browser->tab_strip_model()->RemoveObserver(this);
}

void SavedTabGroupModelListener::OnTabGroupChanged(
    const TabGroupChange& change) {
  const TabStripModel* tab_strip_model = change.model;
  if (!model_->Contains(change.group))
    return;

  const TabGroup* group =
      tab_strip_model->group_model()->GetTabGroup(change.group);
  switch (change.type) {
    // Called when a group is created the first tab is added.
    case TabGroupChange::kCreated: {
      NOTIMPLEMENTED();
      return;
    }
    // Called when a tab groups editor menu is opened.
    case TabGroupChange::kEditorOpened: {
      NOTIMPLEMENTED();
      return;
    }
    // Called when the tabs in the group changes.
    case TabGroupChange::kContentsChanged: {
      // TODO(dljames): kContentsChanged will update the urls associated with
      // the group stored in the model with TabGroupId change.group.
      NOTIMPLEMENTED();
      return;
    }
    // Called when a groups title or color changes
    case TabGroupChange::kVisualsChanged: {
      const tab_groups::TabGroupVisualData* visual_data = group->visual_data();
      model_->Update(change.group, visual_data);
      return;
    }
    // Called when a groups is moved by interacting with its header.
    case TabGroupChange::kMoved: {
      NOTIMPLEMENTED();
      return;
    }
    // Called when the last tab in the groups is removed.
    case TabGroupChange::kClosed: {
      NOTIMPLEMENTED();
      return;
    }
  }
}
