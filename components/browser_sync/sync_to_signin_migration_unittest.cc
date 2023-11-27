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
#include "components/prefs/testing_pref_service.h"
#include "components/signin/public/base/signin_pref_names.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/sync/base/model_type.h"
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
  explicit SyncToSigninMigrationTestBase(bool migration_feature_enabled) {
    if (migration_feature_enabled) {
      features_.InitAndEnableFeature(kMigrateSyncingUserToSignedIn);
    } else {
      features_.InitAndDisableFeature(kMigrateSyncingUserToSignedIn);
    }

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

 private:
  base::test::ScopedFeatureList features_;
  base::test::SingleThreadTaskEnvironment task_environment_;

 protected:
  TestingPrefServiceSimple pref_service_;
  std::unique_ptr<syncer::SyncPrefs> sync_prefs_;
  syncer::TestSyncService sync_service_;
  base::ScopedTempDir fake_profile_dir_;
};

class SyncToSigninMigrationTest : public SyncToSigninMigrationTestBase,
                                  public testing::Test {
 public:
  SyncToSigninMigrationTest()
      : SyncToSigninMigrationTestBase(
            /*migration_feature_enabled=*/true) {}
};

TEST_F(SyncToSigninMigrationTest, SyncActive) {
  // Sync is active.
  ASSERT_EQ(sync_service_.GetTransportState(),
            syncer::SyncService::TransportState::ACTIVE);
  ASSERT_TRUE(sync_service_.HasSyncConsent());

  const std::string gaia_id = sync_service_.GetAccountInfo().gaia;
  const std::string email = sync_service_.GetAccountInfo().email;

  // Save the above state to prefs.
  RecordStateToPrefs();

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

TEST_F(SyncToSigninMigrationTest, SyncStatusPrefsUnset) {
  // Everything is active.
  ASSERT_EQ(sync_service_.GetTransportState(),
            syncer::SyncService::TransportState::ACTIVE);
  ASSERT_TRUE(sync_service_.HasSyncConsent());

  // Save the Sync configuration (enabled data types etc) to prefs, but not the
  // migration-specific status prefs. This simulates the case of an old client
  // which has never written those prefs.
  RecordStateToPrefs(/*include_status_recorder=*/false);

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

TEST_F(SyncToSigninMigrationTest, SyncTransport) {
  // There's no Sync consent, but otherwise everything is active (running in
  // transport mode).
  sync_service_.SetHasSyncConsent(false);
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

TEST_F(SyncToSigninMigrationTest, SyncDisabledByPolicy) {
  // The user is signed in and opted in to Sync, but Sync is disabled via
  // enterprise policy.
  sync_service_.SetDisableReasons(
      {syncer::SyncService::DISABLE_REASON_ENTERPRISE_POLICY});
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

TEST_F(SyncToSigninMigrationTest, SyncPaused) {
  // Sync-the-feature is enabled, but in the "paused" state due to a persistent
  // auth error.
  sync_service_.SetPersistentAuthError();
  ASSERT_EQ(sync_service_.GetTransportState(),
            syncer::SyncService::TransportState::PAUSED);
  ASSERT_TRUE(sync_service_.HasSyncConsent());
  ASSERT_TRUE(sync_service_.GetActiveDataTypes().Empty());

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

TEST_F(SyncToSigninMigrationTest, SyncInitializing) {
  // The user is signed in and opted in to Sync, but Sync is still initializing.
  sync_service_.SetTransportState(
      syncer::SyncService::TransportState::INITIALIZING);
  ASSERT_TRUE(sync_service_.HasSyncConsent());

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

// Fixture for tests covering migration metrics. The test param determines
// whether the feature flag is enabled or not.
class SyncToSigninMigrationMetricsTest : public SyncToSigninMigrationTestBase,
                                         public testing::TestWithParam<bool> {
 public:
  SyncToSigninMigrationMetricsTest()
      : SyncToSigninMigrationTestBase(
            /*migration_feature_enabled=*/GetParam()) {}

  bool IsMigrationEnabled() const { return GetParam(); }

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
  // All the data type migrations should run - in "DryRun" mode if the feature
  // flag is disabled.
  std::string infix = GetTypeDecisionHistogramInfix();
  histograms.ExpectUniqueSample(
      "Sync.SyncToSigninMigrationDecision." + infix + ".BOOKMARK",
      /*SyncToSigninMigrationDataTypeDecision::kMigrate*/ 0, 1);
  histograms.ExpectUniqueSample(
      "Sync.SyncToSigninMigrationDecision." + infix + ".PASSWORD",
      /*SyncToSigninMigrationDataTypeDecision::kMigrate*/ 0, 1);
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

  std::string infix = GetTypeDecisionHistogramInfix();
  // Bookmarks was active, so its migration should run.
  histograms.ExpectUniqueSample(
      "Sync.SyncToSigninMigrationDecision." + infix + ".BOOKMARK",
      /*SyncToSigninMigrationDataTypeDecision::kMigrate*/ 0, 1);
  // Passwords was not active, even though it was enabled.
  histograms.ExpectUniqueSample(
      "Sync.SyncToSigninMigrationDecision." + infix + ".PASSWORD",
      /*SyncToSigninMigrationDataTypeDecision::kDontMigrateTypeNotActive*/ 2,
      1);
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

  // The migration should not run due to the missing/undefined status.
  histograms.ExpectUniqueSample(
      "Sync.SyncToSigninMigrationDecision",
      /*SyncToSigninMigrationDecision::kDontMigrateSyncStatusUndefined*/ 3, 1);
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

TEST_P(SyncToSigninMigrationMetricsTest, NotSignedIn) {
  // There's no signed-in user.
  sync_service_.SetAccountInfo(CoreAccountInfo());
  sync_service_.SetHasSyncConsent(false);
  sync_service_.SetTransportState(
      syncer::SyncService::TransportState::DISABLED);
  ASSERT_TRUE(sync_service_.GetActiveDataTypes().Empty());

  // Save the above state to prefs.
  RecordStateToPrefs();

  base::HistogramTester histograms;

  MaybeMigrateSyncingUserToSignedIn(fake_profile_dir_.GetPath(),
                                    &pref_service_);

  // The migration should not run since there's no signed-in user.
  histograms.ExpectUniqueSample(
      "Sync.SyncToSigninMigrationDecision",
      /*SyncToSigninMigrationDecision::kDontMigrateNotSignedIn*/ 1, 1);
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
  sync_service_.SetHasSyncConsent(false);
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

TEST_P(SyncToSigninMigrationMetricsTest, SyncPaused) {
  sync_service_.SetPersistentAuthError();
  ASSERT_EQ(sync_service_.GetTransportState(),
            syncer::SyncService::TransportState::PAUSED);
  ASSERT_TRUE(sync_service_.HasSyncConsent());
  ASSERT_TRUE(sync_service_.GetActiveDataTypes().Empty());

  // Save the above state to prefs.
  RecordStateToPrefs();

  base::HistogramTester histograms;

  MaybeMigrateSyncingUserToSignedIn(fake_profile_dir_.GetPath(),
                                    &pref_service_);

  // In the Sync-paused state, the overall migration should run, except if the
  // feature flag is disabled.
  int expected_decision =
      IsMigrationEnabled()
          ? /*SyncToSigninMigrationDecision::kMigrate*/ 0
          : /*SyncToSigninMigrationDecision::kDontMigrateFlagDisabled*/ 5;
  histograms.ExpectUniqueSample("Sync.SyncToSigninMigrationDecision",
                                expected_decision, 1);

  // However, the individual data types were by definition not active and so
  // should not be migrated.
  std::string infix = GetTypeDecisionHistogramInfix();
  histograms.ExpectUniqueSample(
      "Sync.SyncToSigninMigrationDecision." + infix + ".BOOKMARK",
      /*SyncToSigninMigrationDataTypeDecision::kDontMigrateTypeNotActive*/ 2,
      1);
  histograms.ExpectUniqueSample(
      "Sync.SyncToSigninMigrationDecision." + infix + ".PASSWORD",
      /*SyncToSigninMigrationDataTypeDecision::kDontMigrateTypeNotActive*/ 2,
      1);
  histograms.ExpectUniqueSample(
      "Sync.SyncToSigninMigrationDecision." + infix + ".READING_LIST",
      /*SyncToSigninMigrationDataTypeDecision::kDontMigrateTypeNotActive*/ 2,
      1);
}

TEST_P(SyncToSigninMigrationMetricsTest, SyncInitializing) {
  sync_service_.SetTransportState(
      syncer::SyncService::TransportState::INITIALIZING);
  ASSERT_TRUE(sync_service_.HasSyncConsent());
  ASSERT_TRUE(sync_service_.GetActiveDataTypes().Empty());

  // Save the above state to prefs.
  RecordStateToPrefs();

  base::HistogramTester histograms;

  MaybeMigrateSyncingUserToSignedIn(fake_profile_dir_.GetPath(),
                                    &pref_service_);

  // The migration should not run, because Sync was still initializing.
  histograms.ExpectUniqueSample(
      "Sync.SyncToSigninMigrationDecision",
      /*SyncToSigninMigrationDecision::kDontMigrateSyncStatusInitializing*/ 4,
      1);
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

INSTANTIATE_TEST_SUITE_P(,
                         SyncToSigninMigrationMetricsTest,
                         testing::Bool(),
                         [](const testing::TestParamInfo<bool>& info) {
                           return info.param ? "MigrationEnabled"
                                             : "MigrationDisabled";
                         });

class SyncToSigninMigrationDataTypesTest : public SyncToSigninMigrationTestBase,
                                           public testing::Test {
 public:
  SyncToSigninMigrationDataTypesTest()
      : SyncToSigninMigrationTestBase(
            /*migration_feature_enabled=*/true) {}

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

// The Bookmarks migration isn't implemented on platforms other than iOS yet.
#if BUILDFLAG(IS_IOS)
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
#endif  // BUILDFLAG(IS_IOS)

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

}  // namespace
}  // namespace browser_sync
