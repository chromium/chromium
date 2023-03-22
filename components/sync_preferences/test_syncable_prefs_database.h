// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_PREFERENCES_TEST_SYNCABLE_PREFS_DATABASE_H_
#define COMPONENTS_SYNC_PREFERENCES_TEST_SYNCABLE_PREFS_DATABASE_H_

#include <string>
#include <unordered_map>

#include "components/sync_preferences/syncable_prefs_database.h"

namespace sync_preferences {

class TestSyncablePrefsDatabase : public SyncablePrefsDatabase {
 public:
  explicit TestSyncablePrefsDatabase(
      const std::unordered_map<std::string,
                               sync_preferences::SyncablePrefMetadata>&
          syncable_prefs_map);
  ~TestSyncablePrefsDatabase() override;

  absl::optional<sync_preferences::SyncablePrefMetadata>
  GetSyncablePrefMetadata(const std::string& pref_name) const override;

 private:
  std::unordered_map<std::string, sync_preferences::SyncablePrefMetadata>
      syncable_prefs_map_;
};

}  // namespace sync_preferences

#endif  // COMPONENTS_SYNC_PREFERENCES_TEST_SYNCABLE_PREFS_DATABASE_H_
