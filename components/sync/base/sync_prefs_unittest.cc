// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/base/sync_prefs.h"

#include <memory>

#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "build/chromeos_buildflags.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_value_map.h"
#include "components/prefs/testing_pref_service.h"
#include "components/sync/base/features.h"
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
      sync_prefs_
          ->IsOptedInForBookmarksAndReadingListAccountStorageForTesting());

  // Enable bookmarks and reading list account storage pref.
  sync_prefs_->SetBookmarksAndReadingListAccountStorageOptIn(true);

  // Check pref change to enabled.
  EXPECT_TRUE(
      sync_prefs_
          ->IsOptedInForBookmarksAndReadingListAccountStorageForTesting());

  // Clear pref.
  sync_prefs_->ClearBookmarksAndReadingListAccountStorageOptIn();

  // Default value applied after clearing the pref.
  EXPECT_FALSE(
      sync_prefs_
          ->IsOptedInForBookmarksAndReadingListAccountStorageForTesting());
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

  const char* kBookmarksPref =
      SyncPrefs::GetPrefNameForTypeForTesting(UserSelectableType::kBookmarks);
  const char* kReadingListPref =
      SyncPrefs::GetPrefNameForTypeForTesting(UserSelectableType::kReadingList);
  const char* kPasswordsPref =
      SyncPrefs::GetPrefNameForTypeForTesting(UserSelectableType::kPasswords);
  const char* kAutofillPref =
      SyncPrefs::GetPrefNameForTypeForTesting(UserSelectableType::kAutofill);
  const char* kPreferencesPref =
      SyncPrefs::GetPrefNameForTypeForTesting(UserSelectableType::kPreferences);

  base::test::SingleThreadTaskEnvironment task_environment_;
  TestingPrefServiceSimple pref_service_;
};

TEST_F(SyncPrefsMigrationTest,
       ReplacingSyncWithSignin_NoMigrationForSignedOutUser) {
  base::test::ScopedFeatureList feature_list(
      kReplaceSyncPromosWithSignInPromos);

  // Even though the user is signed out, some prefs are set (e.g. because the
  // user was previously syncing).
  SetBooleanUserPrefValue(kBookmarksPref, PREF_TRUE);
  SetBooleanUserPrefValue(kReadingListPref, PREF_FALSE);

  // The migration runs for a signed-out user. This should do nothing.
  SyncPrefs(&pref_service_)
      .MaybeMigratePrefsForReplacingSyncWithSignin(
          SyncPrefs::SyncAccountState::kNotSignedIn);

  // Everything should be unchanged.
  EXPECT_TRUE(BooleanUserPrefMatches(kBookmarksPref, PREF_TRUE));
  EXPECT_TRUE(BooleanUserPrefMatches(kReadingListPref, PREF_FALSE));
  EXPECT_TRUE(BooleanUserPrefMatches(kPreferencesPref, PREF_UNSET));
}

TEST_F(SyncPrefsMigrationTest,
       ReplacingSyncWithSignin_NoMigrationForSyncingUser) {
  base::test::ScopedFeatureList feature_list(
      kReplaceSyncPromosWithSignInPromos);

  // Some data type prefs are set.
  SetBooleanUserPrefValue(kBookmarksPref, PREF_TRUE);
  SetBooleanUserPrefValue(kReadingListPref, PREF_FALSE);

  // The migration runs for a syncing user. This should do nothing.
  SyncPrefs(&pref_service_)
      .MaybeMigratePrefsForReplacingSyncWithSignin(
          SyncPrefs::SyncAccountState::kSyncing);

  // Everything should be unchanged.
  EXPECT_TRUE(BooleanUserPrefMatches(kBookmarksPref, PREF_TRUE));
  EXPECT_TRUE(BooleanUserPrefMatches(kReadingListPref, PREF_FALSE));
  EXPECT_TRUE(BooleanUserPrefMatches(kPreferencesPref, PREF_UNSET));
}

TEST_F(SyncPrefsMigrationTest, ReplacingSyncWithSignin_RunsOnlyOnce) {
  base::test::ScopedFeatureList feature_list(
      kReplaceSyncPromosWithSignInPromos);

  // The migration initially runs for a new user (not signed in yet). This does
  // not change any actual prefs, but marks the migration as "done".
  SyncPrefs(&pref_service_)
      .MaybeMigratePrefsForReplacingSyncWithSignin(
          SyncPrefs::SyncAccountState::kNotSignedIn);
  ASSERT_TRUE(BooleanUserPrefMatches(kPreferencesPref, PREF_UNSET));

  // Later, the user signs in. When the migration function gets triggered again
  // (typically at the next browser startup), it should *not* migrate anything.
  SyncPrefs(&pref_service_)
      .MaybeMigratePrefsForReplacingSyncWithSignin(
          SyncPrefs::SyncAccountState::kSignedInNotSyncing);

  // Nothing happened - pref is still unset.
  EXPECT_TRUE(BooleanUserPrefMatches(kPreferencesPref, PREF_UNSET));
}

