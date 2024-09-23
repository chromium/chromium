// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync_preferences/syncable_prefs_database.h"

#include <string_view>

#include "base/logging.h"
#include "build/chromeos_buildflags.h"

namespace sync_preferences {

bool SyncablePrefsDatabase::IsPreferenceSyncable(
    std::string_view pref_name) const {
  return GetSyncablePrefMetadata(pref_name).has_value();
}

bool SyncablePrefsDatabase::IsPreferenceMergeable(
    std::string_view pref_name) const {
  std::optional<SyncablePrefMetadata> metadata =
      GetSyncablePrefMetadata(pref_name);
  CHECK(metadata.has_value());
  return metadata->merge_behavior() != MergeBehavior::kNone;
}

}  // namespace sync_preferences
