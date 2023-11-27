// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/browser_sync/sync_to_signin_migration.h"

#include <string>

#include "base/feature_list.h"
#include "base/files/file_util.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/strcat.h"
#include "build/build_config.h"
#include "components/bookmarks/common/bookmark_constants.h"
#include "components/password_manager/core/browser/password_manager_constants.h"
#include "components/prefs/pref_service.h"
#include "components/signin/public/base/gaia_id_hash.h"
#include "components/signin/public/base/signin_pref_names.h"
#include "components/sync/base/model_type.h"
#include "components/sync/base/pref_names.h"
#include "components/sync/service/sync_feature_status_for_migrations_recorder.h"
#include "components/sync/service/sync_prefs.h"

namespace browser_sync {

BASE_FEATURE(kMigrateSyncingUserToSignedIn,
             "MigrateSyncingUserToSignedIn",
             base::FEATURE_DISABLED_BY_DEFAULT);

namespace {

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class SyncToSigninMigrationDecision {
  kMigrate = 0,
  kDontMigrateNotSignedIn = 1,
  kDontMigrateNotSyncing = 2,
  kDontMigrateSyncStatusUndefined = 3,
  kDontMigrateSyncStatusInitializing = 4,
  kDontMigrateFlagDisabled = 5,
  kMaxValue = kDontMigrateFlagDisabled
};

SyncToSigninMigrationDecision ShouldMigrateSyncingUserToSignedIn(
    const PrefService* pref_service) {
  if (pref_service->GetString(prefs::kGoogleServicesAccountId).empty()) {
    // Signed-out user, nothing to migrate.
    return SyncToSigninMigrationDecision::kDontMigrateNotSignedIn;
  }

  if (!pref_service->GetBoolean(prefs::kGoogleServicesConsentedToSync)) {
    // Not a syncing user, nothing to migrate (or already migrated).
    return SyncToSigninMigrationDecision::kDontMigrateNotSyncing;
  }

  syncer::SyncFeatureStatusForSyncToSigninMigration status =
      syncer::SyncFeatureStatusForMigrationsRecorder::
          GetSyncFeatureStatusForSyncToSigninMigration(pref_service);
  switch (status) {
    case syncer::SyncFeatureStatusForSyncToSigninMigration::kDisabledOrPaused:
    case syncer::SyncFeatureStatusForSyncToSigninMigration::kActive:
      // In both these cases, the status is known, and migration can go ahead.
      break;
    case syncer::SyncFeatureStatusForSyncToSigninMigration::kInitializing:
      // In the previous browser run, Sync didn't finish initializing. Defer
      // migration.
      return SyncToSigninMigrationDecision::kDontMigrateSyncStatusInitializing;
    case syncer::SyncFeatureStatusForSyncToSigninMigration::kUndefined:
      // The Sync status pref was never set (which should only happen once per
      // client), or has an unknown/invalid value (which should never happen).
      return SyncToSigninMigrationDecision::kDontMigrateSyncStatusUndefined;
  }
  // TODO(crbug.com/1486420): After some number of attempts, treat
  // "initializing" or "undefined/unknown" as "Sync disabled" and go ahead with
  // the migration?

  // Check the feature flag last, so that metrics can record all the other
  // reasons to not do the migration, even with the flag disabled.
  if (!base::FeatureList::IsEnabled(kMigrateSyncingUserToSignedIn)) {
    return SyncToSigninMigrationDecision::kDontMigrateFlagDisabled;
  }

  return SyncToSigninMigrationDecision::kMigrate;
}

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
    const char* type_enabled_pref) {
#if BUILDFLAG(IS_CHROMEOS_ASH)
  // In ChromeOS-Ash, the "initial-setup-complete" pref doesn't exist.
  bool initial_setup_complete = true;
#else
  bool initial_setup_complete = pref_service->GetBoolean(
      syncer::prefs::internal::kSyncInitialSyncFeatureSetupComplete);
#endif
  bool sync_everything = pref_service->GetBoolean(
      syncer::prefs::internal::kSyncKeepEverythingSynced);

  bool type_enabled =
      initial_setup_complete &&
      (sync_everything || pref_service->GetBoolean(type_enabled_pref));
  if (!type_enabled) {
    return SyncToSigninMigrationDataTypeDecision::kDontMigrateTypeDisabled;
  }

  bool type_active = syncer::SyncFeatureStatusForMigrationsRecorder::
      GetSyncDataTypeActiveForSyncToSigninMigration(pref_service, type);
  if (!type_active) {
    return SyncToSigninMigrationDataTypeDecision::kDontMigrateTypeNotActive;
  }

  return SyncToSigninMigrationDataTypeDecision::kMigrate;
}

const char* GetHistogramMigratingOrNotInfix(bool doing_migration) {
  return doing_migration ? "Migration." : "DryRun.";
}

}  // namespace

