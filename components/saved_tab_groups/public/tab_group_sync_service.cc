// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/saved_tab_groups/public/tab_group_sync_service.h"

namespace tab_groups {

SelectedTabInfo::SelectedTabInfo() = default;

SelectedTabInfo::SelectedTabInfo(const std::optional<base::Uuid>& tab_group_id,
                                 const std::optional<base::Uuid>& tab_id,
                                 const std::optional<std::u16string>& tab_title)
    : tab_group_id(tab_group_id), tab_id(tab_id), tab_title(tab_title) {}

SelectedTabInfo::SelectedTabInfo(const SelectedTabInfo&) = default;

SelectedTabInfo& SelectedTabInfo::operator=(const SelectedTabInfo&) = default;

SelectedTabInfo::~SelectedTabInfo() = default;

bool SelectedTabInfo::operator==(const SelectedTabInfo& other) const {
  return tab_group_id == other.tab_group_id && tab_id == other.tab_id &&
         tab_title == other.tab_title;
}

CollaborationFinder* TabGroupSyncService::GetCollaborationFinderForTesting() {
  return nullptr;
}

std::set<LocalTabID> TabGroupSyncService::GetSelectedTabs() {
  return std::set<LocalTabID>();
}

std::u16string TabGroupSyncService::GetTabTitle(
    const LocalTabID& local_tab_id) {
  return std::u16string();
}

}  // namespace tab_groups
