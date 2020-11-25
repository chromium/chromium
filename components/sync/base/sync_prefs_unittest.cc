// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/base/sync_prefs.h"

#include <memory>

#include "base/command_line.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "build/chromeos_buildflags.h"
#include "components/prefs/pref_notifier_impl.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_value_store.h"
#include "components/prefs/testing_pref_service.h"
#include "components/sync/base/pref_names.h"
#include "components/sync/base/user_selectable_type.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace syncer {

namespace {

using ::testing::InSequence;
using ::testing::StrictMock;

// Obsolete pref that used to store if sync should be prevented from
// automatically starting up. This is now replaced by its inverse
// kSyncRequested.
const char kSyncSuppressStart[] = "sync.suppress_start";

class SyncPrefsTest : public testing::Test {
 protected:
  SyncPrefsTest() {
    SyncPrefs::RegisterProfilePrefs(pref_service_.registry());
    sync_prefs_ = std::make_unique<SyncPrefs>(&pref_service_);
  }

  base::test::SingleThreadTaskEnvironment task_environment_;
  TestingPrefServiceSimple pref_service_;
  std::unique_ptr<SyncPrefs> sync_prefs_;
};

// Verify that invalidation versions are persisted and loaded correctly.
TEST_F(SyncPrefsTest, InvalidationVersions) {
  std::map<ModelType, int64_t> versions;
  versions[BOOKMARKS] = 10;
  versions[SESSIONS] = 20;
  versions[PREFERENCES] = 30;

  sync_prefs_->UpdateInvalidationVersions(versions);

  std::map<ModelType, int64_t> versions2 =
      sync_prefs_->GetInvalidationVersions();

  EXPECT_EQ(versions.size(), versions2.size());
  for (auto map_iter : versions2) {
    EXPECT_EQ(versions[map_iter.first], map_iter.second);
  }
}

TEST_F(SyncPrefsTest, PollInterval) {
  EXPECT_TRUE(sync_prefs_->GetPollInterval().is_zero());

  sync_prefs_->SetPollInterval(base::TimeDelta::FromMinutes(30));

  EXPECT_FALSE(sync_prefs_->GetPollInterval().is_zero());
  EXPECT_EQ(sync_prefs_->GetPollInterval().InMinutes(), 30);
}

class MockSyncPrefObserver : public SyncPrefObserver {
 public:
  MOCK_METHOD(void, OnSyncManagedPrefChange, (bool), (override));
  MOCK_METHOD(void, OnFirstSetupCompletePrefChange, (bool), (override));
  MOCK_METHOD(void, OnSyncRequestedPrefChange, (bool), (override));
  MOCK_METHOD(void, OnPreferredDataTypesPrefChange, (), (override));
};

TEST_F(SyncPrefsTest, ObservedPrefs) {
  StrictMock<MockSyncPrefObserver> mock_sync_pref_observer;
  InSequence dummy;
  EXPECT_CALL(mock_sync_pref_observer, OnSyncManagedPrefChange(true));
  EXPECT_CALL(mock_sync_pref_observer, OnSyncManagedPrefChange(false));
  EXPECT_CALL(mock_sync_pref_observer, OnFirstSetupCompletePrefChange(true));
  EXPECT_CALL(mock_sync_pref_observer, OnFirstSetupCompletePrefChange(false));
  EXPECT_CALL(mock_sync_pref_observer, OnSyncRequestedPrefChange(true));
  EXPECT_CALL(mock_sync_pref_observer, OnSyncRequestedPrefChange(false));

  ASSERT_FALSE(sync_prefs_->IsManaged());
  ASSERT_FALSE(sync_prefs_->IsFirstSetupComplete());
  ASSERT_FALSE(sync_prefs_->IsSyncRequested());

  sync_prefs_->AddSyncPrefObserver(&mock_sync_pref_observer);

  sync_prefs_->SetManagedForTest(true);
  EXPECT_TRUE(sync_prefs_->IsManaged());
  sync_prefs_->SetManagedForTest(false);
  EXPECT_FALSE(sync_prefs_->IsManaged());

  sync_prefs_->SetFirstSetupComplete();
  EXPECT_TRUE(sync_prefs_->IsFirstSetupComplete());
  sync_prefs_->ClearFirstSetupComplete();
  EXPECT_FALSE(sync_prefs_->IsFirstSetupComplete());

  sync_prefs_->SetSyncRequested(true);
  EXPECT_TRUE(sync_prefs_->IsSyncRequested());
  sync_prefs_->SetSyncRequested(false);
  EXPECT_FALSE(sync_prefs_->IsSyncRequested());

  sync_prefs_->RemoveSyncPrefObserver(&mock_sync_pref_observer);
}

#if BUILDFLAG(IS_CHROMEOS_ASH)
TEST_F(SyncPrefsTest, SetSelectedOsTypesTriggersPreferredDataTypesPrefChange) {
  StrictMock<MockSyncPrefObserver> mock_sync_pref_observer;
  EXPECT_CALL(mock_sync_pref_observer, OnPreferredDataTypesPrefChange());

  sync_prefs_->AddSyncPrefObserver(&mock_sync_pref_observer);
  sync_prefs_->SetSelectedOsTypes(/*sync_all_os_types=*/false,
                                  UserSelectableOsTypeSet(),
                                  UserSelectableOsTypeSet());
  sync_prefs_->RemoveSyncPrefObserver(&mock_sync_pref_observer);
}
#endif

TEST_F(SyncPrefsTest, ClearLocalSyncTransportData) {
  ASSERT_FALSE(sync_prefs_->IsFirstSetupComplete());
  ASSERT_EQ(base::Time(), sync_prefs_->GetLastSyncedTime());
  ASSERT_TRUE(sync_prefs_->GetEncryptionBootstrapToken().empty());

  sync_prefs_->SetFirstSetupComplete();
  sync_prefs_->SetLastSyncedTime(base::Time::Now());
  sync_prefs_->SetEncryptionBootstrapToken("explicit_passphrase_token");
  sync_prefs_->SetKeystoreEncryptionBootstrapToken("keystore_token");

  ASSERT_TRUE(sync_prefs_->IsFirstSetupComplete());
  ASSERT_NE(base::Time(), sync_prefs_->GetLastSyncedTime());
  ASSERT_EQ("explicit_passphrase_token",
            sync_prefs_->GetEncryptionBootstrapToken());
  ASSERT_EQ("keystore_token",
            sync_prefs_->GetKeystoreEncryptionBootstrapToken());

  sync_prefs_->ClearLocalSyncTransportData();

  EXPECT_EQ(base::Time(), sync_prefs_->GetLastSyncedTime());
  EXPECT_TRUE(sync_prefs_->GetKeystoreEncryptionBootstrapToken().empty());

  // User-entered field should not have been cleared.
  EXPECT_TRUE(sync_prefs_->IsFirstSetupComplete());
  EXPECT_EQ("explicit_passphrase_token",
            sync_prefs_->GetEncryptionBootstrapToken());
}

TEST_F(SyncPrefsTest, Basic) {
  EXPECT_FALSE(sync_prefs_->IsFirstSetupComplete());
  sync_prefs_->SetFirstSetupComplete();
  EXPECT_TRUE(sync_prefs_->IsFirstSetupComplete());

  EXPECT_FALSE(sync_prefs_->IsSyncRequested());
  sync_prefs_->SetSyncRequested(true);
  EXPECT_TRUE(sync_prefs_->IsSyncRequested());
  sync_prefs_->SetSyncRequested(false);
  EXPECT_FALSE(sync_prefs_->IsSyncRequested());

  EXPECT_EQ(base::Time(), sync_prefs_->GetLastSyncedTime());
  const base::Time& now = base::Time::Now();
  sync_prefs_->SetLastSyncedTime(now);
  EXPECT_EQ(now, sync_prefs_->GetLastSyncedTime());

  EXPECT_TRUE(sync_prefs_->HasKeepEverythingSynced());
  sync_prefs_->SetSelectedTypes(
      /*keep_everything_synced=*/false,
      /*registered_types=*/UserSelectableTypeSet::All(),
      /*selected_types=*/UserSelectableTypeSet::All());
  EXPECT_FALSE(sync_prefs_->HasKeepEverythingSynced());
  sync_prefs_->SetSelectedTypes(
      /*keep_everything_synced=*/true,
      /*registered_types=*/UserSelectableTypeSet::All(),
      /*selected_types=*/UserSelectableTypeSet());
  EXPECT_TRUE(sync_prefs_->HasKeepEverythingSynced());

  EXPECT_TRUE(sync_prefs_->GetEncryptionBootstrapToken().empty());
  sync_prefs_->SetEncryptionBootstrapToken("token");
  EXPECT_EQ("token", sync_prefs_->GetEncryptionBootstrapToken());
}

TEST_F(SyncPrefsTest, SelectedTypesKeepEverythingSynced) {
  ASSERT_TRUE(sync_prefs_->HasKeepEverythingSynced());

  EXPECT_EQ(UserSelectableTypeSet::All(), sync_prefs_->GetSelectedTypes());
  for (UserSelectableType type : UserSelectableTypeSet::All()) {
    sync_prefs_->SetSelectedTypes(
        /*keep_everything_synced=*/true,
        /*registered_types=*/UserSelectableTypeSet::All(),
        /*selected_types=*/{type});
    EXPECT_EQ(UserSelectableTypeSet::All(), sync_prefs_->GetSelectedTypes());
  }
}

TEST_F(SyncPrefsTest, SelectedTypesKeepEverythingSyncedButPolicyRestricted) {
  ASSERT_TRUE(sync_prefs_->HasKeepEverythingSynced());
  pref_service_.SetManagedPref(prefs::kSyncPreferences,
                               std::make_unique<base::Value>(false));

  UserSelectableTypeSet expected_type_set = UserSelectableTypeSet::All();
  expected_type_set.Remove(UserSelectableType::kPreferences);
  EXPECT_EQ(expected_type_set, sync_prefs_->GetSelectedTypes());
}

TEST_F(SyncPrefsTest, SelectedTypesNotKeepEverythingSynced) {
  sync_prefs_->SetSelectedTypes(
      /*keep_everything_synced=*/false,
      /*registered_types=*/UserSelectableTypeSet::All(),
      /*selected_types=*/UserSelectableTypeSet());

  ASSERT_NE(UserSelectableTypeSet::All(), sync_prefs_->GetSelectedTypes());
  for (UserSelectableType type : UserSelectableTypeSet::All()) {
    sync_prefs_->SetSelectedTypes(
        /*keep_everything_synced=*/false,
        /*registered_types=*/UserSelectableTypeSet::All(),
        /*selected_types=*/{type});
    EXPECT_EQ(UserSelectableTypeSet{type}, sync_prefs_->GetSelectedTypes());
  }
}

TEST_F(SyncPrefsTest, SelectedTypesNotKeepEverythingSyncedAndPolicyRestricted) {
  pref_service_.SetManagedPref(prefs::kSyncPreferences,
                               std::make_unique<base::Value>(false));
  sync_prefs_->SetSelectedTypes(
      /*keep_everything_synced=*/false,
      /*registered_types=*/UserSelectableTypeSet::All(),
      /*selected_types=*/UserSelectableTypeSet());

  ASSERT_FALSE(
      sync_prefs_->GetSelectedTypes().Has(UserSelectableType::kPreferences));
  for (UserSelectableType type : UserSelectableTypeSet::All()) {
    sync_prefs_->SetSelectedTypes(
        /*keep_everything_synced=*/false,
        /*registered_types=*/UserSelectableTypeSet::All(),
        /*selected_types=*/{type});
    UserSelectableTypeSet expected_type_set = UserSelectableTypeSet{type};
    expected_type_set.Remove(UserSelectableType::kPreferences);
    EXPECT_EQ(expected_type_set, sync_prefs_->GetSelectedTypes());
  }
}

#if BUILDFLAG(IS_CHROMEOS_ASH)
TEST_F(SyncPrefsTest, IsSyncAllOsTypesEnabled) {
  EXPECT_TRUE(sync_prefs_->IsSyncAllOsTypesEnabled());

  sync_prefs_->SetSelectedOsTypes(
      /*sync_all_os_types=*/false,
      /*registered_types=*/UserSelectableOsTypeSet::All(),
      /*selected_types=*/UserSelectableOsTypeSet::All());
  EXPECT_FALSE(sync_prefs_->IsSyncAllOsTypesEnabled());
  // Browser pref is not affected.
  EXPECT_TRUE(sync_prefs_->HasKeepEverythingSynced());

  sync_prefs_->SetSelectedOsTypes(
      /*sync_all_os_types=*/true,
      /*registered_types=*/UserSelectableOsTypeSet::All(),
      /*selected_types=*/UserSelectableOsTypeSet::All());
  EXPECT_TRUE(sync_prefs_->IsSyncAllOsTypesEnabled());
}

TEST_F(SyncPrefsTest, GetSelectedOsTypesWithAllOsTypesEnabled) {
  EXPECT_TRUE(sync_prefs_->IsSyncAllOsTypesEnabled());
  EXPECT_EQ(UserSelectableOsTypeSet::All(), sync_prefs_->GetSelectedOsTypes());
  for (UserSelectableOsType type : UserSelectableOsTypeSet::All()) {
    sync_prefs_->SetSelectedOsTypes(
        /*sync_all_os_types=*/true,
        /*registered_types=*/UserSelectableOsTypeSet::All(),
        /*selected_types=*/{type});
    EXPECT_EQ(UserSelectableOsTypeSet::All(),
              sync_prefs_->GetSelectedOsTypes());
  }
}

TEST_F(SyncPrefsTest, GetSelectedOsTypesNotAllOsTypesSelected) {
  const UserSelectableTypeSet browser_types = sync_prefs_->GetSelectedTypes();

  sync_prefs_->SetSelectedOsTypes(
      /*sync_all_os_types=*/false,
      /*registered_types=*/UserSelectableOsTypeSet::All(),
      /*selected_types=*/UserSelectableOsTypeSet());
  EXPECT_EQ(UserSelectableOsTypeSet(), sync_prefs_->GetSelectedOsTypes());
  // Browser types are not changed.
  EXPECT_EQ(browser_types, sync_prefs_->GetSelectedTypes());

  for (UserSelectableOsType type : UserSelectableOsTypeSet::All()) {
    sync_prefs_->SetSelectedOsTypes(
        /*sync_all_os_types=*/false,
        /*registered_types=*/UserSelectableOsTypeSet::All(),
        /*selected_types=*/{type});
    EXPECT_EQ(UserSelectableOsTypeSet{type}, sync_prefs_->GetSelectedOsTypes());
    // Browser types are not changed.
    EXPECT_EQ(browser_types, sync_prefs_->GetSelectedTypes());
  }
}
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

// Similar to SyncPrefsTest, but does not create a SyncPrefs instance. This lets
// individual tests set up the "before" state of the PrefService before
// SyncPrefs gets created.
class SyncPrefsMigrationTest : public testing::Test {
 protected:
  SyncPrefsMigrationTest() {
    SyncPrefs::RegisterProfilePrefs(pref_service_.registry());
  }

