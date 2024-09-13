// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/browser_sync/sync_to_signin_migration.h"

#include <memory>
#include <string>

#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "components/browser_sync/browser_sync_switches.h"
#include "components/prefs/testing_pref_service.h"
#include "components/signin/public/base/signin_pref_names.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/sync/base/data_type.h"
#include "components/sync/base/features.h"
#include "components/sync/base/pref_names.h"
#include "components/sync/base/user_selectable_type.h"
#include "components/sync/service/sync_feature_status_for_migrations_recorder.h"
#include "components/sync/service/sync_prefs.h"
#include "components/sync/test/test_sync_service.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace browser_sync {
namespace {

class SyncToSigninMigrationTestBase {
 public:
  SyncToSigninMigrationTestBase(bool migration_feature_enabled,
                                bool force_migration_feature_enabled) {
    features_.InitWithFeatureStates(
        {{syncer::kReplaceSyncPromosWithSignInPromos, true},
         {switches::kMigrateSyncingUserToSignedIn, migration_feature_enabled},
         {switches::kForceMigrateSyncingUserToSignedIn,
          force_migration_feature_enabled}});

    signin::IdentityManager::RegisterProfilePrefs(pref_service_.registry());
    syncer::SyncPrefs::RegisterProfilePrefs(pref_service_.registry());

    sync_prefs_ = std::make_unique<syncer::SyncPrefs>(&pref_service_);

    CHECK(fake_profile_dir_.CreateUniqueTempDir());
  }
  virtual ~SyncToSigninMigrationTestBase() = default;

  void RecordStateToPrefs(bool include_status_recorder = true) {
    // Populate signin prefs based on the state of the TestSyncService.
    pref_service_.SetString(prefs::kGoogleServicesAccountId,
                            sync_service_.GetAccountInfo().gaia);
    pref_service_.SetBoolean(prefs::kGoogleServicesConsentedToSync,
                             sync_service_.HasSyncConsent());
    pref_service_.SetString(prefs::kGoogleServicesLastSyncingGaiaId,
                            sync_service_.GetAccountInfo().gaia);
    pref_service_.SetString(prefs::kGoogleServicesLastSyncingUsername,
                            sync_service_.GetAccountInfo().email);

    // Populate sync prefs. The TestSyncService doesn't write these, so they
    // have to be set manually here.
    syncer::SyncUserSettings* settings = sync_service_.GetUserSettings();
    sync_prefs_->SetSelectedTypesForSyncingUser(
        settings->IsSyncEverythingEnabled(),
        settings->GetRegisteredSelectableTypes(), settings->GetSelectedTypes());
#if !BUILDFLAG(IS_CHROMEOS_ASH)
    sync_prefs_->SetInitialSyncFeatureSetupComplete();
#endif  // !BUILDFLAG(IS_CHROMEOS_ASH)

    if (include_status_recorder) {
      // Populate migration-specific Sync status prefs.
      syncer::SyncFeatureStatusForMigrationsRecorder recorder(&pref_service_,
                                                              &sync_service_);
      // Before destroying the recorder again, tell it that sync is shutting
      // down to avoid a dangling observer.
      recorder.OnSyncShutdown(&sync_service_);
    }
  }

  void FastForwardBy(base::TimeDelta delta) {
    task_environment_.FastForwardBy(delta);
  }

 private:
  base::test::ScopedFeatureList features_;
  base::test::SingleThreadTaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};

 protected:
  TestingPrefServiceSimple pref_service_;
  std::unique_ptr<syncer::SyncPrefs> sync_prefs_;
  syncer::TestSyncService sync_service_;
  base::ScopedTempDir fake_profile_dir_;
};

// Fixture for tests covering the migration logic. The test param determines
// whether the force-migration feature flag is enabled or not (the regular
// migration is always enabled in this test).
class SyncToSigninMigrationTest : public SyncToSigninMigrationTestBase,
                                  public testing::TestWithParam<bool> {
 public:
  SyncToSigninMigrationTest()
      : SyncToSigninMigrationTestBase(
            /*migration_feature_enabled=*/true,
            /*force_migration_feature_enabled=*/IsForceMigrationEnabled()) {}

  bool IsForceMigrationEnabled() const { return GetParam(); }
};

TEST_P(SyncToSigninMigrationTest, SyncActive) {
  // Sync is active.
  ASSERT_EQ(sync_service_.GetTransportState(),
            syncer::SyncService::TransportState::ACTIVE);
  ASSERT_TRUE(sync_service_.HasSyncConsent());

  const std::string gaia_id = sync_service_.GetAccountInfo().gaia;
  const std::string email = sync_service_.GetAccountInfo().email;

  // Save the above state to prefs.
  RecordStateToPrefs();
  ASSERT_TRUE(sync_prefs_->IsInitialSyncFeatureSetupComplete());

  // Before the migration, there are no per-account selected types.
  ASSERT_TRUE(
      pref_service_.GetDict(syncer::prefs::internal::kSelectedTypesPerAccount)
          .empty());

  // Run the migration. This should change the user to be non-syncing.
  MaybeMigrateSyncingUserToSignedIn(fake_profile_dir_.GetPath(),
                                    &pref_service_);

  // Note that TestSyncService doesn't consume the prefs, so verify the prefs
  // directly here.
  // The user should still be signed in.
  EXPECT_EQ(pref_service_.GetString(prefs::kGoogleServicesAccountId), gaia_id);
  // But not syncing anymore.
  EXPECT_FALSE(pref_service_.GetBoolean(prefs::kGoogleServicesConsentedToSync));
#if !BUILDFLAG(IS_CHROMEOS_ASH)
  EXPECT_FALSE(sync_prefs_->IsInitialSyncFeatureSetupComplete());
#endif
  // The fact that the user was migrated should be recorded in prefs.
  EXPECT_EQ(pref_service_.GetString(
                prefs::kGoogleServicesSyncingGaiaIdMigratedToSignedIn),
            gaia_id);
  EXPECT_EQ(pref_service_.GetString(
                prefs::kGoogleServicesSyncingUsernameMigratedToSignedIn),
            email);

  // There should be per-account selected types now. The details of this are
  // covered in SyncPrefs unit tests.
  EXPECT_FALSE(
      pref_service_.GetDict(syncer::prefs::internal::kSelectedTypesPerAccount)
          .empty());
}

TEST_P(SyncToSigninMigrationTest, SyncStatusPrefsUnset) {
  // Everything is active.
  ASSERT_EQ(sync_service_.GetTransportState(),
            syncer::SyncService::TransportState::ACTIVE);
  ASSERT_TRUE(sync_service_.HasSyncConsent());

  // Save the Sync configuration (enabled data types etc) to prefs, but not the
  // migration-specific status prefs. This simulates the case of an old client
  // which has never written those prefs.
  RecordStateToPrefs(/*include_status_recorder=*/false);

  // Take a copy of all current pref values, to verify whether the migration
  // modified any of them.
  const base::Value::Dict all_prefs =
      pref_service_.user_prefs_store()->GetValues();

  // Trigger the migration - it should only run in this state if the
  // force-migration is enabled.
  MaybeMigrateSyncingUserToSignedIn(fake_profile_dir_.GetPath(),
                                    &pref_service_);

  // Note that TestSyncService doesn't consume the prefs, so verify the prefs
  // directly here.
  if (IsForceMigrationEnabled()) {
    // There should be per-account selected types now. The details of this are
    // covered in SyncPrefs unit tests.
    EXPECT_FALSE(
        pref_service_.GetDict(syncer::prefs::internal::kSelectedTypesPerAccount)
            .empty());
  } else {
    // Since the migration didn't actually run, the prefs should be unmodified.
    EXPECT_EQ(pref_service_.user_prefs_store()->GetValues(), all_prefs);
  }
}

TEST_P(SyncToSigninMigrationTest, SyncTransport) {
  // There's no Sync consent, but otherwise everything is active (running in
  // transport mode).
  sync_service_.SetSignedIn(signin::ConsentLevel::kSignin);
  ASSERT_EQ(sync_service_.GetTransportState(),
            syncer::SyncService::TransportState::ACTIVE);

  // Save the above state to prefs.
  RecordStateToPrefs();

  // Take a copy of all current pref values, to verify that the migration
  // doesn't modify any of them.
  const base::Value::Dict all_prefs =
      pref_service_.user_prefs_store()->GetValues();

  // Trigger the migration - it should NOT actually run in this state.
  MaybeMigrateSyncingUserToSignedIn(fake_profile_dir_.GetPath(),
                                    &pref_service_);

  // Note that TestSyncService doesn't consume the prefs, so verify the prefs
  // directly here.
  // Since the migration didn't actually run, the prefs should be unmodified.
  EXPECT_EQ(pref_service_.user_prefs_store()->GetValues(), all_prefs);
}

