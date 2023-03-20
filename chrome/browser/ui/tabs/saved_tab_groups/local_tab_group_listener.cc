// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/tabs/saved_tab_groups/local_tab_group_listener.h"

#include "base/token.h"
#include "chrome/browser/ui/tabs/saved_tab_groups/saved_tab_group_utils.h"
#include "chrome/browser/ui/tabs/tab_group.h"
#include "chrome/browser/ui/tabs/tab_group_model.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "components/saved_tab_groups/saved_tab_group_model.h"

LocalTabGroupListener::LocalTabGroupListener(
    const tab_groups::TabGroupId local_id,
    const base::GUID saved_guid,
    SavedTabGroupModel* const model,
    std::vector<std::pair<content::WebContents*, base::GUID>> mapping)
    : model_(model), local_id_(local_id), saved_guid_(saved_guid) {
  for (const auto& [contents, saved_tab_guid] : mapping) {
    const base::Token local_tab_id = base::Token::CreateRandom();

    web_contents_to_tab_id_map_.try_emplace(contents, contents, local_tab_id,
                                            model_);

    SavedTabGroupTab tab(*saved_group()->GetTab(saved_tab_guid));
    tab.SetLocalTabID(local_tab_id);
    model_->ReplaceTabInGroupAt(saved_group()->saved_guid(), saved_tab_guid,
                                std::move(tab));
  }
}

LocalTabGroupListener::~LocalTabGroupListener() = default;

void LocalTabGroupListener::AddWebContents(content::WebContents* web_contents,
                                           TabStripModel* tab_strip_model,
                                           int index) {
  CHECK(model_->Contains(saved_guid_));
  CHECK(tab_strip_model->group_model()->ContainsTabGroup(local_id_));

  const absl::optional<int> first_tab_in_group_index_in_tabstrip =
      tab_strip_model->group_model()->GetTabGroup(local_id_)->GetFirstTab();
  CHECK(first_tab_in_group_index_in_tabstrip.has_value());

  const int relative_index_of_tab_in_group =
      tab_strip_model->GetIndexOfWebContents(web_contents) -
      first_tab_in_group_index_in_tabstrip.value();

  base::Token token = base::Token::CreateRandom();

  // Create a new SavedTabGroupTab linked to `token`.
  SavedTabGroupTab tab =
      SavedTabGroupUtils::CreateSavedTabGroupTabFromWebContents(web_contents,
                                                                saved_guid_);
  tab.SetLocalTabID(token);
  model_->AddTabToGroup(saved_guid_, std::move(tab),
                        relative_index_of_tab_in_group);

  // Link `web_contents` to `token`.
  web_contents_to_tab_id_map_.try_emplace(web_contents, web_contents, token,
                                          model_);
}

void LocalTabGroupListener::RemoveWebContentsIfPresent(
    content::WebContents* web_contents) {
  if (web_contents_to_tab_id_map_.count(web_contents) == 0) {
    return;
  }

  const base::Token tab_id =
      web_contents_to_tab_id_map_.at(web_contents).token();
  const base::GUID tab_guid = saved_group()->GetTab(tab_id)->saved_tab_guid();

  web_contents_to_tab_id_map_.erase(web_contents);
  model_->RemoveTabFromGroup(saved_guid_, tab_guid);
}
