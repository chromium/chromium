// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/service/sync_prefs.h"

#include <memory>
#include <vector>

#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "build/chromeos_buildflags.h"
#include "components/password_manager/core/browser/features/password_features.h"
#include "components/prefs/pref_value_map.h"
#include "components/prefs/testing_pref_service.h"
#include "components/signin/public/base/gaia_id_hash.h"
#include "components/sync/base/features.h"
#include "components/sync/base/pref_names.h"
#include "components/sync/base/user_selectable_type.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace syncer {

namespace {

using ::testing::InSequence;
using ::testing::StrictMock;

// Copy of the same constant in sync_prefs.cc, for testing purposes.
constexpr char kObsoleteAutofillWalletImportEnabled[] =
    "autofill.wallet_import_enabled";

class SyncPrefsTest : public testing::Test {
 protected:
  SyncPrefsTest() {
    SyncPrefs::RegisterProfilePrefs(pref_service_.registry());
    sync_prefs_ = std::make_unique<SyncPrefs>(&pref_service_);
    gaia_id_hash_ = signin::GaiaIdHash::FromGaiaId("account_gaia");
  }

  base::test::SingleThreadTaskEnvironment task_environment_;
  TestingPrefServiceSimple pref_service_;
  std::unique_ptr<SyncPrefs> sync_prefs_;
  signin::GaiaIdHash gaia_id_hash_;
};

TEST_F(SyncPrefsTest, EncryptionBootstrapToken) {
  EXPECT_TRUE(sync_prefs_->GetEncryptionBootstrapToken().empty());
  sync_prefs_->SetEncryptionBootstrapToken("token");
  EXPECT_EQ("token", sync_prefs_->GetEncryptionBootstrapToken());
  sync_prefs_->ClearEncryptionBootstrapToken();
  EXPECT_TRUE(sync_prefs_->GetEncryptionBootstrapToken().empty());
}

TEST_F(SyncPrefsTest, CachedPassphraseType) {
  EXPECT_FALSE(sync_prefs_->GetCachedPassphraseType().has_value());

  sync_prefs_->SetCachedPassphraseType(PassphraseType::kKeystorePassphrase);
  EXPECT_EQ(PassphraseType::kKeystorePassphrase,
            sync_prefs_->GetCachedPassphraseType());

  sync_prefs_->SetCachedPassphraseType(PassphraseType::kCustomPassphrase);
  EXPECT_EQ(PassphraseType::kCustomPassphrase,
            sync_prefs_->GetCachedPassphraseType());

  sync_prefs_->ClearCachedPassphraseType();
  EXPECT_FALSE(sync_prefs_->GetCachedPassphraseType().has_value());
}

class MockSyncPrefObserver : public SyncPrefObserver {
 public:
  MOCK_METHOD(void, OnSyncManagedPrefChange, (bool), (override));
#if !BUILDFLAG(IS_CHROMEOS_ASH)
  MOCK_METHOD(void, OnFirstSetupCompletePrefChange, (bool), (override));
#endif  // !BUILDFLAG(IS_CHROMEOS_ASH)
  MOCK_METHOD(void, OnPreferredDataTypesPrefChange, (bool), (override));
};

TEST_F(SyncPrefsTest, ObservedPrefs) {
  StrictMock<MockSyncPrefObserver> mock_sync_pref_observer;
  InSequence dummy;
  EXPECT_CALL(mock_sync_pref_observer, OnSyncManagedPrefChange(true));
  EXPECT_CALL(mock_sync_pref_observer, OnSyncManagedPrefChange(false));

  ASSERT_FALSE(sync_prefs_->IsSyncClientDisabledByPolicy());

  sync_prefs_->AddObserver(&mock_sync_pref_observer);

  pref_service_.SetBoolean(prefs::internal::kSyncManaged, true);
  EXPECT_TRUE(sync_prefs_->IsSyncClientDisabledByPolicy());
  pref_service_.SetBoolean(prefs::internal::kSyncManaged, false);
  EXPECT_FALSE(sync_prefs_->IsSyncClientDisabledByPolicy());

  sync_prefs_->RemoveObserver(&mock_sync_pref_observer);
}

#if !BUILDFLAG(IS_CHROMEOS_ASH)
TEST_F(SyncPrefsTest, FirstSetupCompletePrefChange) {
  StrictMock<MockSyncPrefObserver> mock_sync_pref_observer;
  InSequence dummy;

  EXPECT_CALL(mock_sync_pref_observer, OnFirstSetupCompletePrefChange(true));
  EXPECT_CALL(mock_sync_pref_observer, OnFirstSetupCompletePrefChange(false));

  ASSERT_FALSE(sync_prefs_->IsInitialSyncFeatureSetupComplete());

  sync_prefs_->AddObserver(&mock_sync_pref_observer);

  sync_prefs_->SetInitialSyncFeatureSetupComplete();
  EXPECT_TRUE(sync_prefs_->IsInitialSyncFeatureSetupComplete());
  sync_prefs_->ClearInitialSyncFeatureSetupComplete();
  EXPECT_FALSE(sync_prefs_->IsInitialSyncFeatureSetupComplete());

  sync_prefs_->RemoveObserver(&mock_sync_pref_observer);
}
#endif  // !BUILDFLAG(IS_CHROMEOS_ASH)

#if BUILDFLAG(IS_CHROMEOS_ASH)
TEST_F(SyncPrefsTest, SyncFeatureDisabledViaDashboard) {
  EXPECT_FALSE(sync_prefs_->IsSyncFeatureDisabledViaDashboard());

  sync_prefs_->SetSyncFeatureDisabledViaDashboard();
  EXPECT_TRUE(sync_prefs_->IsSyncFeatureDisabledViaDashboard());

  sync_prefs_->ClearSyncFeatureDisabledViaDashboard();
  EXPECT_FALSE(sync_prefs_->IsSyncFeatureDisabledViaDashboard());
}

TEST_F(SyncPrefsTest, SetSelectedOsTypesTriggersPreferredDataTypesPrefChange) {
  StrictMock<MockSyncPrefObserver> mock_sync_pref_observer;
  EXPECT_CALL(mock_sync_pref_observer,
              OnPreferredDataTypesPrefChange(
                  /*payments_integration_enabled_changed=*/false));

  sync_prefs_->AddObserver(&mock_sync_pref_observer);
  sync_prefs_->SetSelectedOsTypes(/*sync_all_os_types=*/false,
                                  UserSelectableOsTypeSet(),
                                  UserSelectableOsTypeSet());
  sync_prefs_->RemoveObserver(&mock_sync_pref_observer);
}
#endif

TEST_F(SyncPrefsTest, Basic) {
#if !BUILDFLAG(IS_CHROMEOS_ASH)
  EXPECT_FALSE(sync_prefs_->IsInitialSyncFeatureSetupComplete());
  sync_prefs_->SetInitialSyncFeatureSetupComplete();
#endif  // !BUILDFLAG(IS_CHROMEOS_ASH)

  EXPECT_TRUE(sync_prefs_->IsInitialSyncFeatureSetupComplete());

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
  EXPECT_FALSE(
      sync_prefs_->IsTypeManagedByCustodian(UserSelectableType::kBookmarks));
  // Other types should be unaffected.
  EXPECT_TRUE(
      sync_prefs_->GetSelectedTypes(SyncPrefs::SyncAccountState::kSyncing)
          .Has(UserSelectableType::kAutofill));
  EXPECT_FALSE(
      sync_prefs_->IsTypeManagedByPolicy(UserSelectableType::kAutofill));
}

TEST_F(SyncPrefsTest, SetTypeDisabledByCustodian) {
  // By default, data types are enabled, and not custodian-controlled.
  ASSERT_TRUE(
      sync_prefs_->GetSelectedTypes(SyncPrefs::SyncAccountState::kSyncing)
          .Has(UserSelectableType::kBookmarks));
  ASSERT_FALSE(
      sync_prefs_->IsTypeManagedByCustodian(UserSelectableType::kBookmarks));
  ASSERT_TRUE(
      sync_prefs_->GetSelectedTypes(SyncPrefs::SyncAccountState::kSyncing)
          .Has(UserSelectableType::kAutofill));
  ASSERT_FALSE(
      sync_prefs_->IsTypeManagedByCustodian(UserSelectableType::kAutofill));

  // Set up a custodian enforcement to disable bookmarks.
  PrefValueMap supervised_user_prefs;
  SyncPrefs::SetTypeDisabledByCustodian(&supervised_user_prefs,
                                        UserSelectableType::kBookmarks);
  // Copy the supervised user prefs map over into the PrefService.
  for (const auto& supervised_user_pref : supervised_user_prefs) {
    pref_service_.SetSupervisedUserPref(supervised_user_pref.first,
                                        supervised_user_pref.second.Clone());
  }

  // The restriction should take effect and disable bookmarks.
  EXPECT_FALSE(
      sync_prefs_->GetSelectedTypes(SyncPrefs::SyncAccountState::kSyncing)
          .Has(UserSelectableType::kBookmarks));
  EXPECT_TRUE(
      sync_prefs_->IsTypeManagedByCustodian(UserSelectableType::kBookmarks));
  EXPECT_FALSE(
      sync_prefs_->IsTypeManagedByPolicy(UserSelectableType::kBookmarks));
  // Other types should be unaffected.
  EXPECT_TRUE(
      sync_prefs_->GetSelectedTypes(SyncPrefs::SyncAccountState::kSyncing)
          .Has(UserSelectableType::kAutofill));
  EXPECT_FALSE(
      sync_prefs_->IsTypeManagedByCustodian(UserSelectableType::kAutofill));
}

TEST_F(SyncPrefsTest, DefaultSelectedTypesInTransportMode) {
  base::test::ScopedFeatureList features;
  features.InitWithFeatures(
      /*enabled_features=*/{kEnableBookmarksAccountStorage,
                            kReadingListEnableDualReadingListModel,
                            kReadingListEnableSyncTransportModeUponSignIn,
                            password_manager::features::
                                kEnablePasswordsAccountStorage,
                            kSyncEnableContactInfoDataTypeInTransportMode,
                            kEnablePreferencesAccountStorage},
      /*disabled_features=*/{kReplaceSyncPromosWithSignInPromos});

  // Based on the feature flags set above, Bookmarks, ReadingList, Passwords,
  // Autofill and Payments are supported and enabled by default.
  // Preferences, History, and Tabs are not supported without
  // kReplaceSyncPromosWithSignInPromos.
  UserSelectableTypeSet expected_types{
      UserSelectableType::kBookmarks, UserSelectableType::kReadingList,
      UserSelectableType::kPasswords, UserSelectableType::kAutofill,
      UserSelectableType::kPayments};

#if BUILDFLAG(IS_IOS)
  // On iOS, Bookmarks and Reading list require a dedicated opt-in.
  EXPECT_EQ(
      sync_prefs_->GetSelectedTypes(
          SyncPrefs::SyncAccountState::kSignedInNotSyncing),
      base::Difference(expected_types, {UserSelectableType::kBookmarks,
                                        UserSelectableType::kReadingList}));

  sync_prefs_->SetBookmarksAndReadingListAccountStorageOptIn(true);
#endif

  EXPECT_EQ(sync_prefs_->GetSelectedTypes(
                SyncPrefs::SyncAccountState::kSignedInNotSyncing),
            expected_types);
}

TEST_F(SyncPrefsTest, DefaultSelectedTypesForAccountInTransportMode) {
  base::test::ScopedFeatureList features;
  features.InitWithFeatures(
      /*enabled_features=*/{kReplaceSyncPromosWithSignInPromos,
                            kEnableBookmarksAccountStorage,
                            kReadingListEnableDualReadingListModel,
                            kReadingListEnableSyncTransportModeUponSignIn,
                            password_manager::features::
                                kEnablePasswordsAccountStorage,
                            kSyncEnableContactInfoDataTypeInTransportMode,
                            kEnablePreferencesAccountStorage},
      /*disabled_features=*/{});

  // Based on the feature flags set above, Bookmarks, ReadingList, Passwords,
  // Autofill, Payments and Preferences are supported and enabled by default.
  // (History and Tabs are also supported, but require a separate opt-in.)
  UserSelectableTypeSet expected_types{
      UserSelectableType::kBookmarks, UserSelectableType::kReadingList,
      UserSelectableType::kPasswords, UserSelectableType::kAutofill,
      UserSelectableType::kPayments,  UserSelectableType::kPreferences};
  EXPECT_EQ(sync_prefs_->GetSelectedTypesForAccount(gaia_id_hash_),
            expected_types);
}

TEST_F(SyncPrefsTest, SetSelectedTypesInTransportMode) {
  base::test::ScopedFeatureList features;
  features.InitWithFeatures(
      /*enabled_features=*/{kEnableBookmarksAccountStorage,
                            kReadingListEnableDualReadingListModel,
                            kReadingListEnableSyncTransportModeUponSignIn,
                            password_manager::features::
                                kEnablePasswordsAccountStorage,
                            kSyncEnableContactInfoDataTypeInTransportMode},
      /*disabled_features=*/{kReplaceSyncPromosWithSignInPromos});

#if BUILDFLAG(IS_IOS)
  // On iOS, Bookmarks and Reading list require a dedicated opt-in.
  sync_prefs_->SetBookmarksAndReadingListAccountStorageOptIn(true);
#endif

  const UserSelectableTypeSet new_types = {UserSelectableType::kAutofill,
                                           UserSelectableType::kPasswords};
  ASSERT_NE(sync_prefs_->GetSelectedTypes(
                SyncPrefs::SyncAccountState::kSignedInNotSyncing),
            new_types);

  sync_prefs_->SetSelectedTypes(
      /*keep_everything_synced=*/false,
      /*registered_types=*/UserSelectableTypeSet::All(),
      /*selected_types=*/new_types);

  EXPECT_EQ(sync_prefs_->GetSelectedTypes(
                SyncPrefs::SyncAccountState::kSignedInNotSyncing),
            new_types);

  // Pass keep_everything_synced true to verify that it has no effect in
  // transport-mode.
  sync_prefs_->SetSelectedTypes(
      /*keep_everything_synced=*/true,
      /*registered_types=*/UserSelectableTypeSet::All(),
      /*selected_types=*/new_types);

  EXPECT_EQ(sync_prefs_->GetSelectedTypes(
                SyncPrefs::SyncAccountState::kSignedInNotSyncing),
            new_types);
}

TEST_F(SyncPrefsTest, SetSelectedTypesForAccountInTransportMode) {
  base::test::ScopedFeatureList features;
  features.InitWithFeatures(
      /*enabled_features=*/{kReplaceSyncPromosWithSignInPromos,
                            password_manager::features::
                                kEnablePasswordsAccountStorage},
      /*disabled_features=*/{});

  const UserSelectableTypeSet default_selected_types =
      sync_prefs_->GetSelectedTypesForAccount(gaia_id_hash_);
  ASSERT_TRUE(default_selected_types.Has(UserSelectableType::kPasswords));

  // Change one of the default values for example kPasswords.
  sync_prefs_->SetSelectedTypeForAccount(UserSelectableType::kPasswords, false,
                                         gaia_id_hash_);

  // kPasswords should be disabled, other default values should be unaffected.
  EXPECT_EQ(
      sync_prefs_->GetSelectedTypesForAccount(gaia_id_hash_),
      Difference(default_selected_types, {UserSelectableType::kPasswords}));
  // Other accounts should be unnafected.
  EXPECT_EQ(sync_prefs_->GetSelectedTypesForAccount(
                signin::GaiaIdHash::FromGaiaId("account_gaia_2")),
            default_selected_types);
}

TEST_F(SyncPrefsTest, SetSelectedTypesInTransportModeWithPolicyRestrictedType) {
  base::test::ScopedFeatureList features;
  features.InitWithFeatures(
      /*enabled_features=*/{password_manager::features::
                                kEnablePasswordsAccountStorage},
      /*disabled_features=*/{kReplaceSyncPromosWithSignInPromos});

  // Passwords is disabled by policy.
  pref_service_.SetManagedPref(prefs::internal::kSyncPasswords,
                               base::Value(false));

  // kPasswords should be disabled.
  UserSelectableTypeSet selected_types = sync_prefs_->GetSelectedTypes(
      SyncPrefs::SyncAccountState::kSignedInNotSyncing);
  ASSERT_FALSE(selected_types.Empty());
  EXPECT_FALSE(selected_types.Has(UserSelectableType::kPasswords));

  // User tries to enable kPasswords.
  sync_prefs_->SetSelectedTypes(
      /*keep_everything_synced=*/false,
      /*registered_types=*/UserSelectableTypeSet::All(),
      /*selected_types=*/{UserSelectableType::kPasswords});

  // kPasswords should still be disabled.
  EXPECT_FALSE(
      sync_prefs_
          ->GetSelectedTypes(SyncPrefs::SyncAccountState::kSignedInNotSyncing)
          .Has(UserSelectableType::kPasswords));
}

TEST_F(SyncPrefsTest,
       SetSelectedTypesForAccountInTransportModeWithPolicyRestrictedType) {
  base::test::ScopedFeatureList features;
  features.InitWithFeatures(
      /*enabled_features=*/{kReplaceSyncPromosWithSignInPromos,
                            password_manager::features::
                                kEnablePasswordsAccountStorage},
      /*disabled_features=*/{});

  // Passwords is disabled by policy.
  pref_service_.SetManagedPref(prefs::internal::kSyncPasswords,
                               base::Value(false));

  // kPasswords should be disabled.
  UserSelectableTypeSet selected_types =
      sync_prefs_->GetSelectedTypesForAccount(gaia_id_hash_);
  ASSERT_FALSE(selected_types.Empty());
  EXPECT_FALSE(selected_types.Has(UserSelectableType::kPasswords));

  // User tries to enable kPasswords.
  sync_prefs_->SetSelectedTypeForAccount(UserSelectableType::kPasswords, true,
                                         gaia_id_hash_);

  // kPasswords should still be disabled.
  EXPECT_FALSE(sync_prefs_->GetSelectedTypesForAccount(gaia_id_hash_)
                   .Has(UserSelectableType::kPasswords));
}

TEST_F(SyncPrefsTest, KeepAccountSettingsPrefsOnlyForUsers) {
  base::test::ScopedFeatureList enable_sync_to_signin(
      kReplaceSyncPromosWithSignInPromos);

  const UserSelectableTypeSet default_selected_types =
      sync_prefs_->GetSelectedTypesForAccount(gaia_id_hash_);

  auto gaia_id_hash_2 = signin::GaiaIdHash::FromGaiaId("account_gaia_2");

  // Change one of the default values for example kPasswords for account 1.
  sync_prefs_->SetSelectedTypeForAccount(UserSelectableType::kPasswords, false,
                                         gaia_id_hash_);
  // Change one of the default values for example kReadingList for account 2.
  sync_prefs_->SetSelectedTypeForAccount(UserSelectableType::kReadingList,
                                         false, gaia_id_hash_2);
  ASSERT_EQ(
      sync_prefs_->GetSelectedTypesForAccount(gaia_id_hash_),
      Difference(default_selected_types, {UserSelectableType::kPasswords}));
  ASSERT_EQ(
      sync_prefs_->GetSelectedTypesForAccount(gaia_id_hash_2),
      Difference(default_selected_types, {UserSelectableType::kReadingList}));

  // Remove account 2 from device by setting the available_gaia_ids to have the
  // gaia id of account 1 only.
  sync_prefs_->KeepAccountSettingsPrefsOnlyForUsers(
      /*available_gaia_ids=*/{gaia_id_hash_});

  // Nothing should change on account 1.
  EXPECT_EQ(
      sync_prefs_->GetSelectedTypesForAccount(gaia_id_hash_),
      Difference(default_selected_types, {UserSelectableType::kPasswords}));
  // Account 2 should be cleared to default values.
  EXPECT_EQ(sync_prefs_->GetSelectedTypesForAccount(gaia_id_hash_2),
            default_selected_types);
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
  sync_prefs_->AddObserver(&mock_sync_pref_observer);

  EXPECT_CALL(mock_sync_pref_observer,
              OnPreferredDataTypesPrefChange(
                  /*payments_integration_enabled_changed=*/false));
  sync_prefs_->SetAppsSyncEnabledByOs(/*apps_sync_enabled=*/true);
  EXPECT_TRUE(sync_prefs_->IsAppsSyncEnabledByOs());

  testing::Mock::VerifyAndClearExpectations(&mock_sync_pref_observer);
  EXPECT_CALL(mock_sync_pref_observer,
              OnPreferredDataTypesPrefChange(
                  /*payments_integration_enabled_changed=*/false));
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
    // Enable various features that are required for data types to be supported
    // in transport mode.
    feature_list_.InitWithFeatures(
        /*enabled_features=*/{kEnableBookmarksAccountStorage,
                              kReadingListEnableDualReadingListModel,
                              kReadingListEnableSyncTransportModeUponSignIn,
                              password_manager::features::
                                  kEnablePasswordsAccountStorage,
                              kSyncEnableContactInfoDataTypeInTransportMode,
                              kEnablePreferencesAccountStorage},
        /*disabled_features=*/{});

    SyncPrefs::RegisterProfilePrefs(pref_service_.registry());
    gaia_id_hash_ = signin::GaiaIdHash::FromGaiaId("account_gaia");
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

  // Global prefs for syncing users, affecting all accounts.
  const char* kGlobalBookmarksPref =
      SyncPrefs::GetPrefNameForTypeForTesting(UserSelectableType::kBookmarks);
  const char* kGlobalReadingListPref =
      SyncPrefs::GetPrefNameForTypeForTesting(UserSelectableType::kReadingList);
  const char* kGlobalPasswordsPref =
      SyncPrefs::GetPrefNameForTypeForTesting(UserSelectableType::kPasswords);
  const char* kGlobalAutofillPref =
      SyncPrefs::GetPrefNameForTypeForTesting(UserSelectableType::kAutofill);
  const char* kGlobalPaymentsPref =
      SyncPrefs::GetPrefNameForTypeForTesting(UserSelectableType::kPayments);
  const char* kGlobalPreferencesPref =
      SyncPrefs::GetPrefNameForTypeForTesting(UserSelectableType::kPreferences);

  base::test::ScopedFeatureList feature_list_;
  base::test::SingleThreadTaskEnvironment task_environment_;

  TestingPrefServiceSimple pref_service_;
  signin::GaiaIdHash gaia_id_hash_;
};

TEST_F(SyncPrefsMigrationTest, MigrateAutofillWalletImportEnabledPrefIfSet) {
  pref_service_.SetBoolean(kObsoleteAutofillWalletImportEnabled, false);
  ASSERT_TRUE(
      pref_service_.GetUserPrefValue(kObsoleteAutofillWalletImportEnabled));

  SyncPrefs::MigrateAutofillWalletImportEnabledPref(&pref_service_);

  SyncPrefs prefs(&pref_service_);

  EXPECT_TRUE(pref_service_.GetUserPrefValue(
      SyncPrefs::GetPrefNameForTypeForTesting(UserSelectableType::kPayments)));
  EXPECT_FALSE(pref_service_.GetBoolean(
      SyncPrefs::GetPrefNameForTypeForTesting(UserSelectableType::kPayments)));
}

TEST_F(SyncPrefsMigrationTest, MigrateAutofillWalletImportEnabledPrefIfUnset) {
  ASSERT_FALSE(
      pref_service_.GetUserPrefValue(kObsoleteAutofillWalletImportEnabled));

  SyncPrefs::MigrateAutofillWalletImportEnabledPref(&pref_service_);

  SyncPrefs prefs(&pref_service_);

  EXPECT_FALSE(pref_service_.GetUserPrefValue(
      SyncPrefs::GetPrefNameForTypeForTesting(UserSelectableType::kPayments)));
}

// Regression test for crbug.com/1467307.
TEST_F(SyncPrefsMigrationTest,
       MigrateAutofillWalletImportEnabledPrefIfUnsetWithSyncEverythingOff) {
  // Mimic an old profile where sync-everything was turned off without
  // populating kObsoleteAutofillWalletImportEnabled (i.e. before the UI
  // included the payments toggle).
  pref_service_.SetBoolean(prefs::internal::kSyncKeepEverythingSynced, false);

  ASSERT_FALSE(
      pref_service_.GetUserPrefValue(kObsoleteAutofillWalletImportEnabled));

  SyncPrefs::MigrateAutofillWalletImportEnabledPref(&pref_service_);

  SyncPrefs prefs(&pref_service_);

  EXPECT_TRUE(pref_service_.GetUserPrefValue(
      SyncPrefs::GetPrefNameForTypeForTesting(UserSelectableType::kPayments)));
  EXPECT_TRUE(pref_service_.GetBoolean(
      SyncPrefs::GetPrefNameForTypeForTesting(UserSelectableType::kPayments)));
}

TEST_F(SyncPrefsMigrationTest, SyncToSignin_NoMigrationForSignedOutUser) {
  base::test::ScopedFeatureList enable_sync_to_signin(
      kReplaceSyncPromosWithSignInPromos);

  EXPECT_FALSE(
      SyncPrefs(&pref_service_)
          .MaybeMigratePrefsForSyncToSigninPart1(
              SyncPrefs::SyncAccountState::kNotSignedIn, signin::GaiaIdHash()));
  // Part 2 isn't called because the engine isn't initialized.
}

TEST_F(SyncPrefsMigrationTest, SyncToSignin_NoMigrationForSyncingUser) {
  base::test::ScopedFeatureList enable_sync_to_signin(
      kReplaceSyncPromosWithSignInPromos);

  SyncPrefs prefs(&pref_service_);
  EXPECT_FALSE(prefs.MaybeMigratePrefsForSyncToSigninPart1(
      SyncPrefs::SyncAccountState::kSyncing, gaia_id_hash_));
  EXPECT_FALSE(prefs.MaybeMigratePrefsForSyncToSigninPart2(
      gaia_id_hash_,
      /*is_using_explicit_passphrase=*/true));
}

TEST_F(SyncPrefsMigrationTest, SyncToSignin_RunsOnlyOnce) {
  base::test::ScopedFeatureList enable_sync_to_signin(
      kReplaceSyncPromosWithSignInPromos);

  {
    SyncPrefs prefs(&pref_service_);

    // The user is signed-out, so the migration should not run and it should be
    // be marked as done. MaybeMigratePrefsForSyncToSigninPart2() isn't called
    // yet, because the sync engine wasn't initialized.
    ASSERT_FALSE(prefs.MaybeMigratePrefsForSyncToSigninPart1(
        SyncPrefs::SyncAccountState::kNotSignedIn, signin::GaiaIdHash()));

    // The user signs in, causing the engine to initialize and the call to part
    // 2. The migration should not run, because this wasn't an *existing*
    // signed-in user.
    EXPECT_FALSE(prefs.MaybeMigratePrefsForSyncToSigninPart2(
        gaia_id_hash_,
        /*is_using_explicit_passphrase=*/true));
  }

  // The browser is restarted.
  {
    SyncPrefs prefs(&pref_service_);

    // Both methods are called. No migration should run.
    EXPECT_FALSE(prefs.MaybeMigratePrefsForSyncToSigninPart1(
        SyncPrefs::SyncAccountState::kSignedInNotSyncing, gaia_id_hash_));
    EXPECT_FALSE(prefs.MaybeMigratePrefsForSyncToSigninPart2(
        gaia_id_hash_,
        /*is_using_explicit_passphrase=*/true));
  }
}

TEST_F(SyncPrefsMigrationTest, SyncToSignin_RunsAgainAfterFeatureReenabled) {
  // The feature gets enabled for the first time.
  {
    base::test::ScopedFeatureList enable_sync_to_signin(
        kReplaceSyncPromosWithSignInPromos);

    SyncPrefs prefs(&pref_service_);

    // The user is signed-in non-syncing, so part 1 runs. The user also has an
    // explicit passphrase, so part 2 runs too.
    EXPECT_TRUE(prefs.MaybeMigratePrefsForSyncToSigninPart1(
        SyncPrefs::SyncAccountState::kSignedInNotSyncing, gaia_id_hash_));
    EXPECT_TRUE(prefs.MaybeMigratePrefsForSyncToSigninPart2(
        gaia_id_hash_,
        /*is_using_explicit_passphrase=*/true));
  }

  // On the next startup, the feature is disabled.
  {
    base::test::ScopedFeatureList disable_sync_to_signin;
    disable_sync_to_signin.InitAndDisableFeature(
        kReplaceSyncPromosWithSignInPromos);

    SyncPrefs prefs(&pref_service_);

    // Since the feature is disabled now, no migration runs.
    EXPECT_FALSE(prefs.MaybeMigratePrefsForSyncToSigninPart1(
        SyncPrefs::SyncAccountState::kSignedInNotSyncing, gaia_id_hash_));
    EXPECT_FALSE(prefs.MaybeMigratePrefsForSyncToSigninPart2(
        gaia_id_hash_,
        /*is_using_explicit_passphrase=*/true));
  }

  // On the next startup, the feature is enabled again.
  {
    base::test::ScopedFeatureList enable_sync_to_signin(
        kReplaceSyncPromosWithSignInPromos);

    SyncPrefs prefs(&pref_service_);

    // Since it was disabled in between, the migration should run again.
    EXPECT_TRUE(prefs.MaybeMigratePrefsForSyncToSigninPart1(
        SyncPrefs::SyncAccountState::kSignedInNotSyncing, gaia_id_hash_));
    EXPECT_TRUE(prefs.MaybeMigratePrefsForSyncToSigninPart2(
        gaia_id_hash_,
        /*is_using_explicit_passphrase=*/true));
  }
}

TEST_F(SyncPrefsMigrationTest, SyncToSignin_GlobalPrefsAreUnchanged) {
  base::test::ScopedFeatureList enable_sync_to_signin(
      kReplaceSyncPromosWithSignInPromos);

  for (UserSelectableType type : UserSelectableTypeSet::All()) {
    ASSERT_TRUE(
        BooleanUserPrefMatches(SyncPrefs::GetPrefNameForTypeForTesting(type),
                               BooleanPrefState::PREF_UNSET));
  }

  SyncPrefs prefs(&pref_service_);

  ASSERT_TRUE(prefs.MaybeMigratePrefsForSyncToSigninPart1(
      SyncPrefs::SyncAccountState::kSignedInNotSyncing, gaia_id_hash_));
  ASSERT_TRUE(prefs.MaybeMigratePrefsForSyncToSigninPart2(
      gaia_id_hash_,
      /*is_using_explicit_passphrase=*/true));

  for (UserSelectableType type : UserSelectableTypeSet::All()) {
    EXPECT_TRUE(
        BooleanUserPrefMatches(SyncPrefs::GetPrefNameForTypeForTesting(type),
                               BooleanPrefState::PREF_UNSET));
  }
}

TEST_F(SyncPrefsMigrationTest, SyncToSignin_TurnsPreferencesOff) {
  base::test::ScopedFeatureList enable_sync_to_signin(
      kReplaceSyncPromosWithSignInPromos);

  SyncPrefs prefs(&pref_service_);

  // Pre-migration, preferences is enabled by default.
  ASSERT_TRUE(prefs.GetSelectedTypesForAccount(gaia_id_hash_)
                  .Has(UserSelectableType::kPreferences));

  // Run the migration for a pre-existing signed-in non-syncing user.
  prefs.MaybeMigratePrefsForSyncToSigninPart1(
      SyncPrefs::SyncAccountState::kSignedInNotSyncing, gaia_id_hash_);

  // Preferences should've been turned off in the account-scoped settings.
  EXPECT_FALSE(prefs.GetSelectedTypesForAccount(gaia_id_hash_)
                   .Has(UserSelectableType::kPreferences));
}

TEST_F(SyncPrefsMigrationTest, SyncToSignin_MigratesBookmarksOptedIn) {
  {
    // The SyncToSignin feature starts disabled.
    base::test::ScopedFeatureList disable_sync_to_signin;
    disable_sync_to_signin.InitAndDisableFeature(
        kReplaceSyncPromosWithSignInPromos);

    // Bookmarks and ReadingList are enabled (by default - the actual prefs are
    // not set explicitly). On iOS, an additional opt-in pref is required.
    SyncPrefs prefs(&pref_service_);
#if BUILDFLAG(IS_IOS)
    prefs.SetBookmarksAndReadingListAccountStorageOptIn(true);
#endif  // BUILDFLAG(IS_IOS)
    ASSERT_TRUE(
        prefs.GetSelectedTypes(SyncPrefs::SyncAccountState::kSignedInNotSyncing)
            .Has(UserSelectableType::kBookmarks));
    ASSERT_TRUE(
        prefs.GetSelectedTypes(SyncPrefs::SyncAccountState::kSignedInNotSyncing)
            .Has(UserSelectableType::kReadingList));
  }

  {
    // Now (on the next browser restart) the SyncToSignin feature gets enabled,
    // and the migration runs.
    base::test::ScopedFeatureList enable_sync_to_signin(
        kReplaceSyncPromosWithSignInPromos);

    SyncPrefs prefs(&pref_service_);

    ASSERT_TRUE(prefs.GetSelectedTypesForAccount(gaia_id_hash_)
                    .Has(UserSelectableType::kBookmarks));
    ASSERT_TRUE(prefs.GetSelectedTypesForAccount(gaia_id_hash_)
                    .Has(UserSelectableType::kReadingList));

    prefs.MaybeMigratePrefsForSyncToSigninPart1(
        SyncPrefs::SyncAccountState::kSignedInNotSyncing, gaia_id_hash_);

    // Bookmarks and ReadingList should still be enabled.
    EXPECT_TRUE(prefs.GetSelectedTypesForAccount(gaia_id_hash_)
                    .Has(UserSelectableType::kBookmarks));
    EXPECT_TRUE(prefs.GetSelectedTypesForAccount(gaia_id_hash_)
                    .Has(UserSelectableType::kReadingList));
  }
}

#if BUILDFLAG(IS_IOS)
TEST_F(SyncPrefsMigrationTest, SyncToSignin_MigratesBookmarksNotOptedIn) {
  {
    // The SyncToSignin feature starts disabled.
    base::test::ScopedFeatureList disable_sync_to_signin;
    disable_sync_to_signin.InitAndDisableFeature(
        kReplaceSyncPromosWithSignInPromos);

    // The regular Bookmarks and ReadingList prefs are enabled (by default), but
    // the additional opt-in pref is not.
    SyncPrefs prefs(&pref_service_);
    ASSERT_FALSE(
        prefs.GetSelectedTypes(SyncPrefs::SyncAccountState::kSignedInNotSyncing)
            .Has(UserSelectableType::kBookmarks));
    ASSERT_FALSE(
        prefs.GetSelectedTypes(SyncPrefs::SyncAccountState::kSignedInNotSyncing)
            .Has(UserSelectableType::kReadingList));
  }

  {
    // Now (on the next browser restart) the SyncToSignin feature gets enabled,
    // and the migration runs.
    base::test::ScopedFeatureList enable_sync_to_signin(
        kReplaceSyncPromosWithSignInPromos);

    SyncPrefs prefs(&pref_service_);

    // Sanity check: Without the migration, Bookmarks and ReadingList would now
    // be considered enabled.
    ASSERT_TRUE(prefs.GetSelectedTypesForAccount(gaia_id_hash_)
                    .Has(UserSelectableType::kBookmarks));
    ASSERT_TRUE(prefs.GetSelectedTypesForAccount(gaia_id_hash_)
                    .Has(UserSelectableType::kReadingList));

    // Run the migration!
    prefs.MaybeMigratePrefsForSyncToSigninPart1(
        SyncPrefs::SyncAccountState::kSignedInNotSyncing, gaia_id_hash_);

    // After the migration, the types should be disabled.
    EXPECT_FALSE(prefs.GetSelectedTypesForAccount(gaia_id_hash_)
                     .Has(UserSelectableType::kBookmarks));
    EXPECT_FALSE(prefs.GetSelectedTypesForAccount(gaia_id_hash_)
                     .Has(UserSelectableType::kReadingList));
  }
}
#endif  // BUILDFLAG(IS_IOS)

TEST_F(SyncPrefsMigrationTest,
       SyncToSignin_TurnsAutofillAndPaymentsOffForCustomPassphraseUser) {
  base::test::ScopedFeatureList enable_sync_to_signin(
      kReplaceSyncPromosWithSignInPromos);

  SyncPrefs prefs(&pref_service_);

  // Autofill and payments are enabled (by default).
  ASSERT_TRUE(prefs.GetSelectedTypesForAccount(gaia_id_hash_)
                  .Has(UserSelectableType::kAutofill));
  ASSERT_TRUE(prefs.GetSelectedTypesForAccount(gaia_id_hash_)
                  .Has(UserSelectableType::kPayments));

  // Run the first phase of the migration.
  prefs.MaybeMigratePrefsForSyncToSigninPart1(
      SyncPrefs::SyncAccountState::kSignedInNotSyncing, gaia_id_hash_);

  // Autofill and payments should still be unaffected for now, since the
  // passphrase state wasn't known yet.
  ASSERT_TRUE(prefs.GetSelectedTypesForAccount(gaia_id_hash_)
                  .Has(UserSelectableType::kAutofill));
  ASSERT_TRUE(prefs.GetSelectedTypesForAccount(gaia_id_hash_)
                  .Has(UserSelectableType::kPayments));

  // Now run the second phase, once the passphrase state is known (and it's
  // a custom passphrase).
  prefs.MaybeMigratePrefsForSyncToSigninPart2(
      gaia_id_hash_,
      /*is_using_explicit_passphrase=*/true);

  // Now Autofill and Payments should've been turned off in the account-scoped
  // settings.
  EXPECT_FALSE(prefs.GetSelectedTypesForAccount(gaia_id_hash_)
                   .Has(UserSelectableType::kAutofill));
  EXPECT_FALSE(prefs.GetSelectedTypesForAccount(gaia_id_hash_)
                   .Has(UserSelectableType::kPayments));
}

TEST_F(SyncPrefsMigrationTest,
       SyncToSignin_LeavesAutofillAloneForUserWithoutExplicitPassphrase) {
  base::test::ScopedFeatureList enable_sync_to_signin(
      kReplaceSyncPromosWithSignInPromos);

  SyncPrefs prefs(&pref_service_);

  // Autofill and payments are enabled (by default).
  ASSERT_TRUE(prefs.GetSelectedTypesForAccount(gaia_id_hash_)
                  .Has(UserSelectableType::kAutofill));
  ASSERT_TRUE(prefs.GetSelectedTypesForAccount(gaia_id_hash_)
                  .Has(UserSelectableType::kPayments));

  // Run the first phase of the migration.
  prefs.MaybeMigratePrefsForSyncToSigninPart1(
      SyncPrefs::SyncAccountState::kSignedInNotSyncing, gaia_id_hash_);

  // The types should still be unaffected for now, since the passphrase state
  // wasn't known yet.
  ASSERT_TRUE(prefs.GetSelectedTypesForAccount(gaia_id_hash_)
                  .Has(UserSelectableType::kAutofill));
  ASSERT_TRUE(prefs.GetSelectedTypesForAccount(gaia_id_hash_)
                  .Has(UserSelectableType::kPayments));

  // Now run the second phase, once the passphrase state is known (and it's a
  // regular keystore passphrase, i.e. no custom passphrase).
  prefs.MaybeMigratePrefsForSyncToSigninPart2(
      gaia_id_hash_,
      /*is_using_explicit_passphrase=*/false);

  // Since this is not a custom passphrase user, the types should still be
  // unaffected.
  EXPECT_TRUE(prefs.GetSelectedTypesForAccount(gaia_id_hash_)
                  .Has(UserSelectableType::kAutofill));
  EXPECT_TRUE(prefs.GetSelectedTypesForAccount(gaia_id_hash_)
                  .Has(UserSelectableType::kPayments));
}

TEST_F(SyncPrefsMigrationTest, SyncToSignin_Part2RunsOnSecondAttempt) {
  base::test::ScopedFeatureList enable_sync_to_signin(
      kReplaceSyncPromosWithSignInPromos);

  {
    SyncPrefs prefs(&pref_service_);

    // Autofill and payments are enabled (by default).
    ASSERT_TRUE(prefs.GetSelectedTypesForAccount(gaia_id_hash_)
                    .Has(UserSelectableType::kAutofill));
    ASSERT_TRUE(prefs.GetSelectedTypesForAccount(gaia_id_hash_)
                    .Has(UserSelectableType::kPayments));

    // Run the first phase of the migration.
    prefs.MaybeMigratePrefsForSyncToSigninPart1(
        SyncPrefs::SyncAccountState::kSignedInNotSyncing, gaia_id_hash_);

    // The account-scoped settings should still be unaffected for now, since the
    // passphrase state wasn't known yet.
    ASSERT_TRUE(prefs.GetSelectedTypesForAccount(gaia_id_hash_)
                    .Has(UserSelectableType::kAutofill));
    ASSERT_TRUE(prefs.GetSelectedTypesForAccount(gaia_id_hash_)
                    .Has(UserSelectableType::kPayments));
  }

  // Before the second phase runs, Chrome gets restarted.
  {
    SyncPrefs prefs(&pref_service_);

    // The first phase runs again. This should effectively do nothing.
    prefs.MaybeMigratePrefsForSyncToSigninPart1(
        SyncPrefs::SyncAccountState::kSignedInNotSyncing, gaia_id_hash_);

    ASSERT_TRUE(prefs.GetSelectedTypesForAccount(gaia_id_hash_)
                    .Has(UserSelectableType::kAutofill));
    ASSERT_TRUE(prefs.GetSelectedTypesForAccount(gaia_id_hash_)
                    .Has(UserSelectableType::kPayments));

    // Now run the second phase.
    prefs.MaybeMigratePrefsForSyncToSigninPart2(
        gaia_id_hash_,
        /*is_using_explicit_passphrase=*/true);

    // Now the types should've been turned off in the account-scoped settings.
    EXPECT_FALSE(prefs.GetSelectedTypesForAccount(gaia_id_hash_)
                     .Has(UserSelectableType::kAutofill));
    EXPECT_FALSE(prefs.GetSelectedTypesForAccount(gaia_id_hash_)
                     .Has(UserSelectableType::kPayments));
  }
}

}  // namespace

}  // namespace syncer
