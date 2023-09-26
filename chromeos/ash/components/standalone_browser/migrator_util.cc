// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/standalone_browser/migrator_util.h"

#include <string_view>

#include "base/logging.h"
#include "base/values.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/scoped_user_pref_update.h"

namespace ash::standalone_browser::migrator_util {
namespace {
// Local state pref name to keep track of the number of previous migration
// attempts. It is a dictionary of the form `{<user_id_hash>: <count>}`.
constexpr char kMigrationAttemptCountPref[] =
    "ash.browser_data_migrator.migration_attempt_count";

}  // namespace

void RegisterLocalStatePrefs(PrefRegistrySimple* registry) {
  registry->RegisterDictionaryPref(kMigrationAttemptCountPref);
}

bool IsMigrationAttemptLimitReachedForUser(PrefService* local_state,
                                           std::string_view user_id_hash) {
  const int attempts =
      GetMigrationAttemptCountForUser(local_state, user_id_hash);
  LOG_IF(WARNING, attempts > 0)
      << "The number of previous of migration attemps = " << attempts;

  return attempts >= kMaxMigrationAttemptCount;
}

int GetMigrationAttemptCountForUser(PrefService* local_state,
                                    std::string_view user_id_hash) {
  return local_state->GetDict(kMigrationAttemptCountPref)
      .FindInt(user_id_hash)
      .value_or(0);
}

void UpdateMigrationAttemptCountForUser(PrefService* local_state,
                                        std::string_view user_id_hash) {
  int count = GetMigrationAttemptCountForUser(local_state, user_id_hash);
  count += 1;
  ScopedDictPrefUpdate update(local_state, kMigrationAttemptCountPref);
  base::Value::Dict& dict = update.Get();
  dict.Set(user_id_hash, count);
}

void ClearMigrationAttemptCountForUser(PrefService* local_state,
                                       std::string_view user_id_hash) {
  ScopedDictPrefUpdate update(local_state, kMigrationAttemptCountPref);
  base::Value::Dict& dict = update.Get();
  dict.Remove(user_id_hash);
}

}  // namespace ash::standalone_browser::migrator_util