TEST_P(SyncToSigninMigrationTest, SyncDisabledByPolicy) {
  // The user is signed in and opted in to Sync, but Sync is disabled via
  // enterprise policy.
  sync_service_.SetAllowedByEnterprisePolicy(false);
  ASSERT_EQ(sync_service_.GetTransportState(),
            syncer::SyncService::TransportState::DISABLED);
  ASSERT_TRUE(sync_service_.HasSyncConsent());

  const std::string gaia_id = sync_service_.GetAccountInfo().gaia;
  const std::string email = sync_service_.GetAccountInfo().email;

  // Save the above state to prefs.
  RecordStateToPrefs();

  // Before the migration, there are no per-account selected types.
  ASSERT_TRUE(
      pref_service_.GetDict(syncer::prefs::internal::kSelectedTypesPerAccount)
          .empty());

  // Run the migration. This should change the user to be non-syncing (even
  // though Sync wasn't actually active).
  MaybeMigrateSyncingUserToSignedIn(fake_profile_dir_.GetPath(),
                                    &pref_service_);

  // Note that TestSyncService doesn't consume the prefs, so verify the prefs
  // directly here.
  // The user should still be signed in.
  EXPECT_EQ(pref_service_.GetString(prefs::kGoogleServicesAccountId), gaia_id);
  // But not syncing anymore.
  EXPECT_FALSE(pref_service_.GetBoolean(prefs::kGoogleServicesConsentedToSync));
  // The fact that the user was migrated should be recorded in prefs.
  EXPECT_EQ(pref_service_.GetString(
                prefs::kGoogleServicesSyncingGaiaIdMigratedToSignedIn),
            gaia_id);
  EXPECT_EQ(pref_service_.GetString(
                prefs::kGoogleServicesSyncingUsernameMigratedToSignedIn),
            email);

  // There should be per-account selected types now. The details of this are
  // covered in SyncPrefs unit tests.
  EXPECT_FALSE(
      pref_service_.GetDict(syncer::prefs::internal::kSelectedTypesPerAccount)
          .empty());
}

TEST_P(SyncToSigninMigrationTest, SyncPaused_MinDelayNotPassed) {
  // Sync-the-feature is enabled, but in the "paused" state due to a persistent
  // auth error.
  sync_service_.SetPersistentAuthError();
  RecordStateToPrefs();
  ASSERT_EQ(sync_service_.GetTransportState(),
            syncer::SyncService::TransportState::PAUSED);
  ASSERT_TRUE(sync_service_.HasSyncConsent());
  ASSERT_TRUE(sync_service_.GetActiveDataTypes().empty());
  ASSERT_TRUE(
      pref_service_.GetDict(syncer::prefs::internal::kSelectedTypesPerAccount)
          .empty());
  const std::string gaia_id = sync_service_.GetAccountInfo().gaia;
  const std::string email = sync_service_.GetAccountInfo().email;

  // Attempt to migrate.
  MaybeMigrateSyncingUserToSignedIn(fake_profile_dir_.GetPath(),
                                    &pref_service_);

  // Note that TestSyncService doesn't consume the prefs, so verify the prefs
  // directly here.
  if (IsForceMigrationEnabled()) {
    // Enabling the forced migration flag causes the min delay requirement to be
    // ignored, immediately moving the user to the signed-in state.
    EXPECT_EQ(pref_service_.GetString(prefs::kGoogleServicesAccountId),
              gaia_id);
    EXPECT_FALSE(
        pref_service_.GetBoolean(prefs::kGoogleServicesConsentedToSync));
    EXPECT_EQ(pref_service_.GetString(
                  prefs::kGoogleServicesSyncingGaiaIdMigratedToSignedIn),
              gaia_id);
    EXPECT_EQ(pref_service_.GetString(
                  prefs::kGoogleServicesSyncingUsernameMigratedToSignedIn),
              email);
    EXPECT_FALSE(
        pref_service_.GetDict(syncer::prefs::internal::kSelectedTypesPerAccount)
            .empty());
  } else {
    // The migration should not run yet, giving the user some time to resolve
    // the error (switches::kMinDelayToMigrateSyncPaused).
    EXPECT_EQ(pref_service_.GetString(prefs::kGoogleServicesAccountId),
              gaia_id);
    EXPECT_TRUE(
        pref_service_.GetBoolean(prefs::kGoogleServicesConsentedToSync));
    EXPECT_EQ(pref_service_.GetString(
                  prefs::kGoogleServicesSyncingGaiaIdMigratedToSignedIn),
              std::string());
    EXPECT_EQ(pref_service_.GetString(
                  prefs::kGoogleServicesSyncingUsernameMigratedToSignedIn),
              std::string());
    EXPECT_TRUE(
        pref_service_.GetDict(syncer::prefs::internal::kSelectedTypesPerAccount)
            .empty());
  }
}

TEST_P(SyncToSigninMigrationTest, SyncPaused_MinDelayPassed) {
  if (IsForceMigrationEnabled()) {
    // When the forced migration flag is enabled, there is no waiting for the
    // error to be resolved. The migration runs on the first attempt and that's
    // covered in SyncPaused_MinDelayNotPassed.
    return;
  }

  // Sync-the-feature is enabled but transport is "paused" due to a persistent
  // auth error. Simulate a first migration attempt that does nothing (see
  // SyncPaused_MinDelayNotPassed test).
  sync_service_.SetPersistentAuthError();
  ASSERT_EQ(sync_service_.GetTransportState(),
            syncer::SyncService::TransportState::PAUSED);
  ASSERT_TRUE(sync_service_.HasSyncConsent());
  ASSERT_TRUE(sync_service_.GetActiveDataTypes().empty());
  ASSERT_TRUE(
      pref_service_.GetDict(syncer::prefs::internal::kSelectedTypesPerAccount)
          .empty());
  const std::string gaia_id = sync_service_.GetAccountInfo().gaia;
  const std::string email = sync_service_.GetAccountInfo().email;
  RecordStateToPrefs();
  MaybeMigrateSyncingUserToSignedIn(fake_profile_dir_.GetPath(),
                                    &pref_service_);
  ASSERT_TRUE(sync_service_.HasSyncConsent());

  // Now, enough time has passed and the migration is attempted again.
  FastForwardBy(switches::kMinDelayToMigrateSyncPaused.Get());
  MaybeMigrateSyncingUserToSignedIn(fake_profile_dir_.GetPath(),
                                    &pref_service_);

  // Note that TestSyncService doesn't consume the prefs, so verify the prefs
  // directly here.
  // The user should still be signed in.
  EXPECT_EQ(pref_service_.GetString(prefs::kGoogleServicesAccountId), gaia_id);
  // But not syncing anymore.
  EXPECT_FALSE(pref_service_.GetBoolean(prefs::kGoogleServicesConsentedToSync));
  // The fact that the user was migrated should be recorded in prefs.
  EXPECT_EQ(pref_service_.GetString(
                prefs::kGoogleServicesSyncingGaiaIdMigratedToSignedIn),
            gaia_id);
  EXPECT_EQ(pref_service_.GetString(
                prefs::kGoogleServicesSyncingUsernameMigratedToSignedIn),
            email);
  // There should be per-account selected types now. The details of this are
  // covered in SyncPrefs unit tests.
  EXPECT_FALSE(
      pref_service_.GetDict(syncer::prefs::internal::kSelectedTypesPerAccount)
          .empty());
}

TEST_P(SyncToSigninMigrationTest, SyncPaused_AuthErrorResolved) {
  if (IsForceMigrationEnabled()) {
    // When the forced migration flag is enabled, there is no waiting for the
    // error to be resolved. The migration runs on the first attempt and that's
    // covered in SyncPaused_MinDelayNotPassed.
    return;
  }

  // Sync-the-feature is enabled but transport is "paused" due to a persistent
  // auth error. Simulate a first migration attempt that does nothing (see
  // SyncPaused_MinDelayNotPassed test).
  sync_service_.SetPersistentAuthError();
  ASSERT_EQ(sync_service_.GetTransportState(),
            syncer::SyncService::TransportState::PAUSED);
  ASSERT_TRUE(sync_service_.HasSyncConsent());
  ASSERT_TRUE(sync_service_.GetActiveDataTypes().empty());
  ASSERT_TRUE(
      pref_service_.GetDict(syncer::prefs::internal::kSelectedTypesPerAccount)
          .empty());
  const std::string gaia_id = sync_service_.GetAccountInfo().gaia;
  const std::string email = sync_service_.GetAccountInfo().email;
  RecordStateToPrefs();
  MaybeMigrateSyncingUserToSignedIn(fake_profile_dir_.GetPath(),
                                    &pref_service_);

  // Attempt the migration again with the auth error resolved.
  sync_service_.ClearAuthError();
  RecordStateToPrefs();
  MaybeMigrateSyncingUserToSignedIn(fake_profile_dir_.GetPath(),
                                    &pref_service_);

  // The migration should have run.
  EXPECT_EQ(pref_service_.GetString(prefs::kGoogleServicesAccountId), gaia_id);
  EXPECT_FALSE(pref_service_.GetBoolean(prefs::kGoogleServicesConsentedToSync));
  EXPECT_EQ(pref_service_.GetString(
                prefs::kGoogleServicesSyncingGaiaIdMigratedToSignedIn),
            gaia_id);
  EXPECT_EQ(pref_service_.GetString(
                prefs::kGoogleServicesSyncingUsernameMigratedToSignedIn),
            email);
  EXPECT_FALSE(
      pref_service_.GetDict(syncer::prefs::internal::kSelectedTypesPerAccount)
          .empty());
}

