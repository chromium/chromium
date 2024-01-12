// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/browser_sync/sync_to_signin_migration.h"

#include <string>

#include "base/feature_list.h"
#include "base/files/file_util.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/strcat.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "components/bookmarks/common/bookmark_constants.h"
#include "components/browser_sync/browser_sync_switches.h"
#include "components/password_manager/core/browser/password_manager_constants.h"
#include "components/prefs/pref_service.h"
#include "components/signin/public/base/gaia_id_hash.h"
#include "components/signin/public/base/signin_pref_names.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/sync/base/data_type_histogram.h"
#include "components/sync/base/features.h"
#include "components/sync/base/model_type.h"
#include "components/sync/base/pref_names.h"
#include "components/sync/service/sync_feature_status_for_migrations_recorder.h"
#include "components/sync/service/sync_prefs.h"

namespace browser_sync {

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
  kUndoMigration = 6,
  kUndoNotNecessary = 7,
  kMaxValue = kUndoNotNecessary
};

SyncToSigninMigrationDecision GetSyncToSigninMigrationDecision(
    const PrefService* pref_service) {
  // If the flag to undo the migration is set, that overrides anything else.
  if (base::FeatureList::IsEnabled(
          switches::kUndoMigrationOfSyncingUserToSignedIn)) {
    if (pref_service
            ->GetString(prefs::kGoogleServicesSyncingGaiaIdMigratedToSignedIn)
            .empty()) {
      // The user was never migrated, or the migration was already undone.
      // Nothing to be done here.
      return SyncToSigninMigrationDecision::kUndoNotNecessary;
    } else {
      // The user (or more precisely, this profile) was previously migrated.
      return SyncToSigninMigrationDecision::kUndoMigration;
    }
  }

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

  // Check the feature flag(s) last, so that metrics can record all the other
  // reasons to not do the migration, even with the flag disabled.
  if (!base::FeatureList::IsEnabled(
          syncer::kReplaceSyncPromosWithSignInPromos) ||
      !base::FeatureList::IsEnabled(switches::kMigrateSyncingUserToSignedIn)) {
    return SyncToSigninMigrationDecision::kDontMigrateFlagDisabled;
  }

  return SyncToSigninMigrationDecision::kMigrate;
}

void UndoSyncToSigninMigration(PrefService* pref_service) {
  const std::string migrated_gaia_id = pref_service->GetString(
      prefs::kGoogleServicesSyncingGaiaIdMigratedToSignedIn);

  if (migrated_gaia_id.empty()) {
    // The user was never migrated, or the migration was already undone. Nothing
    // to be done here.
    return;
  }

  const std::string signed_in_gaia_id =
      pref_service->GetString(prefs::kGoogleServicesAccountId);

  if (signed_in_gaia_id != migrated_gaia_id) {
    // The user was migrated, but has since signed out, or signed in with a
    // different account. Clean up the migration prefs; otherwise nothing to be
    // done here.
    pref_service->ClearPref(
        prefs::kGoogleServicesSyncingGaiaIdMigratedToSignedIn);
    pref_service->ClearPref(
        prefs::kGoogleServicesSyncingUsernameMigratedToSignedIn);
    return;
  }

  // The user was migrated, and is still signed in with the same account. Undo
  // the migration.

  // Mark the user as syncing again.
  pref_service->SetBoolean(prefs::kGoogleServicesConsentedToSync, true);
#if !BUILDFLAG(IS_CHROMEOS_ASH)
  pref_service->SetBoolean(
      syncer::prefs::internal::kSyncInitialSyncFeatureSetupComplete, true);
#endif

  // Restore the "previously syncing user" prefs too.
  pref_service->SetString(prefs::kGoogleServicesLastSyncingAccountIdDeprecated,
                          signed_in_gaia_id);
  pref_service->SetString(prefs::kGoogleServicesLastSyncingGaiaId,
                          signed_in_gaia_id);
  pref_service->SetString(
      prefs::kGoogleServicesLastSyncingUsername,
      pref_service->GetString(
          prefs::kGoogleServicesSyncingUsernameMigratedToSignedIn));

  // Clear the "migrated user" prefs, so the "undo" logic doesn't run again.
  pref_service->ClearPref(
      prefs::kGoogleServicesSyncingGaiaIdMigratedToSignedIn);
  pref_service->ClearPref(
      prefs::kGoogleServicesSyncingUsernameMigratedToSignedIn);

  // Selected-data-types prefs: No reverse migration - the user will just go
  // back to their previous Sync settings.

#if BUILDFLAG(IS_IOS)
  // Bookmarks: The forward migration is an atomic file move. Either that
  // happened, in which case the Sync machinery will clean up the account store
  // and start over with the local-or-syncable store. Or the file move didn't
  // happen for some reason. Either way, nothing to be done here.
#else
  // TODO(crbug.com/1503647): On platforms other than iOS, the forward migration
  // for bookmarks isn't implemented yet, so the reverse migration can't be
  // implemented either.
  NOTIMPLEMENTED();
#endif  // BUILDFLAG(IS_IOS)

  // Passwords: Same as bookmarks, this is an atomic file move. Nothing to be
  // done here.

  // ReadingList: The migration is asynchronous. Most likely it has been
  // completed by this point, but in case it's still pending, stop attempting it
  // now.
  pref_service->ClearPref(
      syncer::prefs::internal::kMigrateReadingListFromLocalToAccount);
}

