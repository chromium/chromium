// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync_preferences/syncable_prefs_database.h"

#include <string_view>

#include "base/feature_list.h"
#include "base/logging.h"
#include "components/sync/base/features.h"
#include "components/sync_preferences/features.h"

namespace sync_preferences {

WriteBehavior SyncablePrefMetadata::write_behavior() const {
  CHECK(base::FeatureList::IsEnabled(features::kAccountScopedPrefs));
  return write_behavior_;
}

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

bool SyncablePrefsDatabase::IsPreferenceAlwaysSyncing(
    std::string_view pref_name) const {
  CHECK(base::FeatureList::IsEnabled(
      syncer::kSyncSupportAlwaysSyncingPriorityPreferences));
  std::optional<SyncablePrefMetadata> metadata =
      GetSyncablePrefMetadata(pref_name);
  CHECK(metadata.has_value());
  return metadata->pref_sensitivity() ==
         PrefSensitivity::kExemptFromUserControlWhileSignedIn;
}

}  // namespace sync_preferences