TEST_P(SyncToSigninMigrationTest, SyncInitializing) {
  // The user is signed in and opted in to Sync, but Sync is still initializing.
  sync_service_.SetMaxTransportState(
      syncer::SyncService::TransportState::INITIALIZING);
  ASSERT_TRUE(sync_service_.HasSyncConsent());

  // Save the above state to prefs.
  RecordStateToPrefs();

  // Take a copy of all current pref values, to verify whether the migration
  // modified any of them.
  const base::Value::Dict all_prefs =
      pref_service_.user_prefs_store()->GetValues();

  // Trigger the migration - it should only run in this state if the
  // force-migration is enabled.
  MaybeMigrateSyncingUserToSignedIn(fake_profile_dir_.GetPath(),
                                    &pref_service_);

  // Note that TestSyncService doesn't consume the prefs, so verify the prefs
  // directly here.
  if (IsForceMigrationEnabled()) {
    // There should be per-account selected types now. The details of this are
    // covered in SyncPrefs unit tests.
    EXPECT_FALSE(
        pref_service_.GetDict(syncer::prefs::internal::kSelectedTypesPerAccount)
            .empty());
  } else {
    // Since the migration didn't actually run, the prefs should be unmodified.
    EXPECT_EQ(pref_service_.user_prefs_store()->GetValues(), all_prefs);
  }
}

TEST_P(SyncToSigninMigrationTest, UndoFeaturePreventsMigration) {
  base::test::ScopedFeatureList undo_feature;
  undo_feature.InitAndEnableFeature(
      switches::kUndoMigrationOfSyncingUserToSignedIn);

  // Everything is active.
  ASSERT_EQ(sync_service_.GetTransportState(),
            syncer::SyncService::TransportState::ACTIVE);
  ASSERT_TRUE(sync_service_.HasSyncConsent());
  ASSERT_TRUE(sync_service_.GetActiveDataTypes().HasAll(
      {syncer::BOOKMARKS, syncer::PASSWORDS, syncer::READING_LIST}));

  // Save the above state to prefs.
  RecordStateToPrefs();

  // Take a copy of all current pref values, to verify that the migration
  // doesn't modify any of them.
  const base::Value::Dict all_prefs =
      pref_service_.user_prefs_store()->GetValues();

  base::HistogramTester histograms;

  // Trigger the migration.
  MaybeMigrateSyncingUserToSignedIn(fake_profile_dir_.GetPath(),
                                    &pref_service_);

  // Even though the user would be eligible, the "undo" feature should have
  // prevented the migration from happening. (And since there was nothing to
  // undo, it shouldn't have had any effect either.)
  EXPECT_EQ(pref_service_.user_prefs_store()->GetValues(), all_prefs);

  histograms.ExpectUniqueSample(
      "Sync.SyncToSigninMigrationDecision",
      /*SyncToSigninMigrationDecision::kUndoNotNecessary*/ 7, 1);
}

INSTANTIATE_TEST_SUITE_P(,
                         SyncToSigninMigrationTest,
                         testing::Bool(),
                         [](const testing::TestParamInfo<bool>& info) {
                           return info.param ? "ForceMigrationEnabled"
                                             : "ForceMigrationDisabled";
                         });

// Fixture for tests covering migration metrics. The test param determines
// whether the migration feature flag and possibly also the force-migration
// feature flag are enabled.
enum class FeatureState {
  kMigrationDisabled,
  kMigrationEnabled,
  kMigrationForced,
};
class SyncToSigninMigrationMetricsTest
    : public SyncToSigninMigrationTestBase,
      public testing::TestWithParam<FeatureState> {
 public:
  SyncToSigninMigrationMetricsTest()
      : SyncToSigninMigrationTestBase(
            /*migration_feature_enabled=*/IsMigrationEnabled(),
            /*force_migration_feature_enabled=*/IsForceMigrationEnabled()) {}

  bool IsMigrationEnabled() const {
    switch (GetParam()) {
      case FeatureState::kMigrationDisabled:
        return false;
      case FeatureState::kMigrationEnabled:
      case FeatureState::kMigrationForced:
        return true;
    }
  }
  bool IsForceMigrationEnabled() const {
    switch (GetParam()) {
      case FeatureState::kMigrationDisabled:
      case FeatureState::kMigrationEnabled:
        return false;
      case FeatureState::kMigrationForced:
        return true;
    }
  }

  std::string GetTypeDecisionHistogramInfix() const {
    return IsMigrationEnabled() ? "Migration" : "DryRun";
  }
};

TEST_P(SyncToSigninMigrationMetricsTest, SyncAndAllDataTypesActive) {
  // Everything is active.
  ASSERT_EQ(sync_service_.GetTransportState(),
            syncer::SyncService::TransportState::ACTIVE);
  ASSERT_TRUE(sync_service_.HasSyncConsent());
  ASSERT_TRUE(sync_service_.GetActiveDataTypes().HasAll(
      {syncer::BOOKMARKS, syncer::PASSWORDS, syncer::READING_LIST}));

  // Save the above state to prefs.
  RecordStateToPrefs();

  base::HistogramTester histograms;

  MaybeMigrateSyncingUserToSignedIn(fake_profile_dir_.GetPath(),
                                    &pref_service_);

  // The overall migration should run, except if the feature flag is disabled.
  int expected_decision =
      IsMigrationEnabled()
          ? /*SyncToSigninMigrationDecision::kMigrate*/ 0
          : /*SyncToSigninMigrationDecision::kDontMigrateFlagDisabled*/ 5;
  histograms.ExpectUniqueSample("Sync.SyncToSigninMigrationDecision",
                                expected_decision, 1);
  if (IsMigrationEnabled()) {
    histograms.ExpectTotalCount("Sync.SyncToSigninMigrationOutcome", 1);
    histograms.ExpectTotalCount("Sync.SyncToSigninMigrationTime", 1);
  } else {
    histograms.ExpectTotalCount("Sync.SyncToSigninMigrationOutcome", 0);
    histograms.ExpectTotalCount("Sync.SyncToSigninMigrationTime", 0);
  }
  // All the data type migrations should run - in "DryRun" mode if the feature
  // flag is disabled.
  std::string infix = GetTypeDecisionHistogramInfix();
  histograms.ExpectUniqueSample(
      "Sync.SyncToSigninMigrationDecision." + infix + ".BOOKMARK",
      /*SyncToSigninMigrationDataTypeDecision::kMigrate*/ 0, 1);
#if BUILDFLAG(IS_ANDROID)
  // PASSWORDS is migrated by other layers on Android.
  histograms.ExpectTotalCount(
      "Sync.SyncToSigninMigrationDecision." + infix + ".PASSWORD", 0);
#else
  histograms.ExpectUniqueSample(
      "Sync.SyncToSigninMigrationDecision." + infix + ".PASSWORD",
      /*SyncToSigninMigrationDataTypeDecision::kMigrate*/ 0, 1);
#endif
  histograms.ExpectUniqueSample(
      "Sync.SyncToSigninMigrationDecision." + infix + ".READING_LIST",
      /*SyncToSigninMigrationDataTypeDecision::kMigrate*/ 0, 1);
}

TEST_P(SyncToSigninMigrationMetricsTest, SyncActiveButNotDataTypes) {
  // Sync-the-feature is active.
  ASSERT_EQ(sync_service_.GetTransportState(),
            syncer::SyncService::TransportState::ACTIVE);
  ASSERT_TRUE(sync_service_.HasSyncConsent());

  // ReadingList is not selected.
  sync_service_.GetUserSettings()->SetSelectedTypes(
      /*sync_everything=*/false, {syncer::UserSelectableType::kBookmarks,
                                  syncer::UserSelectableType::kPasswords});
  // Passwords is selected, but failed to actually start up (e.g. disabled by
  // policy).
  sync_service_.SetFailedDataTypes({syncer::PASSWORDS});

  ASSERT_TRUE(sync_service_.GetActiveDataTypes().Has(syncer::BOOKMARKS));
  ASSERT_FALSE(sync_service_.GetActiveDataTypes().Has(syncer::PASSWORDS));
  ASSERT_FALSE(sync_service_.GetActiveDataTypes().Has(syncer::READING_LIST));

  // Save the above state to prefs.
  RecordStateToPrefs();

  base::HistogramTester histograms;

  MaybeMigrateSyncingUserToSignedIn(fake_profile_dir_.GetPath(),
                                    &pref_service_);

  // The overall migration should run, except if the feature flag is disabled.
  int expected_decision =
      IsMigrationEnabled()
          ? /*SyncToSigninMigrationDecision::kMigrate*/ 0
          : /*SyncToSigninMigrationDecision::kDontMigrateFlagDisabled*/ 5;
  histograms.ExpectUniqueSample("Sync.SyncToSigninMigrationDecision",
                                expected_decision, 1);
  if (IsMigrationEnabled()) {
    histograms.ExpectTotalCount("Sync.SyncToSigninMigrationOutcome", 1);
    histograms.ExpectTotalCount("Sync.SyncToSigninMigrationTime", 1);
  } else {
    histograms.ExpectTotalCount("Sync.SyncToSigninMigrationOutcome", 0);
    histograms.ExpectTotalCount("Sync.SyncToSigninMigrationTime", 0);
  }

  std::string infix = GetTypeDecisionHistogramInfix();
  // Bookmarks was active, so its migration should run.
  histograms.ExpectUniqueSample(
      "Sync.SyncToSigninMigrationDecision." + infix + ".BOOKMARK",
      /*SyncToSigninMigrationDataTypeDecision::kMigrate*/ 0, 1);
#if BUILDFLAG(IS_ANDROID)
  // PASSWORDS is migrated by other layers on Android.
  histograms.ExpectTotalCount(
      "Sync.SyncToSigninMigrationDecision." + infix + ".PASSWORD", 0);
#else
  // Passwords was not active, even though it was enabled.
  histograms.ExpectUniqueSample(
      "Sync.SyncToSigninMigrationDecision." + infix + ".PASSWORD",
      /*SyncToSigninMigrationDataTypeDecision::kDontMigrateTypeNotActive*/ 2,
      1);
#endif  // BUILDFLAG(IS_ANDROID)
  // ReadingList was disabled by the user.
  histograms.ExpectUniqueSample(
      "Sync.SyncToSigninMigrationDecision." + infix + ".READING_LIST",
      /*SyncToSigninMigrationDataTypeDecision::kDontMigrateTypeDisabled*/ 1, 1);
}

