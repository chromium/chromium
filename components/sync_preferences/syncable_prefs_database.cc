// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync_preferences/syncable_prefs_database.h"

#include <string_view>

#include "base/feature_list.h"
#include "base/logging.h"
#include "components/sync_preferences/features.h"

namespace sync_preferences {

WriteBehavior SyncablePrefMetadata::write_behavior() const {
  CHECK(base::FeatureList::IsEnabled(features::kAccountScopedPrefs));
  return write_behavior_;
}

bool SyncablePrefsDatabase::IsPreferenceSyncable(
    std::string_view pref_name) const {
  return GetSyncablePrefMetadata(pref_name);
}

bool SyncablePrefsDatabase::IsPreferenceMergeable(
    std::string_view pref_name) const {
  const SyncablePrefMetadata* metadata = GetSyncablePrefMetadata(pref_name);
  CHECK(metadata);
  return metadata->merge_behavior() != MergeBehavior::kNone;
}

bool SyncablePrefsDatabase::IsPreferenceAlwaysSyncing(
    std::string_view pref_name) const {
  const SyncablePrefMetadata* metadata = GetSyncablePrefMetadata(pref_name);
  CHECK(metadata);
  return metadata->pref_sensitivity() ==
         PrefSensitivity::kExemptFromUserControlWhileSignedIn;
}

}  // namespace sync_preferences
