// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_STANDALONE_BROWSER_MIGRATOR_UTIL_H_
#define CHROMEOS_ASH_COMPONENTS_STANDALONE_BROWSER_MIGRATOR_UTIL_H_

#include <optional>
#include <string_view>

#include "base/component_export.h"
#include "base/version.h"

class PrefRegistrySimple;
class PrefService;

namespace ash::standalone_browser::migrator_util {

// Specifies the mode of migration. Used to distinguish what migration mode the
// user used to migrate to Lacros.
enum class MigrationMode {
  kCopy = 0,  // Migrate using `CopyMigrator`. CopyMigrator is deprecated.
  kMove = 1,  // Migrate using `MoveMigrator`.
  kSkipForNewUser = 2,  // Skip migration for new users.
};

// Represents whether the function is being called before the Policy is
// initialized or not.
enum class PolicyInitState {
  kBeforeInit,
  kAfterInit,
};

// Maximum number of migration attempts. Migration will be skipped for the user
// after reaching this limit with this many failed/skipped attempts.
constexpr int kMaxMigrationAttemptCount = 3;

// Registers prefs used via local state PrefService.
COMPONENT_EXPORT(CHROMEOS_ASH_COMPONENTS_STANDALONE_BROWSER)
void RegisterLocalStatePrefs(PrefRegistrySimple* registry);

// Checks whether the number of profile migration attempts have reached its
// limit for the user.
COMPONENT_EXPORT(CHROMEOS_ASH_COMPONENTS_STANDALONE_BROWSER)
bool IsMigrationAttemptLimitReachedForUser(PrefService* local_state,
                                           std::string_view user_id_hash);

// Gets the number of migration attempts for the user stored in
// `kMigrationAttemptCountPref`.
COMPONENT_EXPORT(CHROMEOS_ASH_COMPONENTS_STANDALONE_BROWSER)
int GetMigrationAttemptCountForUser(PrefService* local_state,
                                    std::string_view user_id_hash);

// Increments the migration attempt count stored in
// `kMigrationAttemptCountPref` by 1 for the user identified by
// `user_id_hash`.
COMPONENT_EXPORT(CHROMEOS_ASH_COMPONENTS_STANDALONE_BROWSER)
void UpdateMigrationAttemptCountForUser(PrefService* local_state,
                                        std::string_view user_id_hash);

// Resets the number of migration attempts for the user stored in
// `kMigrationAttemptCountPref.
COMPONENT_EXPORT(CHROMEOS_ASH_COMPONENTS_STANDALONE_BROWSER)
void ClearMigrationAttemptCountForUser(PrefService* local_state,
                                       std::string_view user_id_hash);

// Returns the migration mode that was used to mark profile migration as
// completed. If migration is not completed, the `optional` will not have a
// value.
COMPONENT_EXPORT(CHROMEOS_ASH_COMPONENTS_STANDALONE_BROWSER)
std::optional<MigrationMode> GetCompletedMigrationMode(
    PrefService* local_state,
    std::string_view user_id_hash);

// Checks if profile migration has been completed for the user. If `print_mode`
// is true, it prints the mode the migration was completed with.
COMPONENT_EXPORT(CHROMEOS_ASH_COMPONENTS_STANDALONE_BROWSER)
bool IsProfileMigrationCompletedForUser(PrefService* local_state,
                                        std::string_view user_id_hash,
                                        bool print_mode = false);

// Sets the value of `kProfileMigrationCompletedForUserPref` or
// `kProfileMoveMigrationCompletedForUserPref` to be true for the user
// identified by `user_id_hash`, depending on `mode`.
COMPONENT_EXPORT(CHROMEOS_ASH_COMPONENTS_STANDALONE_BROWSER)
void SetProfileMigrationCompletedForUser(PrefService* local_state,
                                         std::string_view user_id_hash,
                                         MigrationMode mode);

// Clears the values of `kProfileMigrationCompletedForUserPref` and
// `kProfileMoveMigrationCompletedForUserPref` prefs for user identified by
// `user_id_hash`:
COMPONENT_EXPORT(CHROMEOS_ASH_COMPONENTS_STANDALONE_BROWSER)
void ClearProfileMigrationCompletedForUser(PrefService* local_state,
                                           std::string_view user_id_hash);

// Makes `IsProfileMigrationCompletedForUser()` return true without actually
// updating Local State. It allows tests to avoid marking profile migration as
// completed by getting user_id_hash of the logged in user and updating
// g_browser_process->local_state() etc.
COMPONENT_EXPORT(CHROMEOS_ASH_COMPONENTS_STANDALONE_BROWSER)
void SetProfileMigrationCompletedForTest(std::optional<bool> is_completed);

// Reads `kDataVerPref` and gets corresponding data version for `user_id_hash`.
// If no such version is registered yet, returns `Version` that is invalid.
// Should only be called on UI thread since it reads from `LocalState`.
COMPONENT_EXPORT(CHROMEOS_ASH_COMPONENTS_STANDALONE_BROWSER)
base::Version GetDataVer(PrefService* local_state,
                         std::string_view user_id_hash);

// Records data version for `user_id_hash` in `LocalState`. Should only be
// called on UI thread since it reads from `LocalState`.
COMPONENT_EXPORT(CHROMEOS_ASH_COMPONENTS_STANDALONE_BROWSER)
void RecordDataVer(PrefService* local_state,
                   std::string_view user_id_hash,
                   const base::Version& version);

}  // namespace ash::standalone_browser::migrator_util

#endif  // CHROMEOS_ASH_COMPONENTS_STANDALONE_BROWSER_MIGRATOR_UTIL_H_