TEST_P(SyncToSigninMigrationMetricsTest, SyncStatusPrefsUnset) {
  // Everything is active.
  ASSERT_EQ(sync_service_.GetTransportState(),
            syncer::SyncService::TransportState::ACTIVE);
  ASSERT_TRUE(sync_service_.HasSyncConsent());
  ASSERT_TRUE(sync_service_.GetActiveDataTypes().HasAll(
      {syncer::BOOKMARKS, syncer::PASSWORDS, syncer::READING_LIST}));

  // Save the Sync configuration (enabled data types etc) to prefs, but not the
  // migration-specific status prefs. This simulates the case of an old client
  // which has never written those prefs.
  RecordStateToPrefs(/*include_status_recorder=*/false);

  base::HistogramTester histograms;

  MaybeMigrateSyncingUserToSignedIn(fake_profile_dir_.GetPath(),
                                    &pref_service_);

  // With the missing/undefined status, the overall migration should only run if
  // the force-migration flag was enabled.
  int expected_decision =
      IsForceMigrationEnabled()
          ? /*SyncToSigninMigrationDecision::kMigrateForced*/ 8
          : /*SyncToSigninMigrationDecision::kDontMigrateSyncStatusUndefined*/
          3;
  histograms.ExpectUniqueSample("Sync.SyncToSigninMigrationDecision",
                                expected_decision, 1);
  histograms.ExpectTotalCount("Sync.SyncToSigninMigrationOutcome",
                              IsForceMigrationEnabled() ? 1 : 0);
  histograms.ExpectTotalCount("Sync.SyncToSigninMigrationTime",
                              IsForceMigrationEnabled() ? 1 : 0);

  histograms.ExpectTotalCount(
      "Sync.SyncToSigninMigrationDecision.DryRun.BOOKMARK", 0);
  histograms.ExpectTotalCount(
      "Sync.SyncToSigninMigrationDecision.DryRun.PASSWORD", 0);
  histograms.ExpectTotalCount(
      "Sync.SyncToSigninMigrationDecision.DryRun.READING_LIST", 0);
  if (IsForceMigrationEnabled()) {
    // The individual data types were not active and so should not be migrated.
    histograms.ExpectUniqueSample(
        "Sync.SyncToSigninMigrationDecision.Migration.BOOKMARK",
        /*SyncToSigninMigrationDataTypeDecision::kDontMigrateTypeNotActive*/ 2,
        1);
#if !BUILDFLAG(IS_ANDROID)
    histograms.ExpectUniqueSample(
        "Sync.SyncToSigninMigrationDecision.Migration.PASSWORD",
        /*SyncToSigninMigrationDataTypeDecision::kDontMigrateTypeNotActive*/ 2,
        1);
#endif
    histograms.ExpectUniqueSample(
        "Sync.SyncToSigninMigrationDecision.Migration.READING_LIST",
        /*SyncToSigninMigrationDataTypeDecision::kDontMigrateTypeNotActive*/ 2,
        1);
  } else {
    // The overall migration didn't run.
    histograms.ExpectTotalCount(
        "Sync.SyncToSigninMigrationDecision.Migration.BOOKMARK", 0);
    histograms.ExpectTotalCount(
        "Sync.SyncToSigninMigrationDecision.Migration.PASSWORD", 0);
    histograms.ExpectTotalCount(
        "Sync.SyncToSigninMigrationDecision.Migration.READING_LIST", 0);
  }
}

TEST_P(SyncToSigninMigrationMetricsTest, NotSignedIn) {
  // There's no signed-in user.
  sync_service_.SetSignedOut();
  ASSERT_TRUE(sync_service_.GetActiveDataTypes().empty());

  // Save the above state to prefs.
  RecordStateToPrefs();

  base::HistogramTester histograms;

  MaybeMigrateSyncingUserToSignedIn(fake_profile_dir_.GetPath(),
                                    &pref_service_);

  // The migration should not run since there's no signed-in user.
  histograms.ExpectUniqueSample(
      "Sync.SyncToSigninMigrationDecision",
      /*SyncToSigninMigrationDecision::kDontMigrateNotSignedIn*/ 1, 1);
  histograms.ExpectTotalCount("Sync.SyncToSigninMigrationOutcome", 0);
  histograms.ExpectTotalCount("Sync.SyncToSigninMigrationTime", 0);
  histograms.ExpectTotalCount(
      "Sync.SyncToSigninMigrationDecision.DryRun.BOOKMARK", 0);
  histograms.ExpectTotalCount(
      "Sync.SyncToSigninMigrationDecision.DryRun.PASSWORD", 0);
  histograms.ExpectTotalCount(
      "Sync.SyncToSigninMigrationDecision.DryRun.READING_LIST", 0);
  histograms.ExpectTotalCount(
      "Sync.SyncToSigninMigrationDecision.Migration.BOOKMARK", 0);
  histograms.ExpectTotalCount(
      "Sync.SyncToSigninMigrationDecision.Migration.PASSWORD", 0);
  histograms.ExpectTotalCount(
      "Sync.SyncToSigninMigrationDecision.Migration.READING_LIST", 0);
}

TEST_P(SyncToSigninMigrationMetricsTest, SyncTransport) {
  // There's no Sync consent, but otherwise everything is active (running in
  // transport mode).
  sync_service_.SetSignedIn(signin::ConsentLevel::kSignin);
  ASSERT_EQ(sync_service_.GetTransportState(),
            syncer::SyncService::TransportState::ACTIVE);
  ASSERT_TRUE(sync_service_.GetActiveDataTypes().HasAll(
      {syncer::BOOKMARKS, syncer::PASSWORDS, syncer::READING_LIST}));

  // Save the above state to prefs.
  RecordStateToPrefs();

  base::HistogramTester histograms;

  MaybeMigrateSyncingUserToSignedIn(fake_profile_dir_.GetPath(),
                                    &pref_service_);

  // The migration should not run since this is not a Sync-the-feature user.
  histograms.ExpectUniqueSample(
      "Sync.SyncToSigninMigrationDecision",
      /*SyncToSigninMigrationDecision::kDontMigrateNotSyncing*/ 2, 1);
  histograms.ExpectTotalCount("Sync.SyncToSigninMigrationOutcome", 0);
  histograms.ExpectTotalCount("Sync.SyncToSigninMigrationTime", 0);
  histograms.ExpectTotalCount(
      "Sync.SyncToSigninMigrationDecision.DryRun.BOOKMARK", 0);
  histograms.ExpectTotalCount(
      "Sync.SyncToSigninMigrationDecision.DryRun.PASSWORD", 0);
  histograms.ExpectTotalCount(
      "Sync.SyncToSigninMigrationDecision.DryRun.READING_LIST", 0);
  histograms.ExpectTotalCount(
      "Sync.SyncToSigninMigrationDecision.Migration.BOOKMARK", 0);
  histograms.ExpectTotalCount(
      "Sync.SyncToSigninMigrationDecision.Migration.PASSWORD", 0);
  histograms.ExpectTotalCount(
      "Sync.SyncToSigninMigrationDecision.Migration.READING_LIST", 0);
}

