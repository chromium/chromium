// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_PREFERENCES_COMMON_SYNCABLE_PREFS_DATABASE_H_
#define COMPONENTS_SYNC_PREFERENCES_COMMON_SYNCABLE_PREFS_DATABASE_H_

#include <map>
#include <string_view>

#include "components/sync_preferences/syncable_prefs_database.h"

namespace sync_preferences {

// Should be used by tests.
inline constexpr char kSyncablePrefForTesting[] = "syncable-test-preference";
inline constexpr char kSyncableMergeableDictPrefForTesting[] =
    "syncable-mergeable-dict-test-preference";
inline constexpr char kSyncableMergeableListPrefForTesting[] =
    "syncable-mergeable-list-test-preference";
inline constexpr char kSyncableHistorySensitiveListPrefForTesting[] =
    "syncable-history-sensitive-list-test-preference";

// This class provides an implementation for SyncablePrefsDatabase for common
// syncable preferences, i.e. preferences which are shared between all
// platforms.
class CommonSyncablePrefsDatabase : public SyncablePrefsDatabase {
 public:
  // Returns the metadata associated to the pref or null if `pref_name` is not
  // syncable.
  std::optional<SyncablePrefMetadata> GetSyncablePrefMetadata(
      std::string_view pref_name) const override;

  std::map<std::string_view, SyncablePrefMetadata> GetAllSyncablePrefsForTest()
      const;
};

}  // namespace sync_preferences

#endif  // COMPONENTS_SYNC_PREFERENCES_COMMON_SYNCABLE_PREFS_DATABASE_H_
