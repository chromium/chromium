// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SAVED_TAB_GROUPS_PUBLIC_UTILS_H_
#define COMPONENTS_SAVED_TAB_GROUPS_PUBLIC_UTILS_H_

#include <optional>
#include <string>

#include "components/saved_tab_groups/public/types.h"
#include "url/gurl.h"

namespace tab_groups {

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

}  // namespace tab_groups

#endif  // COMPONENTS_SAVED_TAB_GROUPS_PUBLIC_UTILS_H_