TEST_P(SyncToSigninMigrationMetricsTest, SyncPaused_MinDelayNotPassed) {
  sync_service_.SetPersistentAuthError();
  ASSERT_EQ(sync_service_.GetTransportState(),
            syncer::SyncService::TransportState::PAUSED);
  ASSERT_TRUE(sync_service_.HasSyncConsent());
  ASSERT_TRUE(sync_service_.GetActiveDataTypes().empty());
  RecordStateToPrefs();
  base::HistogramTester histograms;

  MaybeMigrateSyncingUserToSignedIn(fake_profile_dir_.GetPath(),
                                    &pref_service_);

  std::string infix = GetTypeDecisionHistogramInfix();
  if (IsForceMigrationEnabled()) {
    // Enabling the forced migration flag causes the min delay requirement to be
    // ignored, immediately moving the user to the signed-in state. Individual
    // data types were not active and so should not be migrated.
    histograms.ExpectUniqueSample(
        "Sync.SyncToSigninMigrationDecision",
        /*SyncToSigninMigrationDecision::kMigrateForced*/ 8, 1);
    histograms.ExpectTotalCount("Sync.SyncToSigninMigrationTime", 1);
    histograms.ExpectUniqueSample(
        "Sync.SyncToSigninMigrationDecision." + infix + ".BOOKMARK",
        /*SyncToSigninMigrationDataTypeDecision::kDontMigrateTypeNotActive*/ 2,
        1);
#if BUILDFLAG(IS_ANDROID)
    // PASSWORDS is migrated by other layers on Android.
    histograms.ExpectTotalCount(
        "Sync.SyncToSigninMigrationDecision." + infix + ".PASSWORD", 0);
#else
    histograms.ExpectUniqueSample(
        "Sync.SyncToSigninMigrationDecision." + infix + ".PASSWORD",
        /*SyncToSigninMigrationDataTypeDecision::kDontMigrateTypeNotActive*/ 2,
        1);
#endif  // BUILDFLAG(IS_ANDROID)
    histograms.ExpectUniqueSample(
        "Sync.SyncToSigninMigrationDecision." + infix + ".READING_LIST",
        /*SyncToSigninMigrationDataTypeDecision::kDontMigrateTypeNotActive*/ 2,
        1);
  } else if (IsMigrationEnabled()) {
    // The migration should not run because not enough time passed since the
    // auth error was detected. There's still a chance the user will resolve it.
    histograms.ExpectUniqueSample(
        "Sync.SyncToSigninMigrationDecision",
        /*SyncToSigninMigrationDecision::kDontMigrateAuthError*/ 9, 1);
    histograms.ExpectTotalCount("Sync.SyncToSigninMigrationTime", 0);
    histograms.ExpectTotalCount(
        "Sync.SyncToSigninMigrationDecision." + infix + ".BOOKMARK", 0);
    histograms.ExpectTotalCount(
        "Sync.SyncToSigninMigrationDecision." + infix + ".PASSWORD", 0);
    histograms.ExpectTotalCount(
        "Sync.SyncToSigninMigrationDecision." + infix + ".READING_LIST", 0);
  } else {
    // The migration should not run because the flag is disabled. The per type
    // metrics are still recorded for historical reasons.
    histograms.ExpectUniqueSample(
        "Sync.SyncToSigninMigrationDecision",
        /*SyncToSigninMigrationDecision::kDontMigrateFlagDisabled*/ 5, 1);
    histograms.ExpectTotalCount("Sync.SyncToSigninMigrationTime", 0);
    histograms.ExpectUniqueSample(
        "Sync.SyncToSigninMigrationDecision." + infix + ".BOOKMARK",
        /*SyncToSigninMigrationDataTypeDecision::kDontMigrateTypeNotActive*/ 2,
        1);
#if BUILDFLAG(IS_ANDROID)
    // PASSWORDS is migrated by other layers on Android.
    histograms.ExpectTotalCount(
        "Sync.SyncToSigninMigrationDecision." + infix + ".PASSWORD", 0);
#else
    histograms.ExpectUniqueSample(
        "Sync.SyncToSigninMigrationDecision." + infix + ".PASSWORD",
        /*SyncToSigninMigrationDataTypeDecision::kDontMigrateTypeNotActive*/ 2,
        1);
#endif  // BUILDFLAG(IS_ANDROID)
    histograms.ExpectUniqueSample(
        "Sync.SyncToSigninMigrationDecision." + infix + ".READING_LIST",
        /*SyncToSigninMigrationDataTypeDecision::kDontMigrateTypeNotActive*/ 2,
        1);
  }
}

TEST_P(SyncToSigninMigrationMetricsTest, SyncPaused_MinDelayPassed) {
  if (GetParam() != FeatureState::kMigrationEnabled) {
    // For kMigrationForced, the duration of the auth error is irrelevant,
    // the migration succeeds on the first attempt and that's covered in
    // SyncPaused_MinDelayNotPassed.
    // For kMigrationDisabled, waiting won't change anything, the second attempt
    // would fail just like the first one, as in SyncPaused_MinDelayNotPassed.
    return;
  }

  // Simulate a first migration attempt while sync-the-feature is enabled but
  // transport is "paused" due to a persistent auth error. The first attempt
  // does nothing (see SyncPaused_MinDelayNotPassed test).
  sync_service_.SetPersistentAuthError();
  ASSERT_EQ(sync_service_.GetTransportState(),
            syncer::SyncService::TransportState::PAUSED);
  ASSERT_TRUE(sync_service_.HasSyncConsent());
  ASSERT_TRUE(sync_service_.GetActiveDataTypes().empty());
  RecordStateToPrefs();
  MaybeMigrateSyncingUserToSignedIn(fake_profile_dir_.GetPath(),
                                    &pref_service_);
  base::HistogramTester histograms;

  // Now, enough time has passed and the migration is attempted again.
  FastForwardBy(switches::kMinDelayToMigrateSyncPaused.Get());
  MaybeMigrateSyncingUserToSignedIn(fake_profile_dir_.GetPath(),
                                    &pref_service_);

  // The overall migration should run, except if the feature flag is disabled.
  int expected_decision =
      IsMigrationEnabled()
          ? /*SyncToSigninMigrationDecision::kMigrate*/ 0
          : /*SyncToSigninMigrationDecision::kDontMigrateFlagDisabled*/ 5;
  histograms.ExpectUniqueSample("Sync.SyncToSigninMigrationDecision",
                                expected_decision, 1);
  if (IsMigrationEnabled()) {
    histograms.ExpectTotalCount("Sync.SyncToSigninMigrationOutcome", 1);
    histograms.ExpectTotalCount("Sync.SyncToSigninMigrationTime", 1);
  } else {
    histograms.ExpectTotalCount("Sync.SyncToSigninMigrationOutcome", 0);
    histograms.ExpectTotalCount("Sync.SyncToSigninMigrationTime", 0);
  }

  // However, the individual data types were by definition not active and so
  // should not be migrated.
  std::string infix = GetTypeDecisionHistogramInfix();
  histograms.ExpectUniqueSample(
      "Sync.SyncToSigninMigrationDecision." + infix + ".BOOKMARK",
      /*SyncToSigninMigrationDataTypeDecision::kDontMigrateTypeNotActive*/ 2,
      1);
#if BUILDFLAG(IS_ANDROID)
  // PASSWORDS is migrated by other layers on Android.
  histograms.ExpectTotalCount(
      "Sync.SyncToSigninMigrationDecision." + infix + ".PASSWORD", 0);
#else
  histograms.ExpectUniqueSample(
      "Sync.SyncToSigninMigrationDecision." + infix + ".PASSWORD",
      /*SyncToSigninMigrationDataTypeDecision::kDontMigrateTypeNotActive*/ 2,
      1);
#endif  // BUILDFLAG(IS_ANDROID)
  histograms.ExpectUniqueSample(
      "Sync.SyncToSigninMigrationDecision." + infix + ".READING_LIST",
      /*SyncToSigninMigrationDataTypeDecision::kDontMigrateTypeNotActive*/ 2,
      1);
}

TEST_P(SyncToSigninMigrationMetricsTest, SyncPaused_AuthErrorResolved) {
  if (GetParam() != FeatureState::kMigrationEnabled) {
    // For kMigrationForced, the duration of the auth error is irrelevant,
    // the migration succeeds on the first attempt and that's covered in
    // SyncPaused_MinDelayNotPassed.
    // For kMigrationDisabled, waiting won't change anything, the second attempt
    // would fail just like the first one, as in SyncPaused_MinDelayNotPassed.
    return;
  }

  // Sync-the-feature is enabled but transport is "paused" due to a persistent
  // auth error. Simulate a first migration attempt that does nothing (see
  // SyncPaused_MinDelayNotPassed test). After that, the error is resolved.
  sync_service_.SetPersistentAuthError();
  ASSERT_EQ(sync_service_.GetTransportState(),
            syncer::SyncService::TransportState::PAUSED);
  ASSERT_TRUE(sync_service_.HasSyncConsent());
  ASSERT_TRUE(sync_service_.GetActiveDataTypes().empty());
  RecordStateToPrefs();
  MaybeMigrateSyncingUserToSignedIn(fake_profile_dir_.GetPath(),
                                    &pref_service_);
  sync_service_.ClearAuthError();
  RecordStateToPrefs();
  base::HistogramTester histograms;

  // Attempt the migration again with the auth error resolved.
  MaybeMigrateSyncingUserToSignedIn(fake_profile_dir_.GetPath(),
                                    &pref_service_);

  // The migration should run.
  std::string infix = GetTypeDecisionHistogramInfix();
  histograms.ExpectUniqueSample("Sync.SyncToSigninMigrationDecision",
                                /*SyncToSigninMigrationDecision::kMigrate*/ 0,
                                1);
  histograms.ExpectTotalCount("Sync.SyncToSigninMigrationTime", 1);
  histograms.ExpectUniqueSample(
      "Sync.SyncToSigninMigrationDecision." + infix + ".BOOKMARK",
      /*SyncToSigninMigrationDataTypeDecision::kMigrate*/ 0, 1);
#if BUILDFLAG(IS_ANDROID)
  // PASSWORDS is migrated by other layers on Android.
  histograms.ExpectTotalCount(
      "Sync.SyncToSigninMigrationDecision." + infix + ".PASSWORD", 0);
#else
  histograms.ExpectUniqueSample(
      "Sync.SyncToSigninMigrationDecision." + infix + ".PASSWORD",
      /*SyncToSigninMigrationDataTypeDecision::kMigrate*/ 0, 1);
#endif  // BUILDFLAG(IS_ANDROID)
  histograms.ExpectUniqueSample(
      "Sync.SyncToSigninMigrationDecision." + infix + ".READING_LIST",
      /*SyncToSigninMigrationDataTypeDecision::kMigrate*/ 0, 1);
}