const char* GetHistogramMigratingOrNotInfix(bool doing_migration) {
  return doing_migration ? "Migration." : "DryRun.";
}

}  // namespace

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

void MaybeMigrateSyncingUserToSignedIn(const base::FilePath& profile_path,
                                       PrefService* pref_service) {
  base::Time start_time = base::Time::Now();

  // ======================================
  // Global migration decision and metrics.
  // ======================================

  const SyncToSigninMigrationDecision decision =
      GetSyncToSigninMigrationDecision(pref_service);
  base::UmaHistogramEnumeration("Sync.SyncToSigninMigrationDecision", decision);

  switch (decision) {
    case SyncToSigninMigrationDecision::kUndoMigration:
      // Undo the migration (if appropriate) and nothing else.
      UndoSyncToSigninMigration(pref_service);
      return;
    case SyncToSigninMigrationDecision::kDontMigrateNotSignedIn:
    case SyncToSigninMigrationDecision::kDontMigrateNotSyncing:
    case SyncToSigninMigrationDecision::kDontMigrateSyncStatusUndefined:
    case SyncToSigninMigrationDecision::kDontMigrateSyncStatusInitializing:
    case SyncToSigninMigrationDecision::kUndoNotNecessary:
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
  // Guaranteed to be non-empty by GetSyncToSigninMigrationDecision().
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
  // Also clear the "InitialSyncFeatureSetup" pref. It's not needed
  // post-migration, and that pref being true without ConsentLevel::kSync would
  // be an inconsistent state.
#if !BUILDFLAG(IS_CHROMEOS_ASH)
  pref_service->ClearPref(
      syncer::prefs::internal::kSyncInitialSyncFeatureSetupComplete);
#endif

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

  bool migration_successful = true;

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

    migration_successful =
        migration_successful && (error == base::File::Error::FILE_OK);
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

    migration_successful =
        migration_successful && (error == base::File::Error::FILE_OK);
  }
#else
  // TODO(crbug.com/1503647): On platforms other than iOS, the on-disk layout of
  // bookmarks may be different (no two separate JSON files).
  NOTIMPLEMENTED();
#endif  // BUILDFLAG(IS_IOS)

  // Reading list: Set migration pref. The ModelTypeStoreServiceImpl will read
  // it, and instruct the ModelTypeStoreBackend to actually migrate the data.
  // Note that ModelTypeStoreServiceImpl (a KeyedService) can't have been
  // constructed yet, so no risk of race conditions.
  if (reading_list_decision ==
      SyncToSigninMigrationDataTypeDecision::kMigrate) {
    pref_service->SetBoolean(
        syncer::prefs::internal::kMigrateReadingListFromLocalToAccount, true);
    syncer::RecordSyncToSigninMigrationReadingListStep(
        syncer::ReadingListMigrationStep::kMigrationRequested);

    // Note: Triggering this migration cannot fail, so no need to update
    // `migration_successful` here. The actual outcome will be recorded in other
    // histograms.
  }

  // Finally, record the overall outcome, i.e. whether all individual data type
  // migrations were successful. The total count of this histogram also serves
  // as the number of migrations that were completed.
  base::UmaHistogramBoolean("Sync.SyncToSigninMigrationOutcome",
                            migration_successful);
  base::UmaHistogramTimes("Sync.SyncToSigninMigrationTime",
                          base::Time::Now() - start_time);
}

bool WasPrimaryAccountMigratedFromSyncingToSignedIn(
    const signin::IdentityManager* identity_manager,
    const PrefService* pref_service) {
  // Only signed-in non-syncing users can be in the "migrated" state.
  if (!identity_manager->HasPrimaryAccount(signin::ConsentLevel::kSignin) ||
      identity_manager->HasPrimaryAccount(signin::ConsentLevel::kSync)) {
    return false;
  }

  // Check if the current signed-in account ID matches the migrated account ID.
  // In the common case where the account was *not* migrated, the migrated
  // account ID will be empty, and thus not match the current account ID.
  std::string authenticated_gaia_id =
      identity_manager->GetPrimaryAccountInfo(signin::ConsentLevel::kSignin)
          .gaia;
  std::string migrated_gaia_id = pref_service->GetString(
      prefs::kGoogleServicesSyncingGaiaIdMigratedToSignedIn);
  return migrated_gaia_id == authenticated_gaia_id;
}

}  // namespace browser_sync
