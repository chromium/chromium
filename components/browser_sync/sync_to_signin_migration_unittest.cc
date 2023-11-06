// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/browser_sync/sync_to_signin_migration.h"

#include <memory>
#include <string>

#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "components/prefs/testing_pref_service.h"
#include "components/signin/public/base/signin_pref_names.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/sync/base/model_type.h"
#include "components/sync/base/user_selectable_type.h"
#include "components/sync/service/sync_feature_status_for_migrations_recorder.h"
#include "components/sync/service/sync_prefs.h"
#include "components/sync/test/test_sync_service.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace browser_sync {
namespace {

class SyncToSigninMigrationTest : public testing::Test {
 public:
  SyncToSigninMigrationTest() {
    // TODO(crbug.com/1486420): Add tests for the feature-enabled case, once
    // that is implemented.
    features_.InitAndDisableFeature(kMigrateSyncingUserToSignedIn);

    signin::IdentityManager::RegisterProfilePrefs(pref_service_.registry());
    syncer::SyncPrefs::RegisterProfilePrefs(pref_service_.registry());

    sync_prefs_ = std::make_unique<syncer::SyncPrefs>(&pref_service_);
  }
  ~SyncToSigninMigrationTest() override = default;

  void RecordStateToPrefs(bool include_status_recorder = true) {
    // Populate signin prefs based on the state of the TestSyncService.
    pref_service_.SetString(prefs::kGoogleServicesAccountId,
                            sync_service_.GetAccountInfo().gaia);
    pref_service_.SetBoolean(prefs::kGoogleServicesConsentedToSync,
                             sync_service_.HasSyncConsent());

    // Populate sync prefs. The TestSyncService doesn't write these, so they
    // have to be set manually here.
    syncer::SyncUserSettings* settings = sync_service_.GetUserSettings();
    sync_prefs_->SetSelectedTypes(settings->IsSyncEverythingEnabled(),
                                  settings->GetRegisteredSelectableTypes(),
                                  settings->GetSelectedTypes());
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
};

TEST_F(SyncToSigninMigrationTest, SyncAndAllDataTypesActive) {
  // Everything is active.
  ASSERT_EQ(sync_service_.GetTransportState(),
            syncer::SyncService::TransportState::ACTIVE);
  ASSERT_TRUE(sync_service_.HasSyncConsent());
  ASSERT_TRUE(sync_service_.GetActiveDataTypes().HasAll(
      {syncer::BOOKMARKS, syncer::PASSWORDS, syncer::READING_LIST}));

  // Save the above state to prefs.
  RecordStateToPrefs();

  base::HistogramTester histograms;

  MaybeMigrateSyncingUserToSignedIn(&pref_service_);

  // The overall migration should run, except that the feature flag is disabled.
  histograms.ExpectUniqueSample(
      "Sync.SyncToSigninMigrationDecision",
      /*SyncToSigninMigrationDecision::kDontMigrateFlagDisabled*/ 5, 1);
  // All the data type migrations should run - "DryRun" because the feature flag
  // is disabled.
  histograms.ExpectUniqueSample(
      "Sync.SyncToSigninMigrationDecision.DryRun.BOOKMARK",
      /*SyncToSigninMigrationDataTypeDecision::kMigrate*/ 0, 1);
  histograms.ExpectUniqueSample(
      "Sync.SyncToSigninMigrationDecision.DryRun.PASSWORD",
      /*SyncToSigninMigrationDataTypeDecision::kMigrate*/ 0, 1);
  histograms.ExpectUniqueSample(
      "Sync.SyncToSigninMigrationDecision.DryRun.READING_LIST",
      /*SyncToSigninMigrationDataTypeDecision::kMigrate*/ 0, 1);
}

TEST_F(SyncToSigninMigrationTest, SyncActiveButNotDataTypes) {
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

  MaybeMigrateSyncingUserToSignedIn(&pref_service_);

  // The overall migration should run, except that the feature flag is disabled.
  histograms.ExpectUniqueSample(
      "Sync.SyncToSigninMigrationDecision",
      /*SyncToSigninMigrationDecision::kDontMigrateFlagDisabled*/ 5, 1);
  // Bookmarks was active, so its migration should run.
  histograms.ExpectUniqueSample(
      "Sync.SyncToSigninMigrationDecision.DryRun.BOOKMARK",
      /*SyncToSigninMigrationDataTypeDecision::kMigrate*/ 0, 1);
  // Passwords was not active, even though it was enabled.
  histograms.ExpectUniqueSample(
      "Sync.SyncToSigninMigrationDecision.DryRun.PASSWORD",
      /*SyncToSigninMigrationDataTypeDecision::kDontMigrateTypeNotActive*/ 2,
      1);
  // ReadingList was disabled by the user.
  histograms.ExpectUniqueSample(
      "Sync.SyncToSigninMigrationDecision.DryRun.READING_LIST",
      /*SyncToSigninMigrationDataTypeDecision::kDontMigrateTypeDisabled*/ 1, 1);
}

TEST_F(SyncToSigninMigrationTest, SyncStatusPrefsUnset) {
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

  MaybeMigrateSyncingUserToSignedIn(&pref_service_);

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
}

TEST_F(SyncToSigninMigrationTest, NotSignedIn) {
  // There's no signed-in user.
  sync_service_.SetAccountInfo(CoreAccountInfo());
  sync_service_.SetHasSyncConsent(false);
  sync_service_.SetTransportState(
      syncer::SyncService::TransportState::DISABLED);
  ASSERT_TRUE(sync_service_.GetActiveDataTypes().Empty());

  // Save the above state to prefs.
  RecordStateToPrefs();

  base::HistogramTester histograms;

  MaybeMigrateSyncingUserToSignedIn(&pref_service_);

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
}

TEST_F(SyncToSigninMigrationTest, SyncTransport) {
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

  MaybeMigrateSyncingUserToSignedIn(&pref_service_);

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
}

TEST_F(SyncToSigninMigrationTest, SyncPaused) {
  sync_service_.SetPersistentAuthError();
  ASSERT_EQ(sync_service_.GetTransportState(),
            syncer::SyncService::TransportState::PAUSED);
  ASSERT_TRUE(sync_service_.HasSyncConsent());
  ASSERT_TRUE(sync_service_.GetActiveDataTypes().Empty());

  // Save the above state to prefs.
  RecordStateToPrefs();

  base::HistogramTester histograms;

  MaybeMigrateSyncingUserToSignedIn(&pref_service_);

  // In the Sync-paused state, the overall migration should run, except that the
  // feature flag is disabled.
  histograms.ExpectUniqueSample(
      "Sync.SyncToSigninMigrationDecision",
      /*SyncToSigninMigrationDecision::kDontMigrateFlagDisabled*/ 5, 1);
  // However, the individual data types were by definition not active and so
  // should not be migrated.
  histograms.ExpectUniqueSample(
      "Sync.SyncToSigninMigrationDecision.DryRun.BOOKMARK",
      /*SyncToSigninMigrationDataTypeDecision::kDontMigrateTypeNotActive*/ 2,
      1);
  histograms.ExpectUniqueSample(
      "Sync.SyncToSigninMigrationDecision.DryRun.PASSWORD",
      /*SyncToSigninMigrationDataTypeDecision::kDontMigrateTypeNotActive*/ 2,
      1);
  histograms.ExpectUniqueSample(
      "Sync.SyncToSigninMigrationDecision.DryRun.READING_LIST",
      /*SyncToSigninMigrationDataTypeDecision::kDontMigrateTypeNotActive*/ 2,
      1);
}

TEST_F(SyncToSigninMigrationTest, SyncInitializing) {
  sync_service_.SetTransportState(
      syncer::SyncService::TransportState::INITIALIZING);
  ASSERT_TRUE(sync_service_.HasSyncConsent());
  ASSERT_TRUE(sync_service_.GetActiveDataTypes().Empty());

  // Save the above state to prefs.
  RecordStateToPrefs();

  base::HistogramTester histograms;

  MaybeMigrateSyncingUserToSignedIn(&pref_service_);

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
}

}  // namespace
}  // namespace browser_sync