TEST_P(SyncToSigninMigrationMetricsTest, SyncInitializing) {
  sync_service_.SetMaxTransportState(
      syncer::SyncService::TransportState::INITIALIZING);
  ASSERT_TRUE(sync_service_.HasSyncConsent());
  ASSERT_TRUE(sync_service_.GetActiveDataTypes().empty());

  // Save the above state to prefs.
  RecordStateToPrefs();

  base::HistogramTester histograms;

  MaybeMigrateSyncingUserToSignedIn(fake_profile_dir_.GetPath(),
                                    &pref_service_);

  // If Sync was still initializing, the overall migration should only run if
  // the force-migration flag was enabled.
  int expected_decision =
      IsForceMigrationEnabled()
          ? /*SyncToSigninMigrationDecision::kMigrateForced*/ 8
          : /*SyncToSigninMigrationDecision::kDontMigrateSyncStatusInitializing*/
          4;
  histograms.ExpectUniqueSample("Sync.SyncToSigninMigrationDecision",
                                expected_decision, 1);
  histograms.ExpectTotalCount("Sync.SyncToSigninMigrationOutcome",
                              IsForceMigrationEnabled() ? 1 : 0);
  histograms.ExpectTotalCount("Sync.SyncToSigninMigrationTime",
                              IsForceMigrationEnabled() ? 1 : 0);

  histograms.ExpectTotalCount(
      "Sync.SyncToSigninMigrationDecision.DryRun.BOOKMARK", 0);
  histograms.ExpectTotalCount(
      "Sync.SyncToSigninMigrationDecision.DryRun.PASSWORD", 0);
  histograms.ExpectTotalCount(
      "Sync.SyncToSigninMigrationDecision.DryRun.READING_LIST", 0);
  if (IsForceMigrationEnabled()) {
    // The individual data types were not active and so should not be migrated.
    histograms.ExpectUniqueSample(
        "Sync.SyncToSigninMigrationDecision.Migration.BOOKMARK",
        /*SyncToSigninMigrationDataTypeDecision::kDontMigrateTypeNotActive*/ 2,
        1);
#if !BUILDFLAG(IS_ANDROID)
    histograms.ExpectUniqueSample(
        "Sync.SyncToSigninMigrationDecision.Migration.PASSWORD",
        /*SyncToSigninMigrationDataTypeDecision::kDontMigrateTypeNotActive*/ 2,
        1);
#endif
    histograms.ExpectUniqueSample(
        "Sync.SyncToSigninMigrationDecision.Migration.READING_LIST",
        /*SyncToSigninMigrationDataTypeDecision::kDontMigrateTypeNotActive*/ 2,
        1);
  } else {
    // The overall migration didn't run.
    histograms.ExpectTotalCount(
        "Sync.SyncToSigninMigrationDecision.Migration.BOOKMARK", 0);
    histograms.ExpectTotalCount(
        "Sync.SyncToSigninMigrationDecision.Migration.PASSWORD", 0);
    histograms.ExpectTotalCount(
        "Sync.SyncToSigninMigrationDecision.Migration.READING_LIST", 0);
  }
}

INSTANTIATE_TEST_SUITE_P(,
                         SyncToSigninMigrationMetricsTest,
                         testing::ValuesIn({FeatureState::kMigrationDisabled,
                                            FeatureState::kMigrationEnabled,
                                            FeatureState::kMigrationForced}),
                         [](const testing::TestParamInfo<FeatureState>& info) {
                           switch (info.param) {
                             case FeatureState::kMigrationDisabled:
                               return "MigrationDisabled";
                             case FeatureState::kMigrationEnabled:
                               return "MigrationEnabled";
                             case FeatureState::kMigrationForced:
                               return "MigrationForced";
                           }
                           return "";
                         });

class SyncToSigninMigrationDataTypesTest : public SyncToSigninMigrationTestBase,
                                           public testing::Test {
 public:
  SyncToSigninMigrationDataTypesTest()
      : SyncToSigninMigrationTestBase(
            /*migration_feature_enabled=*/true,
            /*force_migration_feature_enabled=*/false) {}

  void SetUp() override {
    // Everything is active.
    ASSERT_EQ(sync_service_.GetTransportState(),
              syncer::SyncService::TransportState::ACTIVE);
    ASSERT_TRUE(sync_service_.HasSyncConsent());
    ASSERT_TRUE(sync_service_.GetActiveDataTypes().HasAll(
        {syncer::BOOKMARKS, syncer::PASSWORDS, syncer::READING_LIST}));

    // Save the above state to prefs.
    RecordStateToPrefs();
  }

  base::FilePath GetBookmarksLocalStorePath() const {
    return fake_profile_dir_.GetPath().AppendASCII("Bookmarks");
  }
  base::FilePath GetBookmarksAccountStorePath() const {
    return fake_profile_dir_.GetPath().AppendASCII("AccountBookmarks");
  }

  base::FilePath GetPasswordsLocalStorePath() const {
    return fake_profile_dir_.GetPath().AppendASCII("Login Data");
  }
  base::FilePath GetPasswordsAccountStorePath() const {
    return fake_profile_dir_.GetPath().AppendASCII("Login Data For Account");
  }
};

TEST_F(SyncToSigninMigrationDataTypesTest, MoveBookmarks_BothExist) {
  // Both bookmark stores exist on disk. The account store is empty, since it
  // was unused pre-migration. This is the typical pre-migration state.
  base::WriteFile(GetBookmarksLocalStorePath(), "local bookmarks");
  base::WriteFile(GetBookmarksAccountStorePath(), "");

  base::HistogramTester histograms;

  MaybeMigrateSyncingUserToSignedIn(fake_profile_dir_.GetPath(),
                                    &pref_service_);

  // The local file should have been moved over the account one.
  EXPECT_FALSE(base::PathExists(GetBookmarksLocalStorePath()));
  EXPECT_TRUE(base::PathExists(GetBookmarksAccountStorePath()));

  std::string account_contents;
  ASSERT_TRUE(base::ReadFileToString(GetBookmarksAccountStorePath(),
                                     &account_contents));
  EXPECT_EQ(account_contents, "local bookmarks");

  histograms.ExpectUniqueSample(
      "Sync.SyncToSigninMigrationOutcome.BookmarksFileMove",
      -base::File::FILE_OK, 1);
}

TEST_F(SyncToSigninMigrationDataTypesTest, MoveBookmarks_OnlyLocalExists) {
  // Only the local store exists on disk; the account store doesn't. This is
  // uncommon, but could happen upgrades directly from an old Chrome version
  // that didn't have an account store yet.
  base::WriteFile(GetBookmarksLocalStorePath(), "local bookmarks");

  base::HistogramTester histograms;

  MaybeMigrateSyncingUserToSignedIn(fake_profile_dir_.GetPath(),
                                    &pref_service_);

  // The local file should have been renamed to the account one.
  EXPECT_FALSE(base::PathExists(GetBookmarksLocalStorePath()));
  EXPECT_TRUE(base::PathExists(GetBookmarksAccountStorePath()));

  std::string account_contents;
  ASSERT_TRUE(base::ReadFileToString(GetBookmarksAccountStorePath(),
                                     &account_contents));
  EXPECT_EQ(account_contents, "local bookmarks");

  histograms.ExpectUniqueSample(
      "Sync.SyncToSigninMigrationOutcome.BookmarksFileMove",
      -base::File::FILE_OK, 1);
}

TEST_F(SyncToSigninMigrationDataTypesTest, MoveBookmarks_OnlyAccountExists) {
  // Only the account store exists on disk; the local store doesn't. This
  // should be impossible in practice, except maybe in rare error cases.
  base::WriteFile(GetBookmarksAccountStorePath(), "account bookmarks");

  base::HistogramTester histograms;

  MaybeMigrateSyncingUserToSignedIn(fake_profile_dir_.GetPath(),
                                    &pref_service_);

  // The migration shouldn't have done anything; the account store should still
  // exist with the same contents.
  EXPECT_FALSE(base::PathExists(GetBookmarksLocalStorePath()));
  EXPECT_TRUE(base::PathExists(GetBookmarksAccountStorePath()));

  std::string account_contents;
  ASSERT_TRUE(base::ReadFileToString(GetBookmarksAccountStorePath(),
                                     &account_contents));
  EXPECT_EQ(account_contents, "account bookmarks");

  histograms.ExpectUniqueSample(
      "Sync.SyncToSigninMigrationOutcome.BookmarksFileMove",
      -base::File::FILE_ERROR_NOT_FOUND, 1);
}

