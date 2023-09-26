// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string_view>

#include "base/component_export.h"

class PrefRegistrySimple;
class PrefService;

namespace ash::standalone_browser::migrator_util {
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
}  // namespace ash::standalone_browser::migrator_util
