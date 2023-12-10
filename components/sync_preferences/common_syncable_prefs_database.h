// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_PREFERENCES_COMMON_SYNCABLE_PREFS_DATABASE_H_
#define COMPONENTS_SYNC_PREFERENCES_COMMON_SYNCABLE_PREFS_DATABASE_H_

#include <map>

#include "base/strings/string_piece.h"
#include "components/sync_preferences/syncable_prefs_database.h"

namespace sync_preferences {

// Should be used by tests.
extern const char kSyncablePrefForTesting[];
extern const char kSyncableMergeableDictPrefForTesting[];

// This class provides an implementation for SyncablePrefsDatabase for common
// syncable preferences, i.e. preferences which are shared between all
// platforms.
class CommonSyncablePrefsDatabase : public SyncablePrefsDatabase {
 public:
  // Returns the metadata associated to the pref or null if `pref_name` is not
  // syncable.
  absl::optional<SyncablePrefMetadata> GetSyncablePrefMetadata(
      const std::string& pref_name) const override;

  std::map<base::StringPiece, SyncablePrefMetadata> GetAllSyncablePrefsForTest()
      const;
};

}  // namespace sync_preferences

#endif  // COMPONENTS_SYNC_PREFERENCES_COMMON_SYNCABLE_PREFS_DATABASE_H_
