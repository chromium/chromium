// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_BROWSER_SYNC_SYNC_TO_SIGNIN_MIGRATION_H_
#define COMPONENTS_BROWSER_SYNC_SYNC_TO_SIGNIN_MIGRATION_H_

#include "base/feature_list.h"
#include "components/sync/base/data_type.h"

namespace base {
class FilePath;
}  // namespace base

namespace signin {
class IdentityManager;
}  // namespace signin

class PrefService;

namespace browser_sync {

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
// LINT.IfChange(SyncToSigninMigrationDataTypeDecision)
enum class SyncToSigninMigrationDataTypeDecision {
  kMigrate = 0,
  kDontMigrateTypeDisabled = 1,
  kDontMigrateTypeNotActive = 2,
  kMaxValue = kDontMigrateTypeNotActive
};
// LINT.ThenChange(/tools/metrics/histograms/metadata/sync/enums.xml:SyncToSigninMigrationDataTypeDecision)

SyncToSigninMigrationDataTypeDecision GetSyncToSigninMigrationDataTypeDecision(
    const PrefService* pref_service,
    syncer::DataType type,
    const char* type_enabled_pref);

// Migrates the current primary account (signed-in user) from "syncing" to
// "signed-in", if they're eligible. The conditions for eligibility include
// Sync-the-feature being enabled, and having been in a "healthy" state during
// the previous browser run.
// Meant to be called early during startup, in particular before any
// KeyedServices are created.
void MaybeMigrateSyncingUserToSignedIn(const base::FilePath& profile_path,
                                       PrefService* pref_service);

// Returns whether the current primary account was migrated from "syncing" to
// "signed-in" via MaybeMigrateSyncingUserToSignedIn().
bool WasPrimaryAccountMigratedFromSyncingToSignedIn(
    const signin::IdentityManager* identity_manager,
    const PrefService* pref_service);

}  // namespace browser_sync

#endif  // COMPONENTS_BROWSER_SYNC_SYNC_TO_SIGNIN_MIGRATION_H_
