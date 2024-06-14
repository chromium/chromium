// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SAVED_TAB_GROUPS_UTILS_H_
#define COMPONENTS_SAVED_TAB_GROUPS_UTILS_H_

#include <optional>
#include <string>

#include "components/saved_tab_groups/types.h"

namespace tab_groups {

// Whether the local IDs are persisted, which is true for Android / iOS, but
// false in desktop.
bool AreLocalIdsPersisted();

// Serialization methods for LocalTabGroupID.
std::string LocalTabGroupIDToString(const LocalTabGroupID& local_tab_group_id);
std::optional<LocalTabGroupID> LocalTabGroupIDFromString(
    const std::string& local_tab_group_id);

}  // namespace tab_groups

#endif  // COMPONENTS_SAVED_TAB_GROUPS_UTILS_H_
