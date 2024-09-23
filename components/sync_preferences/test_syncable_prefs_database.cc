// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync_preferences/test_syncable_prefs_database.h"

#include <string_view>

namespace sync_preferences {

TestSyncablePrefsDatabase::TestSyncablePrefsDatabase(
    const PrefsMap& syncable_prefs_map)
    : syncable_prefs_map_(syncable_prefs_map) {}

TestSyncablePrefsDatabase::~TestSyncablePrefsDatabase() = default;

std::optional<sync_preferences::SyncablePrefMetadata>
TestSyncablePrefsDatabase::GetSyncablePrefMetadata(
    std::string_view pref_name) const {
  if (auto it = syncable_prefs_map_.find(pref_name);
      it != syncable_prefs_map_.end()) {
    return it->second;
  }
  return std::nullopt;
}

}  // namespace sync_preferences
