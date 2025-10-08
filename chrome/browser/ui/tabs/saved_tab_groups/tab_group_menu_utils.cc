// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/tabs/saved_tab_groups/tab_group_menu_utils.h"

#include "base/strings/utf_string_conversions.h"
#include "chrome/grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"

namespace tab_groups {

TabGroupMenuAction::TabGroupMenuAction(Type type,
                                       std::variant<base::Uuid, GURL> element)
    : type(type), element(element) {}
TabGroupMenuAction::TabGroupMenuAction(const TabGroupMenuAction& action) =
    default;
TabGroupMenuAction::~TabGroupMenuAction() = default;

std::u16string TabGroupMenuUtils::GetMenuTextForGroup(
    const tab_groups::SavedTabGroup& group) {
  return group.title().empty()
             ? l10n_util::GetPluralStringFUTF16(
                   IDS_SAVED_TAB_GROUP_TABS_COUNT,
                   static_cast<int>(group.saved_tabs().size()))
             : group.title();
}

std::u16string TabGroupMenuUtils::GetMenuTextForTab(
    const tab_groups::SavedTabGroupTab& tab) {
  return tab.title().empty() ? base::UTF8ToUTF16(tab.url().spec())
                             : tab.title();
}

std::vector<base::Uuid>
TabGroupMenuUtils::GetGroupsForDisplaySortedByCreationTime(
    TabGroupSyncService* tab_group_service) {
  CHECK(tab_group_service);
  std::vector<const SavedTabGroup*> groups;

  std::vector<SavedTabGroup> all_groups = tab_group_service->GetAllGroups();
  for (const SavedTabGroup& group : all_groups) {
    if (group.saved_tabs().empty()) {
      continue;
    }
    groups.push_back(&group);
  }

  std::sort(groups.begin(), groups.end(),
            [](const SavedTabGroup* a, const SavedTabGroup* b) {
              return a->creation_time() > b->creation_time();
            });

  std::vector<base::Uuid> sorted_guids;
  for (const SavedTabGroup* group : groups) {
    sorted_guids.push_back(group->saved_guid());
  }

  return sorted_guids;
}

}  // namespace tab_groups
