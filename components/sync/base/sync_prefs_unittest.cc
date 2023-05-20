// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/base/sync_prefs.h"

#include <memory>

#include "base/test/metrics/histogram_tester.h"
#include "base/test/task_environment.h"
#include "build/chromeos_buildflags.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_value_map.h"
#include "components/prefs/testing_pref_service.h"
#include "components/sync/base/pref_names.h"
#include "components/sync/base/user_selectable_type.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace syncer {

namespace {

using ::testing::InSequence;
using ::testing::StrictMock;

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

TEST_F(SyncPrefsTest, EncryptionBootstrapToken) {
  EXPECT_TRUE(sync_prefs_->GetEncryptionBootstrapToken().empty());
  sync_prefs_->SetEncryptionBootstrapToken("token");
  EXPECT_EQ("token", sync_prefs_->GetEncryptionBootstrapToken());
  sync_prefs_->ClearEncryptionBootstrapToken();
  EXPECT_TRUE(sync_prefs_->GetEncryptionBootstrapToken().empty());
}

class MockSyncPrefObserver : public SyncPrefObserver {
 public:
  MOCK_METHOD(void, OnSyncManagedPrefChange, (bool), (override));
  MOCK_METHOD(void, OnFirstSetupCompletePrefChange, (bool), (override));
  MOCK_METHOD(void, OnPreferredDataTypesPrefChange, (), (override));
};

TEST_F(SyncPrefsTest, ObservedPrefs) {
  StrictMock<MockSyncPrefObserver> mock_sync_pref_observer;
  InSequence dummy;
  EXPECT_CALL(mock_sync_pref_observer, OnSyncManagedPrefChange(true));
  EXPECT_CALL(mock_sync_pref_observer, OnSyncManagedPrefChange(false));
  EXPECT_CALL(mock_sync_pref_observer, OnFirstSetupCompletePrefChange(true));
  EXPECT_CALL(mock_sync_pref_observer, OnFirstSetupCompletePrefChange(false));

  ASSERT_FALSE(sync_prefs_->IsSyncClientDisabledByPolicy());
  ASSERT_FALSE(sync_prefs_->IsInitialSyncFeatureSetupComplete());
  ASSERT_FALSE(sync_prefs_->IsSyncRequested());

  sync_prefs_->AddSyncPrefObserver(&mock_sync_pref_observer);

  pref_service_.SetBoolean(prefs::internal::kSyncManaged, true);
  EXPECT_TRUE(sync_prefs_->IsSyncClientDisabledByPolicy());
  pref_service_.SetBoolean(prefs::internal::kSyncManaged, false);
  EXPECT_FALSE(sync_prefs_->IsSyncClientDisabledByPolicy());

  sync_prefs_->SetInitialSyncFeatureSetupComplete();
  EXPECT_TRUE(sync_prefs_->IsInitialSyncFeatureSetupComplete());
  sync_prefs_->ClearInitialSyncFeatureSetupComplete();
  EXPECT_FALSE(sync_prefs_->IsInitialSyncFeatureSetupComplete());

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

TEST_F(SyncPrefsTest, Basic) {
  EXPECT_FALSE(sync_prefs_->IsInitialSyncFeatureSetupComplete());
  sync_prefs_->SetInitialSyncFeatureSetupComplete();
  EXPECT_TRUE(sync_prefs_->IsInitialSyncFeatureSetupComplete());

  EXPECT_FALSE(sync_prefs_->IsSyncRequested());
  sync_prefs_->SetSyncRequested(true);
  EXPECT_TRUE(sync_prefs_->IsSyncRequested());
  sync_prefs_->SetSyncRequested(false);
  EXPECT_FALSE(sync_prefs_->IsSyncRequested());

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
}

TEST_F(SyncPrefsTest, SelectedTypesKeepEverythingSynced) {
  ASSERT_TRUE(sync_prefs_->HasKeepEverythingSynced());

  EXPECT_EQ(
      UserSelectableTypeSet::All(),
      sync_prefs_->GetSelectedTypes(SyncPrefs::SyncAccountState::kSyncing));
  for (UserSelectableType type : UserSelectableTypeSet::All()) {
    sync_prefs_->SetSelectedTypes(
        /*keep_everything_synced=*/true,
        /*registered_types=*/UserSelectableTypeSet::All(),
        /*selected_types=*/{type});
    EXPECT_EQ(
        UserSelectableTypeSet::All(),
        sync_prefs_->GetSelectedTypes(SyncPrefs::SyncAccountState::kSyncing));
  }
}

TEST_F(SyncPrefsTest, SelectedTypesKeepEverythingSyncedButPolicyRestricted) {
  ASSERT_TRUE(sync_prefs_->HasKeepEverythingSynced());
  pref_service_.SetManagedPref(prefs::internal::kSyncPreferences,
                               base::Value(false));

  UserSelectableTypeSet expected_type_set = UserSelectableTypeSet::All();
  expected_type_set.Remove(UserSelectableType::kPreferences);
  EXPECT_EQ(expected_type_set, sync_prefs_->GetSelectedTypes(
                                   SyncPrefs::SyncAccountState::kSyncing));
}

TEST_F(SyncPrefsTest, SelectedTypesNotKeepEverythingSynced) {
  sync_prefs_->SetSelectedTypes(
      /*keep_everything_synced=*/false,
      /*registered_types=*/UserSelectableTypeSet::All(),
      /*selected_types=*/UserSelectableTypeSet());

  ASSERT_NE(
      UserSelectableTypeSet::All(),
      sync_prefs_->GetSelectedTypes(SyncPrefs::SyncAccountState::kSyncing));
  for (UserSelectableType type : UserSelectableTypeSet::All()) {
    sync_prefs_->SetSelectedTypes(
        /*keep_everything_synced=*/false,
        /*registered_types=*/UserSelectableTypeSet::All(),
        /*selected_types=*/{type});
    EXPECT_EQ(
        UserSelectableTypeSet({type}),
        sync_prefs_->GetSelectedTypes(SyncPrefs::SyncAccountState::kSyncing));
  }
}

TEST_F(SyncPrefsTest, SelectedTypesNotKeepEverythingSyncedAndPolicyRestricted) {
  pref_service_.SetManagedPref(prefs::internal::kSyncPreferences,
                               base::Value(false));
  sync_prefs_->SetSelectedTypes(
      /*keep_everything_synced=*/false,
      /*registered_types=*/UserSelectableTypeSet::All(),
      /*selected_types=*/UserSelectableTypeSet());

  ASSERT_FALSE(
      sync_prefs_->GetSelectedTypes(SyncPrefs::SyncAccountState::kSyncing)
          .Has(UserSelectableType::kPreferences));
  for (UserSelectableType type : UserSelectableTypeSet::All()) {
    sync_prefs_->SetSelectedTypes(
        /*keep_everything_synced=*/false,
        /*registered_types=*/UserSelectableTypeSet::All(),
        /*selected_types=*/{type});
    UserSelectableTypeSet expected_type_set = {type};
    expected_type_set.Remove(UserSelectableType::kPreferences);
    EXPECT_EQ(expected_type_set, sync_prefs_->GetSelectedTypes(
                                     SyncPrefs::SyncAccountState::kSyncing));
  }
}

TEST_F(SyncPrefsTest, SetTypeDisabledByPolicy) {
  // By default, data types are enabled, and not policy-controlled.
  ASSERT_TRUE(
      sync_prefs_->GetSelectedTypes(SyncPrefs::SyncAccountState::kSyncing)
          .Has(UserSelectableType::kBookmarks));
  ASSERT_FALSE(
      sync_prefs_->IsTypeManagedByPolicy(UserSelectableType::kBookmarks));
  ASSERT_TRUE(
      sync_prefs_->GetSelectedTypes(SyncPrefs::SyncAccountState::kSyncing)
          .Has(UserSelectableType::kAutofill));
  ASSERT_FALSE(
      sync_prefs_->IsTypeManagedByPolicy(UserSelectableType::kAutofill));

  // Set up a policy to disable bookmarks.
  PrefValueMap policy_prefs;
  SyncPrefs::SetTypeDisabledByPolicy(&policy_prefs,
                                     UserSelectableType::kBookmarks);
  // Copy the policy prefs map over into the PrefService.
  for (const auto& policy_pref : policy_prefs) {
    pref_service_.SetManagedPref(policy_pref.first, policy_pref.second.Clone());
  }

  // The policy should take effect and disable bookmarks.
  EXPECT_FALSE(
      sync_prefs_->GetSelectedTypes(SyncPrefs::SyncAccountState::kSyncing)
          .Has(UserSelectableType::kBookmarks));
  EXPECT_TRUE(
      sync_prefs_->IsTypeManagedByPolicy(UserSelectableType::kBookmarks));
  // Other types should be unaffected.
  EXPECT_TRUE(
      sync_prefs_->GetSelectedTypes(SyncPrefs::SyncAccountState::kSyncing)
          .Has(UserSelectableType::kAutofill));
  EXPECT_FALSE(
      sync_prefs_->IsTypeManagedByPolicy(UserSelectableType::kAutofill));
}

TEST_F(SyncPrefsTest, SelectedTypesInTransportMode) {
  UserSelectableTypeSet expected_selected_types = UserSelectableTypeSet::All();

#if BUILDFLAG(IS_IOS)
  // In transport-only mode, bookmarks and reading list require an
  // additional opt-in.
  // TODO(crbug.com/1440628): Cleanup the temporary behaviour of an
  // additional opt in for Bookmarks and Reading Lists.
  expected_selected_types.Remove(UserSelectableType::kBookmarks);
  expected_selected_types.Remove(UserSelectableType::kReadingList);
#endif  // BUILDFLAG(IS_IOS)

  // Get default values of selected types in transport-mode.
  UserSelectableTypeSet selected_types = sync_prefs_->GetSelectedTypes(
      SyncPrefs::SyncAccountState::kSignedInNotSyncing);
  EXPECT_EQ(expected_selected_types, selected_types);

  // Change one of the default values for example kPasswords.
  selected_types.Remove(UserSelectableType::kPasswords);
  sync_prefs_->SetSelectedTypes(
      /*keep_everything_synced=*/false,
      /*registered_types=*/UserSelectableTypeSet::All(),
      /*selected_types=*/selected_types);

  // kPasswords should be disabled, other default values should be unaffected.
  for (UserSelectableType type : expected_selected_types) {
    if (type == UserSelectableType::kPasswords) {
      EXPECT_FALSE(selected_types.Has(type));
    } else {
      EXPECT_TRUE(selected_types.Has(type));
    }
  }

  // Pass keep_everything_synced true to verify that it has no effect in
  // transport-mode.
  sync_prefs_->SetSelectedTypes(
      /*keep_everything_synced=*/true,
      /*registered_types=*/UserSelectableTypeSet::All(),
      /*selected_types=*/selected_types);

  // kPasswords should still be disabled, other default values should be
  // unaffected.
  for (UserSelectableType type : expected_selected_types) {
    if (type == UserSelectableType::kPasswords) {
      EXPECT_FALSE(selected_types.Has(type));
    } else {
      EXPECT_TRUE(selected_types.Has(type));
    }
  }
}

TEST_F(SyncPrefsTest, SetSelectedTypeInTransportMode) {
  UserSelectableTypeSet default_selected_types = UserSelectableTypeSet::All();

#if BUILDFLAG(IS_IOS)
  // In transport-only mode, bookmarks and reading list require an
  // additional opt-in.
  // TODO(crbug.com/1440628): Cleanup the temporary behaviour of an
  // additional opt in for Bookmarks and Reading Lists.
  default_selected_types.Remove(UserSelectableType::kBookmarks);
  default_selected_types.Remove(UserSelectableType::kReadingList);
#endif  // BUILDFLAG(IS_IOS)

  // Get default values of selected types in transport-mode.
  UserSelectableTypeSet selected_types = sync_prefs_->GetSelectedTypes(
      SyncPrefs::SyncAccountState::kSignedInNotSyncing);
  EXPECT_EQ(default_selected_types, selected_types);

  // Change one of the default values for example kPasswords.
  sync_prefs_->SetSelectedType(UserSelectableType::kPasswords, false);
  selected_types = sync_prefs_->GetSelectedTypes(
      SyncPrefs::SyncAccountState::kSignedInNotSyncing);

  // kPasswords should be disabled, other default values should be unaffected.
  EXPECT_EQ(selected_types, Difference(default_selected_types,
                                       {UserSelectableType::kPasswords}));
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
  const UserSelectableTypeSet browser_types =
      sync_prefs_->GetSelectedTypes(SyncPrefs::SyncAccountState::kSyncing);

  sync_prefs_->SetSelectedOsTypes(
      /*sync_all_os_types=*/false,
      /*registered_types=*/UserSelectableOsTypeSet::All(),
      /*selected_types=*/UserSelectableOsTypeSet());
  EXPECT_EQ(UserSelectableOsTypeSet(), sync_prefs_->GetSelectedOsTypes());
  // Browser types are not changed.
  EXPECT_EQ(browser_types, sync_prefs_->GetSelectedTypes(
                               SyncPrefs::SyncAccountState::kSyncing));

  for (UserSelectableOsType type : UserSelectableOsTypeSet::All()) {
    sync_prefs_->SetSelectedOsTypes(
        /*sync_all_os_types=*/false,
        /*registered_types=*/UserSelectableOsTypeSet::All(),
        /*selected_types=*/{type});
    EXPECT_EQ(UserSelectableOsTypeSet({type}),
              sync_prefs_->GetSelectedOsTypes());
    // Browser types are not changed.
    EXPECT_EQ(browser_types, sync_prefs_->GetSelectedTypes(
                                 SyncPrefs::SyncAccountState::kSyncing));
  }
}

TEST_F(SyncPrefsTest, SelectedOsTypesKeepEverythingSyncedButPolicyRestricted) {
  ASSERT_TRUE(sync_prefs_->HasKeepEverythingSynced());
  pref_service_.SetManagedPref(prefs::internal::kSyncOsPreferences,
                               base::Value(false));

  UserSelectableOsTypeSet expected_type_set = UserSelectableOsTypeSet::All();
  expected_type_set.Remove(UserSelectableOsType::kOsPreferences);
  EXPECT_EQ(expected_type_set, sync_prefs_->GetSelectedOsTypes());
}

TEST_F(SyncPrefsTest,
       SelectedOsTypesNotKeepEverythingSyncedAndPolicyRestricted) {
  pref_service_.SetManagedPref(prefs::internal::kSyncOsPreferences,
                               base::Value(false));
  sync_prefs_->SetSelectedOsTypes(
      /*sync_all_os_types=*/false,
      /*registered_types=*/UserSelectableOsTypeSet::All(),
      /*selected_types=*/UserSelectableOsTypeSet());

  ASSERT_FALSE(sync_prefs_->GetSelectedOsTypes().Has(
      UserSelectableOsType::kOsPreferences));
  for (UserSelectableOsType type : UserSelectableOsTypeSet::All()) {
    sync_prefs_->SetSelectedOsTypes(
        /*sync_all_os_types=*/false,
        /*registered_types=*/UserSelectableOsTypeSet::All(),
        /*selected_types=*/{type});
    UserSelectableOsTypeSet expected_type_set = {type};
    expected_type_set.Remove(UserSelectableOsType::kOsPreferences);
    EXPECT_EQ(expected_type_set, sync_prefs_->GetSelectedOsTypes());
  }
}

TEST_F(SyncPrefsTest, SetOsTypeDisabledByPolicy) {
  // By default, data types are enabled, and not policy-controlled.
  ASSERT_TRUE(
      sync_prefs_->GetSelectedOsTypes().Has(UserSelectableOsType::kOsApps));
  ASSERT_FALSE(
      sync_prefs_->IsOsTypeManagedByPolicy(UserSelectableOsType::kOsApps));
  ASSERT_TRUE(sync_prefs_->GetSelectedOsTypes().Has(
      UserSelectableOsType::kOsPreferences));
  ASSERT_FALSE(sync_prefs_->IsOsTypeManagedByPolicy(
      UserSelectableOsType::kOsPreferences));

  // Set up a policy to disable apps.
  PrefValueMap policy_prefs;
  SyncPrefs::SetOsTypeDisabledByPolicy(&policy_prefs,
                                       UserSelectableOsType::kOsApps);
  // Copy the policy prefs map over into the PrefService.
  for (const auto& policy_pref : policy_prefs) {
    pref_service_.SetManagedPref(policy_pref.first, policy_pref.second.Clone());
  }

  // The policy should take effect and disable apps.
  EXPECT_FALSE(
      sync_prefs_->GetSelectedOsTypes().Has(UserSelectableOsType::kOsApps));
  EXPECT_TRUE(
      sync_prefs_->IsOsTypeManagedByPolicy(UserSelectableOsType::kOsApps));
  // Other types should be unaffected.
  EXPECT_TRUE(sync_prefs_->GetSelectedOsTypes().Has(
      UserSelectableOsType::kOsPreferences));
  EXPECT_FALSE(sync_prefs_->IsOsTypeManagedByPolicy(
      UserSelectableOsType::kOsPreferences));
}
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

#if BUILDFLAG(IS_CHROMEOS_LACROS)
TEST_F(SyncPrefsTest, ShouldSetAppsSyncEnabledByOsToFalseByDefault) {
  EXPECT_FALSE(sync_prefs_->IsAppsSyncEnabledByOs());
}

TEST_F(SyncPrefsTest, ShouldChangeAppsSyncEnabledByOsAndNotifyObservers) {
  StrictMock<MockSyncPrefObserver> mock_sync_pref_observer;
  sync_prefs_->AddSyncPrefObserver(&mock_sync_pref_observer);

  EXPECT_CALL(mock_sync_pref_observer, OnPreferredDataTypesPrefChange());
  sync_prefs_->SetAppsSyncEnabledByOs(/*apps_sync_enabled=*/true);
  EXPECT_TRUE(sync_prefs_->IsAppsSyncEnabledByOs());

  testing::Mock::VerifyAndClearExpectations(&mock_sync_pref_observer);
  EXPECT_CALL(mock_sync_pref_observer, OnPreferredDataTypesPrefChange());
  sync_prefs_->SetAppsSyncEnabledByOs(/*apps_sync_enabled=*/false);
  EXPECT_FALSE(sync_prefs_->IsAppsSyncEnabledByOs());
}
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)

TEST_F(SyncPrefsTest, PassphrasePromptMutedProductVersion) {
  EXPECT_EQ(0, sync_prefs_->GetPassphrasePromptMutedProductVersion());

  sync_prefs_->SetPassphrasePromptMutedProductVersion(83);
  EXPECT_EQ(83, sync_prefs_->GetPassphrasePromptMutedProductVersion());

  sync_prefs_->ClearPassphrasePromptMutedProductVersion();
  EXPECT_EQ(0, sync_prefs_->GetPassphrasePromptMutedProductVersion());
}

#if BUILDFLAG(IS_IOS)
TEST_F(SyncPrefsTest, SetBookmarksAndReadingListAccountStorageOptInPrefChange) {
  // Default value disabled.
  EXPECT_FALSE(
      sync_prefs_->IsOptedInForBookmarksAndReadingListAccountStorage());

  // Enable bookmarks and reading list account storage pref.
  sync_prefs_->SetBookmarksAndReadingListAccountStorageOptIn(true);

  // Check pref change to enabled.
  EXPECT_TRUE(sync_prefs_->IsOptedInForBookmarksAndReadingListAccountStorage());

  // Clear pref.
  sync_prefs_->ClearBookmarksAndReadingListAccountStorageOptIn();

  // Default value applied after clearing the pref.
  EXPECT_FALSE(
      sync_prefs_->IsOptedInForBookmarksAndReadingListAccountStorage());
}
#endif  // BUILDFLAG(IS_IOS)

enum BooleanPrefState { PREF_FALSE, PREF_TRUE, PREF_UNSET };

// Similar to SyncPrefsTest, but does not create a SyncPrefs instance. This lets
// individual tests set up the "before" state of the PrefService before
// SyncPrefs gets created.
class SyncPrefsMigrationTest : public testing::Test {
 protected:
  SyncPrefsMigrationTest() {
    SyncPrefs::RegisterProfilePrefs(pref_service_.registry());
  }

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