TEST_F(SyncToSigninMigrationDataTypesTest, MoveBookmarks_NoneExists) {
  // Neither of the two stores exist on disk. This should be impossible in
  // practice, except maybe in rare error cases.

  base::HistogramTester histograms;

  MaybeMigrateSyncingUserToSignedIn(fake_profile_dir_.GetPath(),
                                    &pref_service_);

  // The migration shouldn't have done anything; still neither of the stores
  // should exist.
  EXPECT_FALSE(base::PathExists(GetBookmarksLocalStorePath()));
  EXPECT_FALSE(base::PathExists(GetBookmarksAccountStorePath()));

  histograms.ExpectUniqueSample(
      "Sync.SyncToSigninMigrationOutcome.BookmarksFileMove",
      -base::File::FILE_ERROR_NOT_FOUND, 1);
}

#if BUILDFLAG(IS_POSIX)
TEST_F(SyncToSigninMigrationDataTypesTest, MoveBookmarks_FolderNotWritable) {
  // Both bookmark stores exist on disk. The account store is empty, since it
  // was unused pre-migration. This is the typical pre-migration state.
  base::WriteFile(GetBookmarksLocalStorePath(), "local bookmarks");
  base::WriteFile(GetBookmarksAccountStorePath(), "");
  // However, the folder containing the files is (for some reason) not writable,
  // so the move/rename can't actually happen. This should not happen in
  // practice (if it does, Chrome will likely be very broken). This test mostly
  // verifies that nothing catastrophic happens, e.g. no crash.
  int mode = 0;
  ASSERT_TRUE(
      base::GetPosixFilePermissions(fake_profile_dir_.GetPath(), &mode));
  mode &= ~base::FILE_PERMISSION_WRITE_BY_USER;
  mode &= ~base::FILE_PERMISSION_WRITE_BY_GROUP;
  mode &= ~base::FILE_PERMISSION_WRITE_BY_OTHERS;
  ASSERT_TRUE(base::SetPosixFilePermissions(fake_profile_dir_.GetPath(), mode));

  base::HistogramTester histograms;

  MaybeMigrateSyncingUserToSignedIn(fake_profile_dir_.GetPath(),
                                    &pref_service_);

  // Nothing should have changed.
  EXPECT_TRUE(base::PathExists(GetBookmarksLocalStorePath()));
  EXPECT_TRUE(base::PathExists(GetBookmarksAccountStorePath()));

  std::string local_contents;
  ASSERT_TRUE(
      base::ReadFileToString(GetBookmarksLocalStorePath(), &local_contents));
  EXPECT_EQ(local_contents, "local bookmarks");
  std::string account_contents;
  ASSERT_TRUE(base::ReadFileToString(GetBookmarksAccountStorePath(),
                                     &account_contents));
  EXPECT_EQ(account_contents, "");

  histograms.ExpectUniqueSample(
      "Sync.SyncToSigninMigrationOutcome.BookmarksFileMove",
      -base::File::FILE_ERROR_ACCESS_DENIED, 1);
}
#endif  // BUILDFLAG(IS_POSIX)

#if BUILDFLAG(IS_ANDROID)
TEST_F(SyncToSigninMigrationDataTypesTest, MovePasswords_NoMoveOnAndroid) {
  base::WriteFile(GetPasswordsLocalStorePath(), "local passwords");
  base::WriteFile(GetPasswordsAccountStorePath(), "account passwords");
  base::HistogramTester histogram_tester;

  MaybeMigrateSyncingUserToSignedIn(fake_profile_dir_.GetPath(),
                                    &pref_service_);

  // The files should be unchanged.
  std::string local_contents;
  std::string account_contents;
  ASSERT_TRUE(
      base::ReadFileToString(GetPasswordsLocalStorePath(), &local_contents));
  ASSERT_TRUE(base::ReadFileToString(GetPasswordsAccountStorePath(),
                                     &account_contents));
  EXPECT_EQ(local_contents, "local passwords");
  EXPECT_EQ(account_contents, "account passwords");
  histogram_tester.ExpectTotalCount(
      "Sync.SyncToSigninMigrationOutcome.PasswordsFileMove", 0);
}
#else
TEST_F(SyncToSigninMigrationDataTypesTest, MovePasswords_BothExist) {
  // Both password stores exist on disk. The account store is empty, since it
  // was unused pre-migration. This is the typical pre-migration state.
  base::WriteFile(GetPasswordsLocalStorePath(), "local passwords");
  base::WriteFile(GetPasswordsAccountStorePath(), "");

  base::HistogramTester histograms;

  MaybeMigrateSyncingUserToSignedIn(fake_profile_dir_.GetPath(),
                                    &pref_service_);

  // The local file should have been moved over the account one.
  EXPECT_FALSE(base::PathExists(GetPasswordsLocalStorePath()));
  EXPECT_TRUE(base::PathExists(GetPasswordsAccountStorePath()));

  std::string account_contents;
  ASSERT_TRUE(base::ReadFileToString(GetPasswordsAccountStorePath(),
                                     &account_contents));
  EXPECT_EQ(account_contents, "local passwords");

  histograms.ExpectUniqueSample(
      "Sync.SyncToSigninMigrationOutcome.PasswordsFileMove",
      -base::File::FILE_OK, 1);
}

TEST_F(SyncToSigninMigrationDataTypesTest, MovePasswords_OnlyLocalExists) {
  // Only the local store exists on disk; the account store doesn't. This is
  // uncommon, but could happen upgrades directly from an old Chrome version
  // that didn't have an account store yet.
  base::WriteFile(GetPasswordsLocalStorePath(), "local passwords");

  base::HistogramTester histograms;

  MaybeMigrateSyncingUserToSignedIn(fake_profile_dir_.GetPath(),
                                    &pref_service_);

  // The local file should have been renamed to the account one.
  EXPECT_FALSE(base::PathExists(GetPasswordsLocalStorePath()));
  EXPECT_TRUE(base::PathExists(GetPasswordsAccountStorePath()));

  std::string account_contents;
  ASSERT_TRUE(base::ReadFileToString(GetPasswordsAccountStorePath(),
                                     &account_contents));
  EXPECT_EQ(account_contents, "local passwords");

  histograms.ExpectUniqueSample(
      "Sync.SyncToSigninMigrationOutcome.PasswordsFileMove",
      -base::File::FILE_OK, 1);
}

TEST_F(SyncToSigninMigrationDataTypesTest, MovePasswords_OnlyAccountExists) {
  // Only the account store exists on disk; the local store doesn't. This
  // should be impossible in practice, except maybe in rare error cases.
  base::WriteFile(GetPasswordsAccountStorePath(), "account passwords");

  base::HistogramTester histograms;

  MaybeMigrateSyncingUserToSignedIn(fake_profile_dir_.GetPath(),
                                    &pref_service_);

  // The migration shouldn't have done anything; the account store should still
  // exist with the same contents.
  EXPECT_FALSE(base::PathExists(GetPasswordsLocalStorePath()));
  EXPECT_TRUE(base::PathExists(GetPasswordsAccountStorePath()));

  std::string account_contents;
  ASSERT_TRUE(base::ReadFileToString(GetPasswordsAccountStorePath(),
                                     &account_contents));
  EXPECT_EQ(account_contents, "account passwords");

  histograms.ExpectUniqueSample(
      "Sync.SyncToSigninMigrationOutcome.PasswordsFileMove",
      -base::File::FILE_ERROR_NOT_FOUND, 1);
}

TEST_F(SyncToSigninMigrationDataTypesTest, MovePasswords_NoneExists) {
  // Neither of the two stores exist on disk. This should be impossible in
  // practice, except maybe in rare error cases.

  base::HistogramTester histograms;

  MaybeMigrateSyncingUserToSignedIn(fake_profile_dir_.GetPath(),
                                    &pref_service_);

  // The migration shouldn't have done anything; still neither of the stores
  // should exist.
  EXPECT_FALSE(base::PathExists(GetPasswordsLocalStorePath()));
  EXPECT_FALSE(base::PathExists(GetPasswordsAccountStorePath()));

  histograms.ExpectUniqueSample(
      "Sync.SyncToSigninMigrationOutcome.PasswordsFileMove",
      -base::File::FILE_ERROR_NOT_FOUND, 1);
}

#if BUILDFLAG(IS_POSIX)
TEST_F(SyncToSigninMigrationDataTypesTest, MovePasswords_FolderNotWritable) {
  // Both password stores exist on disk. The account store is empty, since it
  // was unused pre-migration. This is the typical pre-migration state.
  base::WriteFile(GetPasswordsLocalStorePath(), "local passwords");
  base::WriteFile(GetPasswordsAccountStorePath(), "");
  // However, the folder containing the files is (for some reason) not writable,
  // so the move/rename can't actually happen. This should not happen in
  // practice (if it does, Chrome will likely be very broken). This test mostly
  // verifies that nothing catastrophic happens, e.g. no crash.
  int mode = 0;
  ASSERT_TRUE(
      base::GetPosixFilePermissions(fake_profile_dir_.GetPath(), &mode));
  mode &= ~base::FILE_PERMISSION_WRITE_BY_USER;
  mode &= ~base::FILE_PERMISSION_WRITE_BY_GROUP;
  mode &= ~base::FILE_PERMISSION_WRITE_BY_OTHERS;
  ASSERT_TRUE(base::SetPosixFilePermissions(fake_profile_dir_.GetPath(), mode));

  base::HistogramTester histograms;

  MaybeMigrateSyncingUserToSignedIn(fake_profile_dir_.GetPath(),
                                    &pref_service_);

  // Nothing should have changed.
  EXPECT_TRUE(base::PathExists(GetPasswordsLocalStorePath()));
  EXPECT_TRUE(base::PathExists(GetPasswordsAccountStorePath()));

  std::string local_contents;
  ASSERT_TRUE(
      base::ReadFileToString(GetPasswordsLocalStorePath(), &local_contents));
  EXPECT_EQ(local_contents, "local passwords");
  std::string account_contents;
  ASSERT_TRUE(base::ReadFileToString(GetPasswordsAccountStorePath(),
                                     &account_contents));
  EXPECT_EQ(account_contents, "");

  histograms.ExpectUniqueSample(
      "Sync.SyncToSigninMigrationOutcome.PasswordsFileMove",
      -base::File::FILE_ERROR_ACCESS_DENIED, 1);
}
#endif  // BUILDFLAG(IS_POSIX)
#endif  // BUILDFLAG(IS_ANDROID)

