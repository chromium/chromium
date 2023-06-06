// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync_preferences/syncable_prefs_database.h"

#include "base/logging.h"
#include "build/chromeos_buildflags.h"

namespace sync_preferences {

SyncablePrefMetadata::SyncablePrefMetadata(int syncable_pref_id,
                                           syncer::ModelType model_type,
                                           bool is_history_opt_in_required)
    : syncable_pref_id_(syncable_pref_id),
      model_type_(model_type),
      is_history_opt_in_required_(is_history_opt_in_required) {
  // TODO(crbug.com/1424774): Allow OS_* types only if IS_CHROMEOS_ASH is true.
  // This isn't the case now because of an outlier entry in
  // common_syncable_prefs_database.
  DCHECK(model_type_ == syncer::PREFERENCES ||
         model_type_ == syncer::PRIORITY_PREFERENCES ||
         model_type_ == syncer::OS_PREFERENCES ||
         model_type_ == syncer::OS_PRIORITY_PREFERENCES)
      << "Invalid type " << model_type_
      << " for syncable pref with id=" << syncable_pref_id_;
}

bool SyncablePrefsDatabase::IsPreferenceSyncable(
    const std::string& pref_name) const {
  return GetSyncablePrefMetadata(pref_name).has_value();
}

}  // namespace sync_preferences
