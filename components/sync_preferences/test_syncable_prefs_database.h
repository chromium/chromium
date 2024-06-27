// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_PREFERENCES_TEST_SYNCABLE_PREFS_DATABASE_H_
#define COMPONENTS_SYNC_PREFERENCES_TEST_SYNCABLE_PREFS_DATABASE_H_

#include <functional>
#include <map>
#include <string>
#include <string_view>

#include "components/sync_preferences/syncable_prefs_database.h"

namespace sync_preferences {

class TestSyncablePrefsDatabase : public SyncablePrefsDatabase {
 public:
  using PrefsMap = std::
      map<std::string, sync_preferences::SyncablePrefMetadata, std::less<>>;

  explicit TestSyncablePrefsDatabase(const PrefsMap& syncable_prefs_map);
  ~TestSyncablePrefsDatabase() override;

  std::optional<sync_preferences::SyncablePrefMetadata> GetSyncablePrefMetadata(
      std::string_view pref_name) const override;

 private:
  PrefsMap syncable_prefs_map_;
};

}  // namespace sync_preferences

#endif  // COMPONENTS_SYNC_PREFERENCES_TEST_SYNCABLE_PREFS_DATABASE_H_
