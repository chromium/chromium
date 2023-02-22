// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/tabs/saved_tab_groups/saved_tab_group_model_listener.h"

#include "base/containers/flat_set.h"
#include "base/memory/raw_ptr.h"
#include "base/ranges/algorithm.h"
#include "chrome/browser/favicon/favicon_utils.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/tabs/saved_tab_groups/saved_tab_group_keyed_service.h"
#include "chrome/browser/ui/tabs/saved_tab_groups/saved_tab_group_service_factory.h"
#include "chrome/browser/ui/tabs/saved_tab_groups/saved_tab_group_utils.h"
#include "chrome/browser/ui/tabs/tab_group.h"
#include "chrome/browser/ui/tabs/tab_group_model.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "components/saved_tab_groups/saved_tab_group_model.h"
#include "content/public/browser/web_contents.h"
#include "ui/base/page_transition_types.h"

SavedTabGroupWebContentsListener::SavedTabGroupWebContentsListener(
    content::WebContents* web_contents,
    base::Token token,
    SavedTabGroupModel* model)
    : token_(token), web_contents_(web_contents), model_(model) {
  Observe(web_contents_);
}

SavedTabGroupWebContentsListener::~SavedTabGroupWebContentsListener() = default;

void SavedTabGroupWebContentsListener::DidFinishNavigation(
    content::NavigationHandle* navigation_handle) {
  ui::PageTransition page_transition = navigation_handle->GetPageTransition();
  if (!ui::IsValidPageTransitionType(page_transition) ||
      ui::PageTransitionIsRedirect(page_transition) ||
      !ui::PageTransitionIsMainFrame(page_transition)) {
    return;
  }

  SavedTabGroup* group = model_->GetGroupContainingTab(token_);
  if (!group) {
    return;
  }

  SavedTabGroupTab* tab = group->GetTab(token_);
  tab->SetTitle(web_contents_->GetTitle());
  tab->SetURL(web_contents_->GetURL());
  tab->SetFavicon(favicon::TabFaviconFromWebContents(web_contents_));
  model_->UpdateTabInGroup(group->saved_guid(), *tab);
}

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

base::Token SavedTabGroupBrowserListener::GetOrCreateTrackedIDForWebContents(
    content::WebContents* web_contents) {
  if (web_contents_to_tab_id_map_.count(web_contents) == 0) {
    web_contents_to_tab_id_map_.try_emplace(
        web_contents, web_contents, base::Token::CreateRandom(), model_);
  }

  return web_contents_to_tab_id_map_.at(web_contents).token();
}

void SavedTabGroupBrowserListener::StopTrackingWebContents(
    content::WebContents* web_contents) {
  CHECK(web_contents_to_tab_id_map_.count(web_contents) > 0);
  web_contents_to_tab_id_map_.erase(web_contents);
}

void SavedTabGroupBrowserListener::OnTabGroupChanged(
    const TabGroupChange& change) {
  const TabStripModel* tab_strip_model = change.model;
  if (!model_->Contains(change.group))
    return;
  const TabGroup* group =
      tab_strip_model->group_model()->GetTabGroup(change.group);
  switch (change.type) {
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
    case TabGroupChange::kContentsChanged:
    case TabGroupChange::kCreated:
    case TabGroupChange::kEditorOpened:
    case TabGroupChange::kMoved: {
      NOTIMPLEMENTED();
      return;
    }
  }
}

