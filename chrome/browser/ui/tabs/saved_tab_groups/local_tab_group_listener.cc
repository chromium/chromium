// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/tabs/saved_tab_groups/local_tab_group_listener.h"

#include <unordered_map>

#include "base/token.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/saved_tab_groups/saved_tab_group_utils.h"
#include "chrome/browser/ui/tabs/saved_tab_groups/saved_tab_group_web_contents_listener.h"
#include "chrome/browser/ui/tabs/tab_group.h"
#include "chrome/browser/ui/tabs/tab_group_model.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/tabs/tab_strip_model_observer.h"
#include "components/saved_tab_groups/saved_tab_group_model.h"

namespace content {
class WebContents;
}

LocalTabGroupListener::LocalTabGroupListener(
    const tab_groups::TabGroupId local_id,
    const base::Uuid saved_guid,
    SavedTabGroupModel* const model,
    std::vector<std::pair<content::WebContents*, base::Uuid>> mapping)
    : model_(model), local_id_(local_id), saved_guid_(saved_guid) {
  for (const auto& [contents, saved_tab_guid] : mapping) {
    const base::Token local_tab_id = base::Token::CreateRandom();

    web_contents_to_tab_id_map_.try_emplace(contents, contents, local_tab_id,
                                            model_);

    const SavedTabGroupTab tab(*saved_group()->GetTab(saved_tab_guid));
    model_->UpdateLocalTabId(saved_group()->saved_guid(), tab, local_tab_id);
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
  // The saved tabs and the local tabs should match up in the same order.
  const std::vector<SavedTabGroupTab>& saved_tabs =
      saved_group() ? saved_group()->saved_tabs()
                    : std::vector<SavedTabGroupTab>();
  const std::vector<content::WebContents*> local_tabs =
      SavedTabGroupUtils::GetWebContentsesInGroup(local_id_);

  CHECK_EQ(saved_tabs.size(), local_tabs.size());
  for (size_t i = 0; i < saved_tabs.size(); ++i) {
    const SavedTabGroupTab& saved_tab = saved_tabs[i];
    content::WebContents* const local_tab = local_tabs[i];

    const auto map_entry = web_contents_to_tab_id_map_.find(local_tab);
    CHECK(map_entry != web_contents_to_tab_id_map_.end());

    const SavedTabGroupWebContentsListener& listener = map_entry->second;
    CHECK_EQ(saved_tab.local_tab_id().value(), listener.token());
  }
}

void LocalTabGroupListener::AddWebContentsFromLocal(
    content::WebContents* web_contents,
    TabStripModel* tab_strip_model,
    int index) {
  if (paused_) {
    return;
  }

  CHECK(model_->Contains(saved_guid_));
  CHECK(tab_strip_model->group_model()->ContainsTabGroup(local_id_));

  const absl::optional<int> tabstrip_index_of_first_tab_in_group =
      tab_strip_model->group_model()->GetTabGroup(local_id_)->GetFirstTab();
  CHECK(tabstrip_index_of_first_tab_in_group.has_value());

  const int relative_index_of_tab_in_group =
      tab_strip_model->GetIndexOfWebContents(web_contents) -
      tabstrip_index_of_first_tab_in_group.value();

  base::Token token = base::Token::CreateRandom();

  // Create a new SavedTabGroupTab linked to `token`.
  SavedTabGroupTab tab =
      SavedTabGroupUtils::CreateSavedTabGroupTabFromWebContents(web_contents,
                                                                saved_guid_);
  tab.SetLocalTabID(token);
  tab.SetPosition(relative_index_of_tab_in_group);
  model_->AddTabToGroupLocally(saved_guid_, std::move(tab));

  // Link `web_contents` to `token`.
  web_contents_to_tab_id_map_.try_emplace(web_contents, web_contents, token,
                                          model_);
}

void LocalTabGroupListener::MoveWebContentsFromLocal(
    TabStripModel* tab_strip_model,
    content::WebContents* web_contents,
    int tabstrip_index_of_moved_tab) {
  // Ex:        0 1   2 3 4
  //  TabStrip: A B [ C D E ]
  //  TabGroup:       0 1 2
  // C represents the first tab in the group. For the tabstrip this means C is
  // at index 2. For the tab group, C is at index 0.
  // Moving C to index 4 in the tabstrip means it will now have an index of 2 in
  // the tab group and SavedTabGroupModel.
  const absl::optional<int> tabstrip_index_of_first_tab_in_group =
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
      web_contents_to_tab_id_map_.at(web_contents);
  const base::Uuid& saved_tab_guid =
      saved_group()->GetTab(web_contents_listener.token())->saved_tab_guid();

  model_->MoveTabInGroupTo(saved_guid_, saved_tab_guid, index_in_group);
}

LocalTabGroupListener::Liveness
LocalTabGroupListener::MaybeRemoveWebContentsFromLocal(
    content::WebContents* web_contents) {
  if (paused_) {
    return Liveness::kGroupExists;
  }

  if (web_contents_to_tab_id_map_.count(web_contents) == 0) {
    return Liveness::kGroupExists;
  }

  const base::Token tab_id =
      web_contents_to_tab_id_map_.at(web_contents).token();
  const base::Uuid tab_guid = saved_group()->GetTab(tab_id)->saved_tab_guid();

  web_contents_to_tab_id_map_.erase(web_contents);
  model_->RemoveTabFromGroupLocally(saved_guid_, tab_guid);

  return model_->Contains(saved_guid_) ? Liveness::kGroupExists
                                       : Liveness::kGroupDeleted;
}

void LocalTabGroupListener::GroupRemovedFromSync() {
  PauseTracking();

  // Remove every currently tracked tab; this will also close the local group.
  std::vector<content::WebContents*> contentses;
  for (auto& [contents, listener] : web_contents_to_tab_id_map_) {
    contentses.push_back(contents);
  }
  for (content::WebContents* const contents : contentses) {
    RemoveWebContentsFromSync(contents);
  }

  ResumeTracking();
}

LocalTabGroupListener::Liveness LocalTabGroupListener::UpdateFromSync() {
  PauseTracking();

  RemoveLocalWebContentsNotInSavedGroup();

  const SavedTabGroup* const saved_group = model_->Get(saved_guid_);
  TabStripModel* const tab_strip_model =
      SavedTabGroupUtils::GetBrowserWithTabGroupId(local_id_)
          ->tab_strip_model();

  std::unordered_map<base::Token, content::WebContents*, base::TokenHash>
      token_to_contents_map;
  for (auto& [contents, listener] : web_contents_to_tab_id_map_) {
    token_to_contents_map[listener.token()] = contents;
  }

  // Add, navigate, and reorder local tabs to match saved tabs.
  const gfx::Range group_index_range =
      tab_strip_model->group_model()->GetTabGroup(local_id_)->ListTabs();
  CHECK_LE(group_index_range.length(), saved_group->saved_tabs().size());

  // Parallel iterate over saved tabs and local indices. For each saved tab and
  // index, ensure the corresponding local tab is at that index and in the
  // correct state.
  int next_index_in_tab_strip = group_index_range.start();
  for (const SavedTabGroupTab& saved_tab : saved_group->saved_tabs()) {
    content::WebContents* const contents =
        saved_tab.local_tab_id().has_value()
            ? token_to_contents_map[saved_tab.local_tab_id().value()]
            : nullptr;
    MatchLocalTabToSavedTab(saved_tab, contents, tab_strip_model,
                            next_index_in_tab_strip);
    next_index_in_tab_strip++;
  }

  ResumeTracking();

  return model_->Contains(saved_guid_) ? Liveness::kGroupExists
                                       : Liveness::kGroupDeleted;
}

void LocalTabGroupListener::MatchLocalTabToSavedTab(
    SavedTabGroupTab saved_tab,
    content::WebContents* local_tab,
    TabStripModel* tab_strip_model,
    int target_index_in_tab_strip) {
  if (saved_tab.local_tab_id().has_value()) {
    CHECK(local_tab);
    // Reorder if needed. This approach corresponds to selection sort.
    // N.B.: this approach will do N reorders for a tab that was moved N spots
    // to the left.
    const int current_index = tab_strip_model->GetIndexOfWebContents(local_tab);
    CHECK_EQ(local_id_,
             tab_strip_model->GetTabGroupForTab(current_index).value());
    tab_strip_model->MoveWebContentsAt(current_index, target_index_in_tab_strip,
                                       false);

    // Navigate if needed.
    if (saved_tab.url() != local_tab->GetURL()) {
      web_contents_to_tab_id_map_.at(local_tab).NavigateToUrl(saved_tab.url());
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
  content::WebContents* opened_contents = SavedTabGroupUtils::OpenTabInBrowser(
      tab.url(), browser, browser->profile(),
      WindowOpenDisposition::NEW_BACKGROUND_TAB, index_in_tabstrip, local_id_);

  // Listen to navigations.
  base::Token token = base::Token::CreateRandom();
  model_->UpdateLocalTabId(tab.saved_group_guid(), tab, token);
  web_contents_to_tab_id_map_.try_emplace(opened_contents, opened_contents,
                                          token, model_);
}

void LocalTabGroupListener::RemoveLocalWebContentsNotInSavedGroup() {
  const SavedTabGroup* const saved_group = model_->Get(saved_guid_);
  const std::vector<content::WebContents*> web_contentses_in_local_group =
      SavedTabGroupUtils::GetWebContentsesInGroup(local_id_);
  for (content::WebContents* const contents : web_contentses_in_local_group) {
    const auto& it = web_contents_to_tab_id_map_.find(contents);
    CHECK(it != web_contents_to_tab_id_map_.end());
    if (!saved_group->ContainsTab(it->second.token())) {
      RemoveWebContentsFromSync(contents);
    }
  }
}

void LocalTabGroupListener::RemoveWebContentsFromSync(
    content::WebContents* contents) {
  web_contents_to_tab_id_map_.erase(contents);

  Browser* const browser =
      SavedTabGroupUtils::GetBrowserWithTabGroupId(local_id_);
  CHECK(browser);
  CHECK(browser->tab_strip_model());
  int model_index = browser->tab_strip_model()->GetIndexOfWebContents(contents);
  CHECK(model_index != TabStripModel::kNoTab);

  // Unload listeners can delay or prevent a tab closing. Remove the tab from
  // the group first so the local and saved groups can be consistent even if
  // this happens.
  browser->tab_strip_model()->RemoveFromGroup({model_index});

  // Removing the tab from the group may have moved the tab to maintain group
  // contiguity. Find the tab again and close it.
  model_index = browser->tab_strip_model()->GetIndexOfWebContents(contents);
  browser->tab_strip_model()->CloseWebContentsAt(
      model_index, TabCloseTypes::CLOSE_CREATE_HISTORICAL_TAB);
}
