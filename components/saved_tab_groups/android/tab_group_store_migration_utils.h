// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SAVED_TAB_GROUPS_ANDROID_TAB_GROUP_STORE_MIGRATION_UTILS_H_
#define COMPONENTS_SAVED_TAB_GROUPS_ANDROID_TAB_GROUP_STORE_MIGRATION_UTILS_H_

#include <map>

#include "base/uuid.h"
#include "components/saved_tab_groups/types.h"

namespace tab_groups {

// For migration. Invoked on startup, which reads the stored tab group ID
// mappings from Android SharedPreferences and then clears it out.
std::map<base::Uuid, LocalTabGroupID>
ReadAndClearIdMappingsForMigrationFromSharedPrefs();

// For testing only.
void WriteMappingToSharedPrefsForTesting(const base::Uuid& sync_id,
                                         const LocalTabGroupID& local_id);
void ClearSharedPrefsForTesting();

}  // namespace tab_groups

#endif  // COMPONENTS_SAVED_TAB_GROUPS_ANDROID_TAB_GROUP_STORE_MIGRATION_UTILS_H_
