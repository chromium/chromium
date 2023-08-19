// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync_preferences/syncable_prefs_database.h"

#include "base/logging.h"
#include "build/chromeos_buildflags.h"

namespace sync_preferences {

bool SyncablePrefsDatabase::IsPreferenceSyncable(
    const std::string& pref_name) const {
  return GetSyncablePrefMetadata(pref_name).has_value();
}

}  // namespace sync_preferences