TEST_F(SyncPrefsMigrationTest,
       ReplacingSyncWithSignin_RunsAgainAfterFeatureReenabled) {
  // Initial state: Preferences are enabled.
  SetBooleanUserPrefValue(kPreferencesPref, PREF_TRUE);

  // The feature gets enabled for the first time, and the migration runs.
  {
    base::test::ScopedFeatureList feature_list(
        kReplaceSyncPromosWithSignInPromos);

    SyncPrefs(&pref_service_)
        .MaybeMigratePrefsForReplacingSyncWithSignin(
            SyncPrefs::SyncAccountState::kSignedInNotSyncing);

    // Preferences got migrated to false.
    ASSERT_TRUE(BooleanUserPrefMatches(kPreferencesPref, PREF_FALSE));
  }

  // Reset Preferences to true so we can check whether the migration happened
  // again.
  SetBooleanUserPrefValue(kPreferencesPref, PREF_TRUE);

  // The feature gets disabled, and the migration logic gets triggered again on
  // the next browser startup.
  {
    base::test::ScopedFeatureList feature_list;
    feature_list.InitAndDisableFeature(kReplaceSyncPromosWithSignInPromos);

    SyncPrefs(&pref_service_)
        .MaybeMigratePrefsForReplacingSyncWithSignin(
            SyncPrefs::SyncAccountState::kSignedInNotSyncing);

    // Since the feature is disabled now, this didn't do anything - Preferences
    // is still true.
    ASSERT_TRUE(BooleanUserPrefMatches(kPreferencesPref, PREF_TRUE));
  }

  // The feature gets enabled for the second time, and the migration runs.
  // Since it was disabled in the between, the migration should run again
  {
    base::test::ScopedFeatureList feature_list(
        kReplaceSyncPromosWithSignInPromos);

    SyncPrefs(&pref_service_)
        .MaybeMigratePrefsForReplacingSyncWithSignin(
            SyncPrefs::SyncAccountState::kSignedInNotSyncing);

    // Preferences should have been migrated to false again.
    EXPECT_TRUE(BooleanUserPrefMatches(kPreferencesPref, PREF_FALSE));
  }
}

TEST_F(SyncPrefsMigrationTest, ReplacingSyncWithSignin_TurnsPreferencesOff) {
  base::test::ScopedFeatureList feature_list(
      kReplaceSyncPromosWithSignInPromos);

  ASSERT_TRUE(BooleanUserPrefMatches(kPreferencesPref, PREF_UNSET));

  // Run the migration for a pre-existing signed-in non-syncing user.
  SyncPrefs(&pref_service_)
      .MaybeMigratePrefsForReplacingSyncWithSignin(
          SyncPrefs::SyncAccountState::kSignedInNotSyncing);

  // Preferences should have been set to false.
  EXPECT_TRUE(BooleanUserPrefMatches(kPreferencesPref, PREF_FALSE));
}

TEST_F(SyncPrefsMigrationTest,
       ReplacingSyncWithSignin_MigratesBookmarksOptedIn) {
  {
    // The feature starts disabled.
    base::test::ScopedFeatureList feature_list;
    feature_list.InitAndDisableFeature(kReplaceSyncPromosWithSignInPromos);

    // Bookmarks and ReadingList are enabled (by default - the actual prefs are
    // not set explicitly). On iOS, an additional opt-in pref is required.
    ASSERT_TRUE(BooleanUserPrefMatches(kBookmarksPref, PREF_UNSET));
    ASSERT_TRUE(BooleanUserPrefMatches(kReadingListPref, PREF_UNSET));
#if BUILDFLAG(IS_IOS)
    SetBooleanUserPrefValue(
        prefs::internal::kBookmarksAndReadingListAccountStorageOptIn,
        PREF_TRUE);
#endif  // BUILDFLAG(IS_IOS)
    ASSERT_TRUE(
        SyncPrefs(&pref_service_)
            .GetSelectedTypes(SyncPrefs::SyncAccountState::kSignedInNotSyncing)
            .HasAll({UserSelectableType::kBookmarks,
                     UserSelectableType::kReadingList}));
  }

  {
    // Now (on the next browser restart) the feature gets enabled, and the
    // migration runs.
    base::test::ScopedFeatureList feature_list(
        kReplaceSyncPromosWithSignInPromos);

    SyncPrefs(&pref_service_)
        .MaybeMigratePrefsForReplacingSyncWithSignin(
            SyncPrefs::SyncAccountState::kSignedInNotSyncing);

    // Bookmarks and ReadingList should still be enabled (by default).
    EXPECT_TRUE(BooleanUserPrefMatches(kBookmarksPref, PREF_UNSET));
    EXPECT_TRUE(BooleanUserPrefMatches(kReadingListPref, PREF_UNSET));
    EXPECT_TRUE(
        SyncPrefs(&pref_service_)
            .GetSelectedTypes(SyncPrefs::SyncAccountState::kSignedInNotSyncing)
            .HasAll({UserSelectableType::kBookmarks,
                     UserSelectableType::kReadingList}));
  }
}

