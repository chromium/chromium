// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync_preferences/syncable_prefs_database.h"

namespace sync_preferences {

SyncablePrefMetadata::SyncablePrefMetadata(int syncable_pref_id,
                                           syncer::ModelType model_type)
    : syncable_pref_id_(syncable_pref_id), model_type_(model_type) {}

bool SyncablePrefsDatabase::IsPreferenceSyncable(
    const std::string& pref_name) const {
  return GetSyncablePrefMetadata(pref_name).has_value();
}

}  // namespace sync_preferences