void MaybeMigrateSyncingUserToSignedIn(const base::FilePath& profile_path,
                                       PrefService* pref_service) {
  // ======================================
  // Global migration decision and metrics.
  // ======================================

  const SyncToSigninMigrationDecision decision =
      ShouldMigrateSyncingUserToSignedIn(pref_service);
  base::UmaHistogramEnumeration("Sync.SyncToSigninMigrationDecision", decision);

  switch (decision) {
    case SyncToSigninMigrationDecision::kDontMigrateNotSignedIn:
    case SyncToSigninMigrationDecision::kDontMigrateNotSyncing:
    case SyncToSigninMigrationDecision::kDontMigrateSyncStatusUndefined:
    case SyncToSigninMigrationDecision::kDontMigrateSyncStatusInitializing:
      // No migration, and no point in recording per-type metrics - we're done.
      return;
    case SyncToSigninMigrationDecision::kDontMigrateFlagDisabled:
    case SyncToSigninMigrationDecision::kMigrate:
      // If actually migrating, or the feature flag being disabled is the only
      // reason for not migrating, also record more detailed per-type metrics.
      break;
  }

  // ===================================================
  // Data-type-specific migration decisions and metrics.
  // ===================================================

  const bool doing_migration =
      decision == SyncToSigninMigrationDecision::kMigrate;

  const SyncToSigninMigrationDataTypeDecision bookmarks_decision =
      GetSyncToSigninMigrationDataTypeDecision(
          pref_service, syncer::BOOKMARKS,
          syncer::prefs::internal::kSyncBookmarks);
  base::UmaHistogramEnumeration(
      base::StrCat({"Sync.SyncToSigninMigrationDecision.",
                    GetHistogramMigratingOrNotInfix(doing_migration),
                    syncer::ModelTypeToHistogramSuffix(syncer::BOOKMARKS)}),
      bookmarks_decision);

  const SyncToSigninMigrationDataTypeDecision passwords_decision =
      GetSyncToSigninMigrationDataTypeDecision(
          pref_service, syncer::PASSWORDS,
          syncer::prefs::internal::kSyncPasswords);
  base::UmaHistogramEnumeration(
      base::StrCat({"Sync.SyncToSigninMigrationDecision.",
                    GetHistogramMigratingOrNotInfix(doing_migration),
                    syncer::ModelTypeToHistogramSuffix(syncer::PASSWORDS)}),
      passwords_decision);

  const SyncToSigninMigrationDataTypeDecision reading_list_decision =
      GetSyncToSigninMigrationDataTypeDecision(
          pref_service, syncer::READING_LIST,
          syncer::prefs::internal::kSyncReadingList);
  base::UmaHistogramEnumeration(
      base::StrCat({"Sync.SyncToSigninMigrationDecision.",
                    GetHistogramMigratingOrNotInfix(doing_migration),
                    syncer::ModelTypeToHistogramSuffix(syncer::READING_LIST)}),
      reading_list_decision);

  if (decision != SyncToSigninMigrationDecision::kMigrate) {
    return;
  }

  // =========================
  // Global (prefs) migration.
  // =========================

  // The account identifier of an account is its Gaia ID. So
  // `kGoogleServicesAccountId` stores the Gaia ID of the syncing account.
  const std::string gaia_id =
      pref_service->GetString(prefs::kGoogleServicesAccountId);
  // Guaranteed to be non-empty by ShouldMigrateSyncingUserToSignedIn().
  CHECK(!gaia_id.empty());

  // Remove ConsentLevel::kSync. This also ensures that the whole migration will
  // not run a second time.
  // Note that it's important to explicitly set this pref to false (not just
  // clear it), since the signin code treats "unset" differently.
  pref_service->SetBoolean(prefs::kGoogleServicesConsentedToSync, false);
  // Save the ID and username of the migrated account, to be able to revert the
  // migration if necessary.
  pref_service->SetString(prefs::kGoogleServicesSyncingGaiaIdMigratedToSignedIn,
                          gaia_id);
  pref_service->SetString(
      prefs::kGoogleServicesSyncingUsernameMigratedToSignedIn,
      pref_service->GetString(prefs::kGoogleServicesLastSyncingUsername));
  // Clear the "previously syncing user" prefs, to prevent accidental misuse.
  pref_service->ClearPref(prefs::kGoogleServicesLastSyncingAccountIdDeprecated);
  pref_service->ClearPref(prefs::kGoogleServicesLastSyncingGaiaId);
  pref_service->ClearPref(prefs::kGoogleServicesLastSyncingUsername);

  // Migrate the global data type prefs (used for Sync-the-feature) over to the
  // account-specific ones.
  signin::GaiaIdHash gaia_id_hash = signin::GaiaIdHash::FromGaiaId(gaia_id);
  syncer::SyncPrefs::MigrateGlobalDataTypePrefsToAccount(pref_service,
                                                         gaia_id_hash);

  // Ensure the prefs changes are persisted as soon as possible. (They get
  // persisted on shutdown anyway, but better make sure.)
  pref_service->CommitPendingWrite();

  // ==============================
  // Data-type-specific migrations.
  // ==============================

  // Move passwords DB file, if password sync is enabled.
  if (passwords_decision == SyncToSigninMigrationDataTypeDecision::kMigrate) {
    base::FilePath from_path =
        profile_path.Append(password_manager::kLoginDataForProfileFileName);
    base::FilePath to_path =
        profile_path.Append(password_manager::kLoginDataForAccountFileName);
    base::File::Error error = base::File::Error::FILE_OK;
    base::ReplaceFile(from_path, to_path, &error);
    base::UmaHistogramExactLinear(
        "Sync.SyncToSigninMigrationOutcome.PasswordsFileMove", -error,
        -base::File::FILE_ERROR_MAX);
  }

#if BUILDFLAG(IS_IOS)
  // Move bookmarks json file, if bookmark sync is enabled.
  if (bookmarks_decision == SyncToSigninMigrationDataTypeDecision::kMigrate) {
    base::FilePath from_path =
        profile_path.Append(bookmarks::kLocalOrSyncableBookmarksFileName);
    base::FilePath to_path =
        profile_path.Append(bookmarks::kAccountBookmarksFileName);
    base::File::Error error = base::File::Error::FILE_OK;
    base::ReplaceFile(from_path, to_path, &error);
    base::UmaHistogramExactLinear(
        "Sync.SyncToSigninMigrationOutcome.BookmarksFileMove", -error,
        -base::File::FILE_ERROR_MAX);
  }
#else
  // TODO(crbug.com/1503647): On platforms other than iOS, the on-disk layout of
  // bookmarks may be different (no two separate JSON files).
  NOTIMPLEMENTED();
#endif  // BUILDFLAG(IS_IOS)

  // TODO(crbug.com/1486420): Add migration logic for ReadingList.
  NOTIMPLEMENTED();
}

}  // namespace browser_sync