// A test fixture that performs the SyncToSignin migration, then enables the
// "undo migration" feature.The test param determines whether the
// force-migration feature flag is enabled or not (the regular migration is
// always enabled in this test).
class SyncToSigninMigrationUndoTest : public SyncToSigninMigrationTestBase,
                                      public testing::TestWithParam<bool> {
 public:
  SyncToSigninMigrationUndoTest()
      : SyncToSigninMigrationTestBase(
            /*migration_feature_enabled=*/true,
            /*force_migration_feature_enabled=*/IsForceMigrationEnabled()) {}

  bool IsForceMigrationEnabled() const { return GetParam(); }

  void SetUp() override {
    // Everything is active.
    ASSERT_EQ(sync_service_.GetTransportState(),
              syncer::SyncService::TransportState::ACTIVE);
    ASSERT_TRUE(sync_service_.HasSyncConsent());
    ASSERT_TRUE(sync_service_.GetActiveDataTypes().HasAll(
        {syncer::BOOKMARKS, syncer::PASSWORDS, syncer::READING_LIST}));

    // Save the above state to prefs.
    RecordStateToPrefs();

    // Run the migration, so that there is something to undo.
    MaybeMigrateSyncingUserToSignedIn(fake_profile_dir_.GetPath(),
                                      &pref_service_);

    undo_feature_.InitAndEnableFeature(
        switches::kUndoMigrationOfSyncingUserToSignedIn);
  }

 private:
  base::test::ScopedFeatureList undo_feature_;
};

TEST_P(SyncToSigninMigrationUndoTest, UndoesMigration) {
  // The user is in the migrated state - signed-in:
  ASSERT_FALSE(
      pref_service_.GetString(prefs::kGoogleServicesAccountId).empty());
  ASSERT_EQ(pref_service_.GetString(prefs::kGoogleServicesAccountId),
            sync_service_.GetAccountInfo().gaia);
  // Not syncing:
  ASSERT_FALSE(pref_service_.GetBoolean(prefs::kGoogleServicesConsentedToSync));
  ASSERT_TRUE(
      pref_service_.GetString(prefs::kGoogleServicesLastSyncingGaiaId).empty());
  ASSERT_TRUE(pref_service_.GetString(prefs::kGoogleServicesLastSyncingUsername)
                  .empty());
#if !BUILDFLAG(IS_CHROMEOS_ASH)
  ASSERT_FALSE(sync_prefs_->IsInitialSyncFeatureSetupComplete());
#endif
  // Marked as "migrated":
  ASSERT_EQ(pref_service_.GetString(
                prefs::kGoogleServicesSyncingGaiaIdMigratedToSignedIn),
            sync_service_.GetAccountInfo().gaia);
  ASSERT_EQ(pref_service_.GetString(
                prefs::kGoogleServicesSyncingUsernameMigratedToSignedIn),
            sync_service_.GetAccountInfo().email);

  // Trigger the "undo" migration.
  MaybeMigrateSyncingUserToSignedIn(fake_profile_dir_.GetPath(),
                                    &pref_service_);

  // The migration should've been undone, and the user should be back in the
  // "syncing" state.
  ASSERT_FALSE(
      pref_service_.GetString(prefs::kGoogleServicesAccountId).empty());
  EXPECT_TRUE(pref_service_.GetBoolean(prefs::kGoogleServicesConsentedToSync));
  EXPECT_TRUE(sync_prefs_->IsInitialSyncFeatureSetupComplete());
  // The "last syncing user" prefs should also have been restored.
  EXPECT_EQ(pref_service_.GetString(prefs::kGoogleServicesLastSyncingGaiaId),
            sync_service_.GetAccountInfo().gaia);
  EXPECT_EQ(pref_service_.GetString(prefs::kGoogleServicesLastSyncingUsername),
            sync_service_.GetAccountInfo().email);
  // And the "was migrated" prefs should've been cleared.
  EXPECT_TRUE(
      pref_service_
          .GetString(prefs::kGoogleServicesSyncingGaiaIdMigratedToSignedIn)
          .empty());
  EXPECT_TRUE(
      pref_service_
          .GetString(prefs::kGoogleServicesSyncingUsernameMigratedToSignedIn)
          .empty());
}

TEST_P(SyncToSigninMigrationUndoTest, Idempotent) {
  // Trigger the "undo" migration.
  MaybeMigrateSyncingUserToSignedIn(fake_profile_dir_.GetPath(),
                                    &pref_service_);

  // The user is now back in the "syncing" state.
  ASSERT_FALSE(
      pref_service_.GetString(prefs::kGoogleServicesAccountId).empty());
  ASSERT_TRUE(pref_service_.GetBoolean(prefs::kGoogleServicesConsentedToSync));
  ASSERT_TRUE(
      pref_service_
          .GetString(prefs::kGoogleServicesSyncingGaiaIdMigratedToSignedIn)
          .empty());

  // Take a copy of all current pref values, to verify that the second undo
  // attempt doesn't modify any of them.
  const base::Value::Dict all_prefs =
      pref_service_.user_prefs_store()->GetValues();

  // Trigger the (undo) migration again - it should have no further effect.
  MaybeMigrateSyncingUserToSignedIn(fake_profile_dir_.GetPath(),
                                    &pref_service_);

  // The prefs should be unmodified.
  EXPECT_EQ(pref_service_.user_prefs_store()->GetValues(), all_prefs);
}

TEST_P(SyncToSigninMigrationUndoTest, DoesNotUndoMigrationIfSignedOut) {
  // The user is in the "migrated" state - signed-in, not syncing, marked as
  // migrated.
  ASSERT_FALSE(
      pref_service_.GetString(prefs::kGoogleServicesAccountId).empty());
  ASSERT_FALSE(pref_service_.GetBoolean(prefs::kGoogleServicesConsentedToSync));
  ASSERT_FALSE(
      pref_service_
          .GetString(prefs::kGoogleServicesSyncingGaiaIdMigratedToSignedIn)
          .empty());

  // The account gets signed out.
  pref_service_.ClearPref(prefs::kGoogleServicesAccountId);

  // Trigger the "undo" migration.
  MaybeMigrateSyncingUserToSignedIn(fake_profile_dir_.GetPath(),
                                    &pref_service_);

  // The migration should NOT have been undone, since the account isn't signed
  // in anymore.
  ASSERT_TRUE(pref_service_.GetString(prefs::kGoogleServicesAccountId).empty());
  EXPECT_FALSE(pref_service_.GetBoolean(prefs::kGoogleServicesConsentedToSync));
}

TEST_P(SyncToSigninMigrationUndoTest, DoesNotUndoMigrationIfDiffentAccount) {
  // The user is in the "migrated" state - signed-in, not syncing, marked as
  // migrated.
  ASSERT_FALSE(
      pref_service_.GetString(prefs::kGoogleServicesAccountId).empty());
  ASSERT_FALSE(pref_service_.GetBoolean(prefs::kGoogleServicesConsentedToSync));
  ASSERT_FALSE(
      pref_service_
          .GetString(prefs::kGoogleServicesSyncingGaiaIdMigratedToSignedIn)
          .empty());

  // The account gets signed out, and a different account signed in.
  pref_service_.SetString(prefs::kGoogleServicesAccountId, "different_gaia");
  ASSERT_NE(pref_service_.GetString(prefs::kGoogleServicesAccountId),
            pref_service_.GetString(
                prefs::kGoogleServicesSyncingGaiaIdMigratedToSignedIn));

  // Trigger the "undo" migration.
  MaybeMigrateSyncingUserToSignedIn(fake_profile_dir_.GetPath(),
                                    &pref_service_);

  // The migration should NOT have been undone, since a different account is
  // signed in now.
  ASSERT_EQ(pref_service_.GetString(prefs::kGoogleServicesAccountId),
            "different_gaia");
  EXPECT_FALSE(pref_service_.GetBoolean(prefs::kGoogleServicesConsentedToSync));
}

INSTANTIATE_TEST_SUITE_P(,
                         SyncToSigninMigrationUndoTest,
                         testing::Bool(),
                         [](const testing::TestParamInfo<bool>& info) {
                           return info.param ? "ForceMigrationEnabled"
                                             : "ForceMigrationDisabled";
                         });

}  // namespace
}  // namespace browser_sync