  base::test::SingleThreadTaskEnvironment task_environment_;
  TestingPrefServiceSimple pref_service_;
};

#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_IOS)

TEST_F(SyncPrefsMigrationTest, SyncRequested_NothingSet) {
  // None of the prefs is set explicitly.
  ASSERT_FALSE(pref_service_.GetUserPrefValue(prefs::internal::kSyncRequested));
  ASSERT_FALSE(pref_service_.GetUserPrefValue(
      prefs::internal::kSyncInitialSyncFeatureSetupComplete));
  ASSERT_FALSE(pref_service_.GetUserPrefValue(
      prefs::internal::kSyncKeepEverythingSynced));

  // Run the migration.
  syncer::SyncPrefs::MigrateSyncRequestedPrefPostMice(&pref_service_);

  // The migration should have left all the prefs unset.
  EXPECT_FALSE(pref_service_.GetUserPrefValue(prefs::internal::kSyncRequested));
  EXPECT_FALSE(pref_service_.GetUserPrefValue(
      prefs::internal::kSyncInitialSyncFeatureSetupComplete));
  EXPECT_FALSE(pref_service_.GetUserPrefValue(
      prefs::internal::kSyncKeepEverythingSynced));
}

TEST_F(SyncPrefsMigrationTest, SyncRequested_SyncRequestedWithAllTypes) {
  pref_service_.SetBoolean(prefs::internal::kSyncRequested, true);
  pref_service_.SetBoolean(
      prefs::internal::kSyncInitialSyncFeatureSetupComplete, true);
  pref_service_.SetBoolean(prefs::internal::kSyncKeepEverythingSynced, true);

  // Run the migration.
  syncer::SyncPrefs::MigrateSyncRequestedPrefPostMice(&pref_service_);

  // The migration should have changed nothing.
  SyncPrefs prefs(&pref_service_);
  EXPECT_TRUE(prefs.IsSyncRequested());
  EXPECT_TRUE(prefs.IsInitialSyncFeatureSetupComplete());
  EXPECT_TRUE(prefs.HasKeepEverythingSynced());
}

TEST_F(SyncPrefsMigrationTest, SyncRequested_SyncRequestedWithSomeTypes) {
  const UserSelectableTypeSet enabled_types = {
      UserSelectableType::kBookmarks, UserSelectableType::kPreferences};
  pref_service_.SetBoolean(prefs::internal::kSyncRequested, true);
  pref_service_.SetBoolean(
      prefs::internal::kSyncInitialSyncFeatureSetupComplete, true);
  pref_service_.SetBoolean(prefs::internal::kSyncKeepEverythingSynced, false);
  for (UserSelectableType type : enabled_types) {
    const char* pref_name = SyncPrefs::GetPrefNameForTypeForTesting(type);
    pref_service_.SetBoolean(pref_name, true);
  }

  // Run the migration.
  syncer::SyncPrefs::MigrateSyncRequestedPrefPostMice(&pref_service_);

  // The migration should have changed nothing.
  SyncPrefs prefs(&pref_service_);
  EXPECT_TRUE(prefs.IsSyncRequested());
  EXPECT_TRUE(prefs.IsInitialSyncFeatureSetupComplete());
  EXPECT_FALSE(prefs.HasKeepEverythingSynced());
  EXPECT_EQ(prefs.GetSelectedTypes(SyncPrefs::SyncAccountState::kSyncing),
            enabled_types);
}

TEST_F(SyncPrefsMigrationTest, SyncRequested_SyncRequestedWithNoTypes) {
  pref_service_.SetBoolean(prefs::internal::kSyncRequested, true);
  pref_service_.SetBoolean(
      prefs::internal::kSyncInitialSyncFeatureSetupComplete, true);
  pref_service_.SetBoolean(prefs::internal::kSyncKeepEverythingSynced, false);
  // All selectable types are false by default.

  // Run the migration.
  syncer::SyncPrefs::MigrateSyncRequestedPrefPostMice(&pref_service_);

  // The migration should have changed nothing.
  SyncPrefs prefs(&pref_service_);
  EXPECT_TRUE(prefs.IsSyncRequested());
  EXPECT_TRUE(prefs.IsInitialSyncFeatureSetupComplete());
  EXPECT_FALSE(prefs.HasKeepEverythingSynced());
  EXPECT_TRUE(
      prefs.GetSelectedTypes(SyncPrefs::SyncAccountState::kSyncing).Empty());
}

TEST_F(SyncPrefsMigrationTest, SyncRequested_SyncNotRequestedWithNoTypes) {
  pref_service_.SetBoolean(prefs::internal::kSyncRequested, false);
  pref_service_.SetBoolean(
      prefs::internal::kSyncInitialSyncFeatureSetupComplete, true);
  pref_service_.SetBoolean(prefs::internal::kSyncKeepEverythingSynced, false);
  // All selectable types are false by default.

  // Run the migration.
  syncer::SyncPrefs::MigrateSyncRequestedPrefPostMice(&pref_service_);

  // The migration should have set SyncRequested to true, but kept all data
  // types disabled.
  SyncPrefs prefs(&pref_service_);
  EXPECT_TRUE(prefs.IsSyncRequested());
  EXPECT_TRUE(prefs.IsInitialSyncFeatureSetupComplete());
  EXPECT_FALSE(prefs.HasKeepEverythingSynced());
  EXPECT_TRUE(
      prefs.GetSelectedTypes(SyncPrefs::SyncAccountState::kSyncing).Empty());
}

TEST_F(SyncPrefsMigrationTest, SyncRequested_SyncNotRequestedWithSomeTypes) {
  const UserSelectableTypeSet enabled_types = {
      UserSelectableType::kBookmarks, UserSelectableType::kPreferences};
  pref_service_.SetBoolean(prefs::internal::kSyncRequested, false);
  pref_service_.SetBoolean(
      prefs::internal::kSyncInitialSyncFeatureSetupComplete, true);
  pref_service_.SetBoolean(prefs::internal::kSyncKeepEverythingSynced, false);
  for (UserSelectableType type : enabled_types) {
    const char* pref_name = SyncPrefs::GetPrefNameForTypeForTesting(type);
    pref_service_.SetBoolean(pref_name, true);
  }

  // Run the migration.
  syncer::SyncPrefs::MigrateSyncRequestedPrefPostMice(&pref_service_);

  // The migration should have set SyncRequested to true, but turned off all
  // data types.
  SyncPrefs prefs(&pref_service_);
  EXPECT_TRUE(prefs.IsSyncRequested());
  EXPECT_TRUE(prefs.IsInitialSyncFeatureSetupComplete());
  EXPECT_FALSE(prefs.HasKeepEverythingSynced());
  EXPECT_TRUE(
      prefs.GetSelectedTypes(SyncPrefs::SyncAccountState::kSyncing).Empty());
}

TEST_F(SyncPrefsMigrationTest, SyncRequested_SyncNotRequestedWithAllTypes) {
  const UserSelectableTypeSet enabled_types = {
      UserSelectableType::kBookmarks, UserSelectableType::kPreferences};
  pref_service_.SetBoolean(prefs::internal::kSyncRequested, false);
  pref_service_.SetBoolean(
      prefs::internal::kSyncInitialSyncFeatureSetupComplete, true);
  pref_service_.SetBoolean(prefs::internal::kSyncKeepEverythingSynced, true);
  // Even though "Sync everything" is enabled, also explicitly set some of the
  // individual data type prefs, to make sure the migration handles this case.
  for (UserSelectableType type : enabled_types) {
    const char* pref_name = SyncPrefs::GetPrefNameForTypeForTesting(type);
    pref_service_.SetBoolean(pref_name, true);
  }

  // Run the migration.
  syncer::SyncPrefs::MigrateSyncRequestedPrefPostMice(&pref_service_);

  // The migration should have set SyncRequested to true, but turned off all
  // data types and the "sync everything" flag.
  SyncPrefs prefs(&pref_service_);
  EXPECT_TRUE(prefs.IsSyncRequested());
  EXPECT_TRUE(prefs.IsInitialSyncFeatureSetupComplete());
  EXPECT_FALSE(prefs.HasKeepEverythingSynced());
  EXPECT_TRUE(
      prefs.GetSelectedTypes(SyncPrefs::SyncAccountState::kSyncing).Empty());
}

// There are three boolean prefs which are relevant for the "SyncRequested"
// migration: kSyncRequested, kSyncInitialSyncFeatureSetupComplete, and
// kSyncKeepEverythingSynced (and technically also all the data-type-specific
// prefs, which are not covered by this test). Each can be explicitly true,
// explicitly false, or unset. This class is parameterized to cover all possible
// combinations.
class SyncPrefsSyncRequestedMigrationCombinationsTest
    : public SyncPrefsMigrationTest,
      public testing::WithParamInterface<testing::tuple<BooleanPrefState,
                                                        BooleanPrefState,
                                                        BooleanPrefState>> {};

TEST_P(SyncPrefsSyncRequestedMigrationCombinationsTest, Idempotent) {
  // Set the initial values (true, false, or unset) of the three prefs from the
  // test params.
  SetBooleanUserPrefValue(prefs::internal::kSyncRequested,
                          testing::get<0>(GetParam()));
  SetBooleanUserPrefValue(prefs::internal::kSyncInitialSyncFeatureSetupComplete,
                          testing::get<1>(GetParam()));
  SetBooleanUserPrefValue(prefs::internal::kSyncKeepEverythingSynced,
                          testing::get<2>(GetParam()));

  // Do the first migration.
  syncer::SyncPrefs::MigrateSyncRequestedPrefPostMice(&pref_service_);

  // Record the resulting pref values.
  BooleanPrefState expect_sync_requested =
      GetBooleanUserPrefValue(prefs::internal::kSyncRequested);
  BooleanPrefState expect_initial_sync_feature_setup_complete =
      GetBooleanUserPrefValue(
          prefs::internal::kSyncInitialSyncFeatureSetupComplete);
  BooleanPrefState expect_sync_everything =
      GetBooleanUserPrefValue(prefs::internal::kSyncKeepEverythingSynced);

  // Do the second migration.
  syncer::SyncPrefs::MigrateSyncRequestedPrefPostMice(&pref_service_);

  // Verify that the pref values did not change.
  EXPECT_TRUE(BooleanUserPrefMatches(prefs::internal::kSyncRequested,
                                     expect_sync_requested));
  EXPECT_TRUE(BooleanUserPrefMatches(
      prefs::internal::kSyncInitialSyncFeatureSetupComplete,
      expect_initial_sync_feature_setup_complete));
  EXPECT_TRUE(BooleanUserPrefMatches(prefs::internal::kSyncKeepEverythingSynced,
                                     expect_sync_everything));
}

// Not all combinations of pref values are possible in practice, but anyway the
// migration should always be idempotent, so we test all combinations here.
INSTANTIATE_TEST_SUITE_P(
    All,
    SyncPrefsSyncRequestedMigrationCombinationsTest,
    testing::Combine(::testing::Values(PREF_FALSE, PREF_TRUE, PREF_UNSET),
                     ::testing::Values(PREF_FALSE, PREF_TRUE, PREF_UNSET),
                     ::testing::Values(PREF_FALSE, PREF_TRUE, PREF_UNSET)));

#endif  // BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_IOS)

}  // namespace

}  // namespace syncer
