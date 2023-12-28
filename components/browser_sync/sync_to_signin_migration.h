// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_BROWSER_SYNC_SYNC_TO_SIGNIN_MIGRATION_H_
#define COMPONENTS_BROWSER_SYNC_SYNC_TO_SIGNIN_MIGRATION_H_

#include "base/feature_list.h"
#include "components/sync/base/model_type.h"

namespace base {
class FilePath;
}  // namespace base

class PrefService;

namespace browser_sync {

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class SyncToSigninMigrationDataTypeDecision {
  kMigrate = 0,
  kDontMigrateTypeDisabled = 1,
  kDontMigrateTypeNotActive = 2,
  kMaxValue = kDontMigrateTypeNotActive
};

SyncToSigninMigrationDataTypeDecision GetSyncToSigninMigrationDataTypeDecision(
    const PrefService* pref_service,
    syncer::ModelType type,
    const char* type_enabled_pref);

void MaybeMigrateSyncingUserToSignedIn(const base::FilePath& profile_path,
                                       PrefService* pref_service);

}  // namespace browser_sync

#endif  // COMPONENTS_BROWSER_SYNC_SYNC_TO_SIGNIN_MIGRATION_H_