#if BUILDFLAG(IS_IOS)
TEST_F(SyncPrefsMigrationTest,
       ReplacingSyncWithSignin_MigratesBookmarksNotOptedIn) {
  {
    // The feature starts disabled.
    base::test::ScopedFeatureList feature_list;
    feature_list.InitAndDisableFeature(kReplaceSyncPromosWithSignInPromos);

    // The regular Bookmarks and ReadingList prefs are enabled, but the
    // additional opt-in pref is not.
    SetBooleanUserPrefValue(kBookmarksPref, PREF_TRUE);
    SetBooleanUserPrefValue(kReadingListPref, PREF_TRUE);
    ASSERT_EQ(GetBooleanUserPrefValue(
                  prefs::internal::kBookmarksAndReadingListAccountStorageOptIn),
              PREF_UNSET);
    ASSERT_FALSE(
        SyncPrefs(&pref_service_)
            .GetSelectedTypes(SyncPrefs::SyncAccountState::kSignedInNotSyncing)
            .HasAny({UserSelectableType::kBookmarks,
                     UserSelectableType::kReadingList}));
  }

  {
    // Now (on the next browser restart) the feature gets enabled, and the
    // migration runs.
    base::test::ScopedFeatureList feature_list(
        kReplaceSyncPromosWithSignInPromos);

    // Sanity check: Without the migration, Bookmarks and ReadingList would now
    // be considered enabled.
    ASSERT_TRUE(
        SyncPrefs(&pref_service_)
            .GetSelectedTypes(SyncPrefs::SyncAccountState::kSignedInNotSyncing)
            .HasAll({UserSelectableType::kBookmarks,
                     UserSelectableType::kReadingList}));

    // Run the migration!
    SyncPrefs(&pref_service_)
        .MaybeMigratePrefsForReplacingSyncWithSignin(
            SyncPrefs::SyncAccountState::kSignedInNotSyncing);

    // After the migration, bookmarks should be disabled.
    EXPECT_TRUE(BooleanUserPrefMatches(kBookmarksPref, PREF_FALSE));
    EXPECT_TRUE(BooleanUserPrefMatches(kReadingListPref, PREF_FALSE));
    EXPECT_FALSE(
        SyncPrefs(&pref_service_)
            .GetSelectedTypes(SyncPrefs::SyncAccountState::kSignedInNotSyncing)
            .HasAny({UserSelectableType::kBookmarks,
                     UserSelectableType::kReadingList}));
  }
}
#endif  // BUILDFLAG(IS_IOS)

TEST_F(SyncPrefsMigrationTest,
       ReplacingSyncWithSignin_LeavesAutofillAloneIfPasswordsOn) {
  base::test::ScopedFeatureList feature_list(
      kReplaceSyncPromosWithSignInPromos);

  // Passwords and Autofill are enabled (by default - the actual prefs are not
  // set explicitly).
  ASSERT_TRUE(BooleanUserPrefMatches(kPasswordsPref, PREF_UNSET));
  ASSERT_TRUE(BooleanUserPrefMatches(kAutofillPref, PREF_UNSET));

  // Run the migration for a pre-existing signed-in non-syncing user.
  SyncPrefs(&pref_service_)
      .MaybeMigratePrefsForReplacingSyncWithSignin(
          SyncPrefs::SyncAccountState::kSignedInNotSyncing);

  // Autofill should still be unset.
  EXPECT_TRUE(BooleanUserPrefMatches(kAutofillPref, PREF_UNSET));
}

TEST_F(SyncPrefsMigrationTest,
       ReplacingSyncWithSignin_TurnsAutofillOffIfPasswordsOff) {
  base::test::ScopedFeatureList feature_list(
      kReplaceSyncPromosWithSignInPromos);

  // Autofill is enabled (by default; not set explicitly), but Passwords is
  // explicitly disabled.
  ASSERT_TRUE(BooleanUserPrefMatches(kAutofillPref, PREF_UNSET));
  SetBooleanUserPrefValue(kPasswordsPref, PREF_FALSE);

  // Run the migration for a pre-existing signed-in non-syncing user.
  SyncPrefs(&pref_service_)
      .MaybeMigratePrefsForReplacingSyncWithSignin(
          SyncPrefs::SyncAccountState::kSignedInNotSyncing);

  // Autofill should have been set to false, since the user specifically opted
  // out of Passwords.
  EXPECT_TRUE(BooleanUserPrefMatches(kAutofillPref, PREF_FALSE));
}

}  // namespace

}  // namespace syncer
