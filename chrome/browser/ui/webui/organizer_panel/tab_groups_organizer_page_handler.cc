// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/organizer_panel/tab_groups_organizer_page_handler.h"

#include <algorithm>
#include <optional>
#include <utility>
#include <vector>

#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/tab_group_sync/tab_group_sync_service_factory.h"
#include "chrome/browser/ui/tabs/saved_tab_groups/tab_group_menu_utils.h"
#include "components/saved_tab_groups/public/saved_tab_group.h"
#include "components/saved_tab_groups/public/tab_group_sync_service.h"

namespace {

base::Time GetLastUsedTime(const tab_groups::SavedTabGroup& group) {
  if (!group.last_user_interaction_time().is_null()) {
    return group.last_user_interaction_time();
  }
  if (!group.update_time().is_null()) {
    return group.update_time();
  }
  return group.creation_time();
}

}  // namespace

TabGroupsOrganizerPageHandler::TabGroupsOrganizerPageHandler(
    mojo::PendingReceiver<organizer_panel::mojom::TabGroupsOrganizerPageHandler>
        receiver,
    Profile* profile)
    : receiver_(this, std::move(receiver)),
      tab_group_sync_service_(
          tab_groups::TabGroupSyncServiceFactory::GetForProfile(profile)) {}

TabGroupsOrganizerPageHandler::~TabGroupsOrganizerPageHandler() = default;

void TabGroupsOrganizerPageHandler::GetTabGroups(
    GetTabGroupsCallback callback) {
  std::vector<organizer_panel::mojom::TabGroupPtr> tab_groups;
  if (!tab_group_sync_service_) {
    std::move(callback).Run(std::move(tab_groups));
    return;
  }

  std::vector<tab_groups::SavedTabGroup> groups =
      tab_group_sync_service_->GetAllGroups();
  std::erase_if(groups, [](const tab_groups::SavedTabGroup& group) {
    return group.saved_tabs().empty();
  });

  std::ranges::sort(groups, [](const tab_groups::SavedTabGroup& a,
                               const tab_groups::SavedTabGroup& b) {
    return GetLastUsedTime(a) > GetLastUsedTime(b);
  });

  for (const tab_groups::SavedTabGroup& group : groups) {
    auto tab_group = organizer_panel::mojom::TabGroup::New();
    tab_group->id = group.saved_guid();
    tab_group->title = base::UTF16ToUTF8(
        tab_groups::TabGroupMenuUtils::GetMenuTextForGroup(group));
    tab_group->color = group.color();
    tab_groups.push_back(std::move(tab_group));
  }

  std::move(callback).Run(std::move(tab_groups));
}
