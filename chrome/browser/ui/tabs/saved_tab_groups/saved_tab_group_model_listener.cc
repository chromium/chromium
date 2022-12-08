// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/tabs/saved_tab_groups/saved_tab_group_model_listener.h"

#include "base/containers/flat_set.h"
#include "base/memory/raw_ptr.h"
#include "base/ranges/algorithm.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/tabs/tab_group.h"
#include "chrome/browser/ui/tabs/tab_group_model.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "components/saved_tab_groups/saved_tab_group_model.h"

// TODO(crbug/1376259): Update SavedTabGroupModel state with any groups that
// should be in the SavedTabGroupModel.
SavedTabGroupBrowserListener::SavedTabGroupBrowserListener(
    Browser* browser,
    SavedTabGroupModel* model)
    : browser_(browser), model_(model) {
  DCHECK(browser_);
  browser_->tab_strip_model()->AddObserver(this);
}

SavedTabGroupBrowserListener::~SavedTabGroupBrowserListener() {
  if (browser_)
    browser_->tab_strip_model()->RemoveObserver(this);
}

bool SavedTabGroupBrowserListener::ContainsTabGroup(
    tab_groups::TabGroupId group_id) const {
  return browser_->tab_strip_model()->group_model()->ContainsTabGroup(group_id);
}

void SavedTabGroupBrowserListener::OnTabGroupChanged(
    const TabGroupChange& change) {
  const TabStripModel* tab_strip_model = change.model;
  if (!model_->Contains(change.group))
    return;

  const TabGroup* group =
      tab_strip_model->group_model()->GetTabGroup(change.group);
  switch (change.type) {
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
      model_->UpdateVisualData(change.group, visual_data);
      return;
    }
    // Called when the last tab in the groups is removed.
    case TabGroupChange::kClosed: {
      model_->OnGroupClosedInTabStrip(change.group);
      return;
    }
    // Created is ignored because we explicitly add the TabGroupId to the saved
    // tab group outside of the observer flow. kEditorOpened does not affect the
    // SavedTabGroup, and kMoved does not affect the order of the saved tab
    // groups.
    case TabGroupChange::kCreated:
    case TabGroupChange::kEditorOpened:
    case TabGroupChange::kMoved: {
      NOTIMPLEMENTED();
      return;
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
  BrowserList::GetInstance()->AddObserver(this);
  for (Browser* browser : *BrowserList::GetInstance())
    OnBrowserAdded(browser);
}

SavedTabGroupModelListener::~SavedTabGroupModelListener() {
  BrowserList::GetInstance()->RemoveObserver(this);
  observed_browser_listeners_.clear();
}

TabStripModel* SavedTabGroupModelListener::GetTabStripModelWithTabGroupId(
    tab_groups::TabGroupId group_id) {
  auto it = base::ranges::find_if(
      observed_browser_listeners_, [group_id](const auto& listener_pair) {
        return listener_pair.second.ContainsTabGroup(group_id);
      });
  return it != observed_browser_listeners_.end()
             ? it->second.browser()->tab_strip_model()
             : nullptr;
}

void SavedTabGroupModelListener::OnBrowserAdded(Browser* browser) {
  if (profile_ != browser->profile())
    return;

  // TODO(crbug.com/1345680): Investigate the root cause of duplicate calls.
  if (observed_browser_listeners_.count(browser) > 0)
    return;

  observed_browser_listeners_.try_emplace(browser, browser, model_);
}

void SavedTabGroupModelListener::OnBrowserRemoved(Browser* browser) {
  if (profile_ != browser->profile())
    return;
  observed_browser_listeners_.erase(browser);
}
