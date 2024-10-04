// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/data_sharing/data_sharing_open_group_helper.h"

#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/saved_tab_groups/saved_tab_group_utils.h"
#include "chrome/browser/ui/tabs/saved_tab_groups/tab_group_action_context_desktop.h"

DataSharingOpenGroupHelper::DataSharingOpenGroupHelper(Browser* browser)
    : browser_(browser) {
  tab_group_service_ =
      tab_groups::SavedTabGroupUtils::GetServiceForProfile(browser_->profile());
  tab_group_sync_service_observation_.Observe(tab_group_service_);
}

DataSharingOpenGroupHelper::~DataSharingOpenGroupHelper() = default;

void DataSharingOpenGroupHelper::OpenTabGroupWhenAvailable(
    std::string group_id) {
  if (!OpenTabGroupIfSynced(group_id)) {
    group_ids_.insert(group_id);
  }
}

void DataSharingOpenGroupHelper::OnTabGroupAdded(
    const tab_groups::SavedTabGroup& group,
    tab_groups::TriggerSource source) {
  std::optional<std::string> collab_id = group.collaboration_id();
  if (source == tab_groups::TriggerSource::REMOTE &&
      group.is_shared_tab_group() &&
      std::find(group_ids_.begin(), group_ids_.end(), collab_id) !=
          group_ids_.end()) {
    group_ids_.erase(collab_id.value());
    tab_group_service_->OpenTabGroup(
        group.saved_guid(),
        std::make_unique<tab_groups::TabGroupActionContextDesktop>(
            browser_, tab_groups::OpeningSource::kAutoOpenedFromSync));
  }
}

bool DataSharingOpenGroupHelper::OpenTabGroupIfSynced(std::string group_id) {
  std::vector<tab_groups::SavedTabGroup> all =
      tab_group_service_->GetAllGroups();

  auto exist = [=](tab_groups::SavedTabGroup group) {
    std::optional<std::string> collab_id = group.collaboration_id();
    return collab_id && collab_id.value() == group_id;
  };

  if (auto it = std::find_if(begin(all), end(all), exist);
      it != std::end(all)) {
    tab_group_service_->OpenTabGroup(
        it->saved_guid(),
        std::make_unique<tab_groups::TabGroupActionContextDesktop>(
            browser_, tab_groups::OpeningSource::kAutoOpenedFromSync));
    return true;
  } else {
    return false;
  }
}
