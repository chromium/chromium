// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/standalone_browser/migrator_util.h"

#include <string>
#include <string_view>

#include "base/check.h"
#include "base/logging.h"
#include "base/values.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/scoped_user_pref_update.h"

namespace ash::standalone_browser::migrator_util {
namespace {

std::optional<bool> g_profile_migration_completed_for_test;

// Marks the Chrome version at which profile migration was completed.
constexpr char kDataVerPref[] = "lacros.data_version";

// Local state pref name to keep track of the number of previous migration
// attempts. It is a dictionary of the form `{<user_id_hash>: <count>}`.
constexpr char kMigrationAttemptCountPref[] =
    "ash.browser_data_migrator.migration_attempt_count";

constexpr char kProfileMigrationCompletedForUserPref[] =
    "lacros.profile_migration_completed_for_user";
constexpr char kProfileMoveMigrationCompletedForUserPref[] =
    "lacros.profile_move_migration_completed_for_user";
constexpr char kProfileMigrationCompletedForNewUserPref[] =
    "lacros.profile_migration_completed_for_new_user";

// Checks if the user completed profile migration with the `MigrationMode`.
bool IsMigrationCompletedForUserForMode(PrefService* local_state,
                                        std::string_view user_id_hash,
                                        MigrationMode mode) {
  std::string pref_name;
  switch (mode) {
    case MigrationMode::kCopy:
      pref_name = kProfileMigrationCompletedForUserPref;
      break;
    case MigrationMode::kMove:
      pref_name = kProfileMoveMigrationCompletedForUserPref;
      break;
    case MigrationMode::kSkipForNewUser:
      pref_name = kProfileMigrationCompletedForNewUserPref;
      break;
  }

  const auto* pref = local_state->FindPreference(pref_name);
  // Return if the pref is not registered. This can happen in browser_tests. In
  // such a case, assume that migration was completed.
  if (!pref) {
    return true;
  }

  const base::Value* value = pref->GetValue();
  DCHECK(value->is_dict());
  std::optional<bool> is_completed = value->GetDict().FindBool(user_id_hash);

  return is_completed.value_or(false);
}

}  // namespace

void RegisterLocalStatePrefs(PrefRegistrySimple* registry) {
  registry->RegisterDictionaryPref(kDataVerPref);
  registry->RegisterDictionaryPref(kMigrationAttemptCountPref);
  registry->RegisterDictionaryPref(kProfileMigrationCompletedForUserPref);
  registry->RegisterDictionaryPref(kProfileMoveMigrationCompletedForUserPref);
  registry->RegisterDictionaryPref(kProfileMigrationCompletedForNewUserPref);
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

std::optional<MigrationMode> GetCompletedMigrationMode(
    PrefService* local_state,
    std::string_view user_id_hash) {
  // Note that `kCopy` needs to be checked last because the underlying pref
  // `kProfileMigrationCompletedForUserPref` gets set for all migration mode.
  // Check `SetProfileMigrationCompletedForUser()` for details.
  for (const auto mode : {MigrationMode::kMove, MigrationMode::kSkipForNewUser,
                          MigrationMode::kCopy}) {
    if (IsMigrationCompletedForUserForMode(local_state, user_id_hash, mode)) {
      return mode;
    }
  }

  return std::nullopt;
}

bool IsProfileMigrationCompletedForUser(PrefService* local_state,
                                        std::string_view user_id_hash,
                                        bool print_mode) {
  // Allows tests to avoid marking profile migration as completed by getting
  // user_id_hash of the logged in user and updating
  // g_browser_process->local_state() etc.
  if (g_profile_migration_completed_for_test.has_value()) {
    return g_profile_migration_completed_for_test.value();
  }

  std::optional<MigrationMode> mode =
      GetCompletedMigrationMode(local_state, user_id_hash);

  if (print_mode && mode.has_value()) {
    switch (mode.value()) {
      case MigrationMode::kMove:
        LOG(WARNING) << "Completed migration mode = kMove.";
        break;
      case MigrationMode::kSkipForNewUser:
        LOG(WARNING) << "Completed migration mode = kSkipForNewUser.";
        break;
      case MigrationMode::kCopy:
        LOG(WARNING) << "Completed migration mode = kCopy.";
        break;
    }
  }

  return mode.has_value();
}

void SetProfileMigrationCompletedForUser(PrefService* local_state,
                                         std::string_view user_id_hash,
                                         MigrationMode mode) {
  ScopedDictPrefUpdate update(local_state,
                              kProfileMigrationCompletedForUserPref);
  update->Set(user_id_hash, true);

  switch (mode) {
    case MigrationMode::kMove: {
      ScopedDictPrefUpdate move_update(
          local_state, kProfileMoveMigrationCompletedForUserPref);
      move_update->Set(user_id_hash, true);
      break;
    }
    case MigrationMode::kSkipForNewUser: {
      ScopedDictPrefUpdate new_user_update(
          local_state, kProfileMigrationCompletedForNewUserPref);
      new_user_update->Set(user_id_hash, true);
      break;
    }
    case MigrationMode::kCopy:
      // There is no extra pref set for copy migration.
      // Also note that this mode is deprecated.
      break;
  }
}

void ClearProfileMigrationCompletedForUser(PrefService* local_state,
                                           std::string_view user_id_hash) {
  {
    ScopedDictPrefUpdate update(local_state,
                                kProfileMigrationCompletedForUserPref);
    base::Value::Dict& dict = update.Get();
    dict.Remove(user_id_hash);
  }

  {
    ScopedDictPrefUpdate update(local_state,
                                kProfileMoveMigrationCompletedForUserPref);
    base::Value::Dict& dict = update.Get();
    dict.Remove(user_id_hash);
  }

  {
    ScopedDictPrefUpdate update(local_state,
                                kProfileMigrationCompletedForNewUserPref);
    base::Value::Dict& dict = update.Get();
    dict.Remove(user_id_hash);
  }
}

void SetProfileMigrationCompletedForTest(std::optional<bool> is_completed) {
  g_profile_migration_completed_for_test = is_completed;
}

base::Version GetDataVer(PrefService* local_state,
                         std::string_view user_id_hash) {
  const base::Value::Dict& data_versions = local_state->GetDict(kDataVerPref);
  const std::string* data_version_str = data_versions.FindString(user_id_hash);

  if (!data_version_str) {
    return base::Version();
  }

  return base::Version(*data_version_str);
}

void RecordDataVer(PrefService* local_state,
                   std::string_view user_id_hash,
                   const base::Version& version) {
  DCHECK(version.IsValid());
  ScopedDictPrefUpdate update(local_state, kDataVerPref);
  base::Value::Dict& dict = update.Get();
  dict.Set(user_id_hash, version.GetString());
}

}  // namespace ash::standalone_browser::migrator_util
