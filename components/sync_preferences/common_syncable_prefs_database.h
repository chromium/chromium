// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_PREFERENCES_COMMON_SYNCABLE_PREFS_DATABASE_H_
#define COMPONENTS_SYNC_PREFERENCES_COMMON_SYNCABLE_PREFS_DATABASE_H_

#include "components/sync_preferences/syncable_prefs_database.h"

namespace sync_preferences {

// This class provides an implementation for SyncablePrefsDatabase for common
// syncable preferences, i.e. preferences which are shared between all
// platforms.
class CommonSyncablePrefsDatabase : public SyncablePrefsDatabase {
 public:
  // Returns true if `pref_name` is part of the common syncable preferences
  // allowlist.
  bool IsPreferenceSyncable(const std::string& pref_name) const override;
};

}  // namespace sync_preferences

#endif  // COMPONENTS_SYNC_PREFERENCES_COMMON_SYNCABLE_PREFS_DATABASE_H_