  base::test::SingleThreadTaskEnvironment task_environment_;
  TestingPrefServiceSimple pref_service_;
};

TEST_F(SyncPrefsMigrationTest, SyncSuppressed_NotSet) {
  // Sync was never enabled, so none of the relevant prefs have an explicit
  // value.
  ASSERT_FALSE(pref_service_.GetUserPrefValue(kSyncSuppressStart));
  ASSERT_FALSE(pref_service_.GetUserPrefValue(prefs::kSyncFirstSetupComplete));
  ASSERT_FALSE(pref_service_.GetUserPrefValue(prefs::kSyncRequested));

  syncer::MigrateSyncSuppressedPref(&pref_service_);

  // After the migration, Sync should still be disabled.
  SyncPrefs prefs(&pref_service_);
  EXPECT_FALSE(prefs.IsSyncRequested());
  EXPECT_FALSE(prefs.IsFirstSetupComplete());

  // The new pref should still not have an explicit value.
  EXPECT_FALSE(pref_service_.GetUserPrefValue(kSyncSuppressStart));
  EXPECT_FALSE(pref_service_.GetUserPrefValue(prefs::kSyncRequested));
}

TEST_F(SyncPrefsMigrationTest, SyncSuppressed_SyncEnabled) {
  // Sync is enabled, so kSyncSuppressStart is false and kSyncFirstSetupComplete
  // is true.
  pref_service_.SetBoolean(kSyncSuppressStart, false);
  pref_service_.SetBoolean(prefs::kSyncFirstSetupComplete, true);
  ASSERT_FALSE(pref_service_.GetUserPrefValue(prefs::kSyncRequested));

  syncer::MigrateSyncSuppressedPref(&pref_service_);

  // After the migration, Sync should still be enabled, and the old pref value
  // should be gone.
  SyncPrefs prefs(&pref_service_);
  EXPECT_TRUE(prefs.IsSyncRequested());
  EXPECT_TRUE(prefs.IsFirstSetupComplete());

  EXPECT_FALSE(pref_service_.GetUserPrefValue(kSyncSuppressStart));
  EXPECT_TRUE(pref_service_.GetUserPrefValue(prefs::kSyncRequested));
}

TEST_F(SyncPrefsMigrationTest, SyncSuppressed_SyncEnabledImplicitly) {
  // Sync is enabled implicitly: kSyncSuppressStart does not have a value, so it
  // defaults to false, but kSyncFirstSetupComplete is true. This state should
  // not exist, but it could happen if at some point in the past, the Sync setup
  // flow failed to actually set Sync to requested (see crbug.com/973770).
  ASSERT_FALSE(pref_service_.GetUserPrefValue(kSyncSuppressStart));
  pref_service_.SetBoolean(prefs::kSyncFirstSetupComplete, true);
  ASSERT_FALSE(pref_service_.GetUserPrefValue(prefs::kSyncRequested));

  syncer::MigrateSyncSuppressedPref(&pref_service_);

  // After the migration, Sync should still be enabled, and the old pref value
  // should be gone.
  SyncPrefs prefs(&pref_service_);
  EXPECT_TRUE(prefs.IsSyncRequested());
  EXPECT_TRUE(prefs.IsFirstSetupComplete());

  EXPECT_FALSE(pref_service_.GetUserPrefValue(kSyncSuppressStart));
  EXPECT_TRUE(pref_service_.GetUserPrefValue(prefs::kSyncRequested));
}

TEST_F(SyncPrefsMigrationTest, SyncSuppressed_SyncDisabledWithFirstSetup) {
  // Sync is explicitly disabled, so kSyncSuppressStart is true.
  pref_service_.SetBoolean(kSyncSuppressStart, true);
  pref_service_.SetBoolean(prefs::kSyncFirstSetupComplete, true);
  ASSERT_FALSE(pref_service_.GetUserPrefValue(prefs::kSyncRequested));

  syncer::MigrateSyncSuppressedPref(&pref_service_);

  // After the migration, Sync should still be disabled, and the old pref value
  // should be gone.
  SyncPrefs prefs(&pref_service_);
  EXPECT_FALSE(prefs.IsSyncRequested());
  EXPECT_TRUE(prefs.IsFirstSetupComplete());

  EXPECT_FALSE(pref_service_.GetUserPrefValue(kSyncSuppressStart));
  EXPECT_TRUE(pref_service_.GetUserPrefValue(prefs::kSyncRequested));
}

TEST_F(SyncPrefsMigrationTest, SyncSuppressed_SyncDisabledWithoutFirstSetup) {
  // Sync is explicitly disabled, so kSyncSuppressStart is true.
  pref_service_.SetBoolean(kSyncSuppressStart, true);
  pref_service_.SetBoolean(prefs::kSyncFirstSetupComplete, false);
  ASSERT_FALSE(pref_service_.GetUserPrefValue(prefs::kSyncRequested));

  syncer::MigrateSyncSuppressedPref(&pref_service_);

  // After the migration, Sync should still be disabled, and the old pref value
  // should be gone.
  SyncPrefs prefs(&pref_service_);
  EXPECT_FALSE(prefs.IsSyncRequested());
  EXPECT_FALSE(prefs.IsFirstSetupComplete());

  EXPECT_FALSE(pref_service_.GetUserPrefValue(kSyncSuppressStart));
  EXPECT_TRUE(pref_service_.GetUserPrefValue(prefs::kSyncRequested));
}

enum BooleanPrefState { PREF_FALSE, PREF_TRUE, PREF_UNSET };

// There are three prefs which are relevant for the "SyncSuppressed" migration:
// The old kSyncSuppressStart, the new kSyncRequested, and the (unchanged)
// kSyncFirstSetupComplete. Each can be explicitly true, explicitly false, or
// unset. This class is parameterized to cover all possible combinations.
class SyncPrefsSyncSuppressedMigrationCombinationsTest
    : public SyncPrefsMigrationTest,
      public testing::WithParamInterface<testing::tuple<BooleanPrefState,
                                                        BooleanPrefState,
                                                        BooleanPrefState>> {
 protected:
  void SetBooleanUserPrefValue(const char* pref_name, BooleanPrefState state) {
    switch (state) {
      case PREF_FALSE:
        pref_service_.SetBoolean(pref_name, false);
        break;
      case PREF_TRUE:
        pref_service_.SetBoolean(pref_name, true);
        break;
      case PREF_UNSET:
        pref_service_.ClearPref(pref_name);
        break;
    }
  }

  BooleanPrefState GetBooleanUserPrefValue(const char* pref_name) const {
    const base::Value* pref_value = pref_service_.GetUserPrefValue(pref_name);
    if (!pref_value) {
      return PREF_UNSET;
    }
    return pref_value->GetBool() ? PREF_TRUE : PREF_FALSE;
  }

  bool BooleanUserPrefMatches(const char* pref_name,
                              BooleanPrefState state) const {
    const base::Value* pref_value = pref_service_.GetUserPrefValue(pref_name);
    switch (state) {
      case PREF_FALSE:
        return pref_value && !pref_value->GetBool();
      case PREF_TRUE:
        return pref_value && pref_value->GetBool();
      case PREF_UNSET:
        return !pref_value;
    }
  }
};

TEST_P(SyncPrefsSyncSuppressedMigrationCombinationsTest, Idempotent) {
  // Set the initial values (true, false, or unset) of the three prefs from the
  // test params.
  SetBooleanUserPrefValue(kSyncSuppressStart, testing::get<0>(GetParam()));
  SetBooleanUserPrefValue(prefs::kSyncFirstSetupComplete,
                          testing::get<1>(GetParam()));
  SetBooleanUserPrefValue(prefs::kSyncRequested, testing::get<2>(GetParam()));

  // Do the first migration.
  syncer::MigrateSyncSuppressedPref(&pref_service_);

  // Record the resulting pref values.
  BooleanPrefState expect_suppress_start =
      GetBooleanUserPrefValue(kSyncSuppressStart);
  BooleanPrefState expect_first_setup_complete =
      GetBooleanUserPrefValue(prefs::kSyncFirstSetupComplete);
  BooleanPrefState expect_requested =
      GetBooleanUserPrefValue(prefs::kSyncRequested);

  // Do the second migration.
  syncer::MigrateSyncSuppressedPref(&pref_service_);

  // Verify that the pref values did not change.
  EXPECT_TRUE(
      BooleanUserPrefMatches(kSyncSuppressStart, expect_suppress_start));
  EXPECT_TRUE(BooleanUserPrefMatches(prefs::kSyncFirstSetupComplete,
                                     expect_first_setup_complete));
  EXPECT_TRUE(BooleanUserPrefMatches(prefs::kSyncRequested, expect_requested));
}

// Not all combinations of pref values are possible in practice, but anyway the
// migration should always be idempotent, so we test all combinations here.
INSTANTIATE_TEST_SUITE_P(
    All,
    SyncPrefsSyncSuppressedMigrationCombinationsTest,
    testing::Combine(::testing::Values(PREF_FALSE, PREF_TRUE, PREF_UNSET),
                     ::testing::Values(PREF_FALSE, PREF_TRUE, PREF_UNSET),
                     ::testing::Values(PREF_FALSE, PREF_TRUE, PREF_UNSET)));

}  // namespace

}  // namespace syncer