void SavedTabGroupBrowserListener::TabGroupedStateChanged(
    absl::optional<tab_groups::TabGroupId> new_local_group_id,
    content::WebContents* contents,
    int index) {
  // If the webcontents is already saved then its moving saved groups.
  if (web_contents_to_tab_id_map_.count(contents) > 0) {
    // Remove the tab from it's old group.
    base::Token local_tab_id = web_contents_to_tab_id_map_.at(contents).token();
    SavedTabGroup* old_group = model_->GetGroupContainingTab(local_tab_id);

    // If the tab is tracked by has no old local group, then it is being created
    // via AddTabToGroupForRestore, and does not need to update it's membership
    // in any saved groups/tabs
    if (new_local_group_id && !old_group->local_group_id()) {
      return;
    }

    // if its already in the correct group then this tab was already restored.
    // Remove the tab from it's old group.
    SavedTabGroupTab* tab = old_group->GetTab(local_tab_id);
    model_->RemoveTabFromGroup(old_group->saved_guid(), tab->saved_tab_guid());

    // Remove the tab from the mapping.
    web_contents_to_tab_id_map_.erase(contents);
  }

  // If there's no new group then there's nothing to do since we've already
  // removed from the old SavedTabGroup if the tab was saved.
  if (!new_local_group_id.has_value())
    return;

  // If the group is not currently saved then there is nothing to do.
  SavedTabGroup* new_saved_group = model_->Get(new_local_group_id.value());
  if (new_saved_group == nullptr)
    return;

  absl::optional<int> first_tab_in_group_index_in_tabstrip =
      browser_->tab_strip_model()
          ->group_model()
          ->GetTabGroup(new_saved_group->local_group_id().value())
          ->GetFirstTab();
  DCHECK(first_tab_in_group_index_in_tabstrip.has_value());

  int relative_index_of_tab_in_group =
      browser_->tab_strip_model()->GetIndexOfWebContents(contents) -
      first_tab_in_group_index_in_tabstrip.value();

  SavedTabGroupTab tab =
      SavedTabGroupUtils::CreateSavedTabGroupTabFromWebContents(
          contents, new_saved_group->saved_guid());

  // Add the token for mapping the local web contents to the SavedTabGroupTab.
  base::Token token = base::Token::CreateRandom();
  tab.SetLocalTabID(token);

  // Create a SavedTabGroupTab for the contents and store.
  model_->AddTabToGroup(new_saved_group->saved_guid(), std::move(tab),
                        relative_index_of_tab_in_group);

  // save the contents in the mapping
  web_contents_to_tab_id_map_.try_emplace(contents, contents, token, model_);
}

void SavedTabGroupBrowserListener::WillCloseAllTabs(
    TabStripModel* tab_strip_model) {
  CHECK(tab_strip_model);
  CHECK(saved_tab_group_model());

  if (!tab_strip_model->group_model()) {
    return;
  }

  const std::vector<tab_groups::TabGroupId>& groups =
      tab_strip_model->group_model()->ListTabGroups();

  // Stop tracking web contents changes for groups which are saved and about to
  // be removed.
  for (const tab_groups::TabGroupId& group : groups) {
    if (saved_tab_group_model()->Contains(group)) {
      TabGroup* tab_group = tab_strip_model->group_model()->GetTabGroup(group);
      CHECK(tab_group);

      // Stop listening to all of the webcontents in the group.
      const gfx::Range tab_range = tab_group->ListTabs();
      for (auto i = tab_range.start(); i < tab_range.end(); ++i) {
        content::WebContents* web_contents =
            tab_strip_model->GetWebContentsAt(i);
        StopTrackingWebContents(web_contents);
      }
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

Browser* SavedTabGroupModelListener::GetBrowserWithTabGroupId(
    tab_groups::TabGroupId group_id) {
  auto it = base::ranges::find_if(
      observed_browser_listeners_, [group_id](const auto& listener_pair) {
        return listener_pair.second.ContainsTabGroup(group_id);
      });
  return it != observed_browser_listeners_.end() ? it->second.browser()
                                                 : nullptr;
}

TabStripModel* SavedTabGroupModelListener::GetTabStripModelWithTabGroupId(
    tab_groups::TabGroupId group_id) {
  Browser* browser = GetBrowserWithTabGroupId(group_id);
  return browser ? browser->tab_strip_model() : nullptr;
}

base::Token SavedTabGroupModelListener::GetOrCreateTrackedIDForWebContents(
    Browser* browser,
    content::WebContents* web_contents) {
  CHECK(observed_browser_listeners_.count(browser) > 0);
  return observed_browser_listeners_.at(browser)
      .GetOrCreateTrackedIDForWebContents(web_contents);
}

void SavedTabGroupModelListener::StopTrackingWebContents(
    Browser* browser,
    content::WebContents* web_contents) {
  CHECK(observed_browser_listeners_.count(browser) > 0);
  observed_browser_listeners_.at(browser).StopTrackingWebContents(web_contents);
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
