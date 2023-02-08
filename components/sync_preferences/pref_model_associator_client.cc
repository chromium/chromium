// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync_preferences/pref_model_associator_client.h"
#include "components/sync_preferences/syncable_prefs_database.h"

namespace sync_preferences {
namespace {
// TODO(crbug.com/1401271): Remove this class once GetSyncablePrefsDatabase()
// is marked as pure virtual.
class DummySyncablePrefsDatabase : public SyncablePrefsDatabase {
 public:
  bool IsPreferenceSyncable(const std::string& /*pref_name*/) const override {
    return true;
  }
};
}  // namespace

const SyncablePrefsDatabase&
PrefModelAssociatorClient::GetSyncablePrefsDatabase() const {
  static const DummySyncablePrefsDatabase syncable_prefs_database;
  return syncable_prefs_database;
}

}  // namespace sync_preferences
