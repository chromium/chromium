// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SAVED_TAB_GROUPS_PUBLIC_UTILS_H_
#define COMPONENTS_SAVED_TAB_GROUPS_PUBLIC_UTILS_H_

#include <optional>
#include <string>

#include "components/saved_tab_groups/public/saved_tab_group.h"
#include "components/saved_tab_groups/public/types.h"
#include "components/sync/base/collaboration_id.h"
#include "url/gurl.h"

class PrefService;

namespace tab_groups {

extern const char kChromeSavedTabGroupUnsupportedURL[];

// Whether the local IDs are persisted, which is true for Android / iOS, but
// false in desktop.
bool AreLocalIdsPersisted();

// Serialization methods for LocalTabGroupID.
std::string LocalTabGroupIDToString(const LocalTabGroupID& local_tab_group_id);
std::optional<LocalTabGroupID> LocalTabGroupIDFromString(
    const std::string& local_tab_group_id);

// Returns whether the tab's URL is viable for saving in a saved tab
// group.
bool IsURLValidForSavedTabGroups(const GURL& gurl);

// Returns a default URL and default title. Should be invoked when
// IsURLValidForSavedTabGroups() returns false.
std::pair<GURL, std::u16string> GetDefaultUrlAndTitle();

// Returns a title for display for a given URL. If the site had provided
// a title for the URL, it may be controlled by attacker and thus cannot
// be always trusted,.
std::u16string GetTitleFromUrlForDisplay(const GURL& url);

std::string TabGroupToShortLogString(const std::string_view& prefix,
                                     const SavedTabGroup* group);

std::string TabGroupIdsToShortLogString(
    const std::string_view& prefix,
    base::Uuid group_id,
    const std::optional<syncer::CollaborationId> collaboration_id);

// Returns whether SavedTabGroup's pinned_position has been migrated to
// projects_position.
bool IsTabGroupPinnedPositionToProjectsPositionMigrated(
    PrefService* pref_service);

// Records the migration of SavedTabGroup's pinned_position to
// projects_position.
void SetTabGroupPinnedPositionToProjectsPositionMigrated(
    PrefService* pref_service);

}  // namespace tab_groups

#endif  // COMPONENTS_SAVED_TAB_GROUPS_PUBLIC_UTILS_H_
