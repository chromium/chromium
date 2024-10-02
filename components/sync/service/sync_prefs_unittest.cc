// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/service/sync_prefs.h"

#include <memory>
#include <vector>

#include "base/base64.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "build/chromeos_buildflags.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_value_map.h"
#include "components/prefs/testing_pref_service.h"
#include "components/saved_tab_groups/public/pref_names.h"
#include "components/signin/public/base/gaia_id_hash.h"
#include "components/signin/public/base/signin_pref_names.h"
#include "components/signin/public/base/signin_switches.h"
#include "components/sync/base/features.h"
#include "components/sync/base/pref_names.h"
#include "components/sync/base/user_selectable_type.h"
#include "components/sync/protocol/nigori_specifics.pb.h"
#include "components/sync/service/glue/sync_transport_data_prefs.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace syncer {

namespace {

using ::testing::_;
using ::testing::AtMost;
using ::testing::InSequence;
using ::testing::StrictMock;

// Copy of the same constant in sync_prefs.cc, for testing purposes.
constexpr char kObsoleteAutofillWalletImportEnabled[] =
    "autofill.wallet_import_enabled";

#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)
constexpr char kGaiaId[] = "gaia-id";
#endif  // !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)

class SyncPrefsTest : public testing::Test {
 protected:
  SyncPrefsTest() {
    SyncPrefs::RegisterProfilePrefs(pref_service_.registry());
    // TODO(crbug.com/368409110): These prefs are required due to a workaround
    // in KeepAccountSettingsPrefsOnlyForUsers(); see TODOs there.
    SyncTransportDataPrefs::RegisterProfilePrefs(pref_service_.registry());
    pref_service_.registry()->RegisterDictionaryPref(
        tab_groups::prefs::kLocallyClosedRemoteTabGroupIds,
        base::Value::Dict());

    // Pref is registered in signin internal `PrimaryAccountManager`.
    pref_service_.registry()->RegisterBooleanPref(
        ::prefs::kExplicitBrowserSignin, false);
    sync_prefs_ = std::make_unique<SyncPrefs>(&pref_service_);
    gaia_id_hash_ = signin::GaiaIdHash::FromGaiaId("account_gaia");
  }

  base::test::SingleThreadTaskEnvironment task_environment_;
  TestingPrefServiceSimple pref_service_;
  std::unique_ptr<SyncPrefs> sync_prefs_;
  signin::GaiaIdHash gaia_id_hash_;
};

TEST_F(SyncPrefsTest, EncryptionBootstrapTokenPerAccountSignedOut) {
  auto gaia_id_hash_empty = signin::GaiaIdHash::FromGaiaId("");
  EXPECT_TRUE(
      sync_prefs_->GetEncryptionBootstrapTokenForAccount(gaia_id_hash_empty)
          .empty());
}

TEST_F(SyncPrefsTest, EncryptionBootstrapTokenPerAccount) {
  ASSERT_TRUE(sync_prefs_->GetEncryptionBootstrapTokenForAccount(gaia_id_hash_)
                  .empty());
  sync_prefs_->SetEncryptionBootstrapTokenForAccount("token", gaia_id_hash_);
  EXPECT_EQ("token",
            sync_prefs_->GetEncryptionBootstrapTokenForAccount(gaia_id_hash_));
  auto gaia_id_hash_2 = signin::GaiaIdHash::FromGaiaId("account_gaia_2");
  EXPECT_TRUE(sync_prefs_->GetEncryptionBootstrapTokenForAccount(gaia_id_hash_2)
                  .empty());
  sync_prefs_->SetEncryptionBootstrapTokenForAccount("token2", gaia_id_hash_2);
  EXPECT_EQ("token",
            sync_prefs_->GetEncryptionBootstrapTokenForAccount(gaia_id_hash_));
  EXPECT_EQ("token2",
            sync_prefs_->GetEncryptionBootstrapTokenForAccount(gaia_id_hash_2));
}

TEST_F(SyncPrefsTest, ClearEncryptionBootstrapTokenPerAccount) {
  ASSERT_TRUE(sync_prefs_->GetEncryptionBootstrapTokenForAccount(gaia_id_hash_)
                  .empty());
  sync_prefs_->SetEncryptionBootstrapTokenForAccount("token", gaia_id_hash_);
  EXPECT_EQ("token",
            sync_prefs_->GetEncryptionBootstrapTokenForAccount(gaia_id_hash_));
  auto gaia_id_hash_2 = signin::GaiaIdHash::FromGaiaId("account_gaia_2");
  EXPECT_TRUE(sync_prefs_->GetEncryptionBootstrapTokenForAccount(gaia_id_hash_2)
                  .empty());
  sync_prefs_->SetEncryptionBootstrapTokenForAccount("token2", gaia_id_hash_2);
  EXPECT_EQ("token",
            sync_prefs_->GetEncryptionBootstrapTokenForAccount(gaia_id_hash_));
  EXPECT_EQ("token2",
            sync_prefs_->GetEncryptionBootstrapTokenForAccount(gaia_id_hash_2));
  // Remove account 2 from device by setting the available_gaia_ids to have the
  // gaia id of account 1 only.
  sync_prefs_->KeepAccountSettingsPrefsOnlyForUsers(
      /*available_gaia_ids=*/{gaia_id_hash_});
  EXPECT_EQ("token",
            sync_prefs_->GetEncryptionBootstrapTokenForAccount(gaia_id_hash_));
  EXPECT_TRUE(sync_prefs_->GetEncryptionBootstrapTokenForAccount(gaia_id_hash_2)
                  .empty());
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

TEST_F(SyncPrefsTest, CachedTrustedVaultAutoUpgradeExperimentGroup) {
  const int kTestCohort = 123;
  const sync_pb::TrustedVaultAutoUpgradeExperimentGroup::Type kTestType =
      sync_pb::TrustedVaultAutoUpgradeExperimentGroup::VALIDATION;
  const int kTestTypeIndex = 5;

  EXPECT_FALSE(sync_prefs_->GetCachedTrustedVaultAutoUpgradeExperimentGroup()
                   .has_value());

  {
    sync_pb::TrustedVaultAutoUpgradeExperimentGroup proto;
    proto.set_cohort(kTestCohort);
    proto.set_type(kTestType);
    proto.set_type_index(kTestTypeIndex);
    sync_prefs_->SetCachedTrustedVaultAutoUpgradeExperimentGroup(proto);
  }

  EXPECT_TRUE(sync_prefs_->GetCachedTrustedVaultAutoUpgradeExperimentGroup()
                  .has_value());

  const sync_pb::TrustedVaultAutoUpgradeExperimentGroup group_from_prefs =
      sync_prefs_->GetCachedTrustedVaultAutoUpgradeExperimentGroup().value_or(
          sync_pb::TrustedVaultAutoUpgradeExperimentGroup());

  EXPECT_EQ(kTestCohort, group_from_prefs.cohort());
  EXPECT_EQ(kTestType, group_from_prefs.type());
  EXPECT_EQ(kTestTypeIndex, group_from_prefs.type_index());

  sync_prefs_->ClearCachedTrustedVaultAutoUpgradeExperimentGroup();
  EXPECT_FALSE(sync_prefs_->GetCachedTrustedVaultAutoUpgradeExperimentGroup()
                   .has_value());
}

TEST_F(SyncPrefsTest, CachedTrustedVaultAutoUpgradeExperimentGroupCorrupt) {
  // Populate with a corrupt, non-base64 value.
  pref_service_.SetString(
      prefs::internal::kSyncCachedTrustedVaultAutoUpgradeExperimentGroup,
      "corrupt");
  EXPECT_TRUE(sync_prefs_->GetCachedTrustedVaultAutoUpgradeExperimentGroup()
                  .has_value());
  EXPECT_EQ(0, sync_prefs_->GetCachedTrustedVaultAutoUpgradeExperimentGroup()
                   .value_or(sync_pb::TrustedVaultAutoUpgradeExperimentGroup())
                   .cohort());
  EXPECT_EQ(sync_pb::TrustedVaultAutoUpgradeExperimentGroup::TYPE_UNSPECIFIED,
            sync_prefs_->GetCachedTrustedVaultAutoUpgradeExperimentGroup()
                .value_or(sync_pb::TrustedVaultAutoUpgradeExperimentGroup())
                .type());
  EXPECT_EQ(0, sync_prefs_->GetCachedTrustedVaultAutoUpgradeExperimentGroup()
                   .value_or(sync_pb::TrustedVaultAutoUpgradeExperimentGroup())
                   .type_index());

  // Populate with a corrupt, unparsable value after base64-decoding.
  pref_service_.SetString(
      prefs::internal::kSyncCachedTrustedVaultAutoUpgradeExperimentGroup,
      base::Base64Encode("corrupt"));
  EXPECT_TRUE(sync_prefs_->GetCachedTrustedVaultAutoUpgradeExperimentGroup()
                  .has_value());
  EXPECT_EQ(0, sync_prefs_->GetCachedTrustedVaultAutoUpgradeExperimentGroup()
                   .value_or(sync_pb::TrustedVaultAutoUpgradeExperimentGroup())
                   .cohort());
  EXPECT_EQ(sync_pb::TrustedVaultAutoUpgradeExperimentGroup::TYPE_UNSPECIFIED,
            sync_prefs_->GetCachedTrustedVaultAutoUpgradeExperimentGroup()
                .value_or(sync_pb::TrustedVaultAutoUpgradeExperimentGroup())
                .type());
  EXPECT_EQ(0, sync_prefs_->GetCachedTrustedVaultAutoUpgradeExperimentGroup()
                   .value_or(sync_pb::TrustedVaultAutoUpgradeExperimentGroup())
                   .type_index());
}

class MockSyncPrefObserver : public SyncPrefObserver {
 public:
  MOCK_METHOD(void, OnSyncManagedPrefChange, (bool), (override));
#if !BUILDFLAG(IS_CHROMEOS_ASH)
  MOCK_METHOD(void, OnFirstSetupCompletePrefChange, (bool), (override));
#endif  // !BUILDFLAG(IS_CHROMEOS_ASH)
  MOCK_METHOD(void, OnSelectedTypesPrefChange, (), (override));
};

TEST_F(SyncPrefsTest, ObservedPrefs) {
  StrictMock<MockSyncPrefObserver> mock_sync_pref_observer;
  InSequence in_sequence;
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
  InSequence in_sequence;

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
  EXPECT_CALL(mock_sync_pref_observer, OnSelectedTypesPrefChange);

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
  sync_prefs_->SetSelectedTypesForSyncingUser(
      /*keep_everything_synced=*/false,
      /*registered_types=*/UserSelectableTypeSet::All(),
      /*selected_types=*/UserSelectableTypeSet::All());
  EXPECT_FALSE(sync_prefs_->HasKeepEverythingSynced());
  sync_prefs_->SetSelectedTypesForSyncingUser(
      /*keep_everything_synced=*/true,
      /*registered_types=*/UserSelectableTypeSet::All(),
      /*selected_types=*/UserSelectableTypeSet());
  EXPECT_TRUE(sync_prefs_->HasKeepEverythingSynced());
}

TEST_F(SyncPrefsTest, SelectedTypesKeepEverythingSynced) {
  ASSERT_TRUE(sync_prefs_->HasKeepEverythingSynced());

  EXPECT_EQ(UserSelectableTypeSet::All(),
            sync_prefs_->GetSelectedTypesForSyncingUser());
  for (UserSelectableType type : UserSelectableTypeSet::All()) {
    StrictMock<MockSyncPrefObserver> mock_sync_pref_observer;
    sync_prefs_->AddObserver(&mock_sync_pref_observer);

    // SetSelectedTypesForSyncingUser() should result in at most one observer
    // notification: Never more than one, and in this case, since nothing
    // actually changes, zero calls would also be okay.
    EXPECT_CALL(mock_sync_pref_observer, OnSelectedTypesPrefChange)
        .Times(AtMost(1));

    sync_prefs_->SetSelectedTypesForSyncingUser(
        /*keep_everything_synced=*/true,
        /*registered_types=*/UserSelectableTypeSet::All(),
        /*selected_types=*/{type});
    EXPECT_EQ(UserSelectableTypeSet::All(),
              sync_prefs_->GetSelectedTypesForSyncingUser());

    sync_prefs_->RemoveObserver(&mock_sync_pref_observer);
  }
}

TEST_F(SyncPrefsTest, SelectedTypesKeepEverythingSyncedButPolicyRestricted) {
  ASSERT_TRUE(sync_prefs_->HasKeepEverythingSynced());

  StrictMock<MockSyncPrefObserver> mock_sync_pref_observer;
  sync_prefs_->AddObserver(&mock_sync_pref_observer);

  // Setting a managed pref value should trigger an
  // OnSelectedTypesPrefChange() notification.
  EXPECT_CALL(mock_sync_pref_observer, OnSelectedTypesPrefChange);
  pref_service_.SetManagedPref(prefs::internal::kSyncPreferences,
                               base::Value(false));

  sync_prefs_->RemoveObserver(&mock_sync_pref_observer);

  UserSelectableTypeSet expected_type_set = UserSelectableTypeSet::All();
  expected_type_set.Remove(UserSelectableType::kPreferences);
  EXPECT_EQ(expected_type_set, sync_prefs_->GetSelectedTypesForSyncingUser());
}

TEST_F(SyncPrefsTest, SelectedTypesNotKeepEverythingSynced) {
  sync_prefs_->SetSelectedTypesForSyncingUser(
      /*keep_everything_synced=*/false,
      /*registered_types=*/UserSelectableTypeSet::All(),
      /*selected_types=*/UserSelectableTypeSet());

  ASSERT_NE(UserSelectableTypeSet::All(),
            sync_prefs_->GetSelectedTypesForSyncingUser());
  for (UserSelectableType type : UserSelectableTypeSet::All()) {
    StrictMock<MockSyncPrefObserver> mock_sync_pref_observer;
    sync_prefs_->AddObserver(&mock_sync_pref_observer);

    // SetSelectedTypesForSyncingUser() should result in exactly one call to
    // OnSelectedTypesPrefChange(), even when multiple data types change
    // state (here, usually one gets enabled and one gets disabled).
    EXPECT_CALL(mock_sync_pref_observer, OnSelectedTypesPrefChange);

    sync_prefs_->SetSelectedTypesForSyncingUser(
        /*keep_everything_synced=*/false,
        /*registered_types=*/UserSelectableTypeSet::All(),
        /*selected_types=*/{type});
    EXPECT_EQ(UserSelectableTypeSet({type}),
              sync_prefs_->GetSelectedTypesForSyncingUser());

    sync_prefs_->RemoveObserver(&mock_sync_pref_observer);
  }
}

TEST_F(SyncPrefsTest, SelectedTypesNotKeepEverythingSyncedAndPolicyRestricted) {
  pref_service_.SetManagedPref(prefs::internal::kSyncPreferences,
                               base::Value(false));
  sync_prefs_->SetSelectedTypesForSyncingUser(
      /*keep_everything_synced=*/false,
      /*registered_types=*/UserSelectableTypeSet::All(),
      /*selected_types=*/UserSelectableTypeSet());

  ASSERT_FALSE(sync_prefs_->GetSelectedTypesForSyncingUser().Has(
      UserSelectableType::kPreferences));
  for (UserSelectableType type : UserSelectableTypeSet::All()) {
    sync_prefs_->SetSelectedTypesForSyncingUser(
        /*keep_everything_synced=*/false,
        /*registered_types=*/UserSelectableTypeSet::All(),
        /*selected_types=*/{type});
    UserSelectableTypeSet expected_type_set = {type};
    expected_type_set.Remove(UserSelectableType::kPreferences);
    EXPECT_EQ(expected_type_set, sync_prefs_->GetSelectedTypesForSyncingUser());
  }
}

TEST_F(SyncPrefsTest, SetTypeDisabledByPolicy) {
  // By default, data types are enabled, and not policy-controlled.
  ASSERT_TRUE(sync_prefs_->GetSelectedTypesForSyncingUser().Has(
      UserSelectableType::kBookmarks));
  ASSERT_FALSE(
      sync_prefs_->IsTypeManagedByPolicy(UserSelectableType::kBookmarks));
  ASSERT_TRUE(sync_prefs_->GetSelectedTypesForSyncingUser().Has(
      UserSelectableType::kAutofill));
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
  EXPECT_FALSE(sync_prefs_->GetSelectedTypesForSyncingUser().Has(
      UserSelectableType::kBookmarks));
  EXPECT_TRUE(
      sync_prefs_->IsTypeManagedByPolicy(UserSelectableType::kBookmarks));
  EXPECT_FALSE(
      sync_prefs_->IsTypeManagedByCustodian(UserSelectableType::kBookmarks));
  // Other types should be unaffected.
  EXPECT_TRUE(sync_prefs_->GetSelectedTypesForSyncingUser().Has(
      UserSelectableType::kAutofill));
  EXPECT_FALSE(
      sync_prefs_->IsTypeManagedByPolicy(UserSelectableType::kAutofill));
}

TEST_F(SyncPrefsTest, SetTypeDisabledByCustodian) {
  // By default, data types are enabled, and not custodian-controlled.
  ASSERT_TRUE(sync_prefs_->GetSelectedTypesForSyncingUser().Has(
      UserSelectableType::kBookmarks));
  ASSERT_FALSE(
      sync_prefs_->IsTypeManagedByCustodian(UserSelectableType::kBookmarks));
  ASSERT_TRUE(sync_prefs_->GetSelectedTypesForSyncingUser().Has(
      UserSelectableType::kAutofill));
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
  EXPECT_FALSE(sync_prefs_->GetSelectedTypesForSyncingUser().Has(
      UserSelectableType::kBookmarks));
  EXPECT_TRUE(
      sync_prefs_->IsTypeManagedByCustodian(UserSelectableType::kBookmarks));
  EXPECT_FALSE(
      sync_prefs_->IsTypeManagedByPolicy(UserSelectableType::kBookmarks));
  // Other types should be unaffected.
  EXPECT_TRUE(sync_prefs_->GetSelectedTypesForSyncingUser().Has(
      UserSelectableType::kAutofill));
  EXPECT_FALSE(
      sync_prefs_->IsTypeManagedByCustodian(UserSelectableType::kAutofill));
}

TEST_F(SyncPrefsTest,
       DefaultSelectedTypesForAccountInTransportMode_SyncToSigninDisabled) {
  base::test::ScopedFeatureList features;
  features.InitWithFeatures(
      /*enabled_features=*/{kSyncEnableBookmarksInTransportMode,
#if !BUILDFLAG(IS_IOS)
                            kReadingListEnableSyncTransportModeUponSignIn,
#endif  // !BUILDFLAG(IS_IOS)
                            kEnablePasswordsAccountStorageForNonSyncingUsers,
                            kSyncEnableContactInfoDataTypeInTransportMode,
                            kEnablePreferencesAccountStorage,
                            kSyncEnableExtensionsInTransportMode},
      /*disabled_features=*/{kReplaceSyncPromosWithSignInPromos});

  // Based on the feature flags set above, Passwords, Autofill and Payments are
  // supported and enabled by default. Bookmarks, ReadingList, and Extensions
  // are supported, but not enabled by default. Preferences, History, and Tabs
  // are not supported without kReplaceSyncPromosWithSignInPromos. Transport
  // mode is required for new sync types moving forward. Compare is one of those
  // and is enabled when kReplaceSyncPromosWithSignInPromos is enabled.
  UserSelectableTypeSet expected_types{UserSelectableType::kPasswords,
                                       UserSelectableType::kAutofill,
                                       UserSelectableType::kPayments};

#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)
  // On Desktop, kPasswords and kAutofill are disabled by default.
  expected_types.Remove(UserSelectableType::kPasswords);
  expected_types.Remove(UserSelectableType::kAutofill);
#endif

  EXPECT_EQ(sync_prefs_->GetSelectedTypesForAccount(gaia_id_hash_),
            expected_types);
}

TEST_F(SyncPrefsTest,
       DefaultSelectedTypesForAccountInTransportMode_SyncToSigninEnabled) {
  base::test::ScopedFeatureList features;
  features.InitWithFeatures(
      /*enabled_features=*/{kSyncEnableBookmarksInTransportMode,
                            kReplaceSyncPromosWithSignInPromos,
#if !BUILDFLAG(IS_IOS)
                            kReadingListEnableSyncTransportModeUponSignIn,
#endif  // !BUILDFLAG(IS_IOS)
                            kEnablePasswordsAccountStorageForNonSyncingUsers,
                            kSyncEnableContactInfoDataTypeInTransportMode,
                            kEnablePreferencesAccountStorage,
                            kSyncEnableExtensionsInTransportMode},
      /*disabled_features=*/{});

  // Based on the feature flags set above, Bookmarks, ReadingList, Passwords,
  // Autofill, Payments and Preferences are supported and enabled by default.
  // Extensions is supported, but not enabled by default. (History and Tabs are
  // also supported, but require a separate opt-in.) Transport mode is required
  // for new sync types moving forward. Compare is one of those and is enabled
  // when kReplaceSyncPromosWithSignInPromos is enabled.
  UserSelectableTypeSet expected_types{
      UserSelectableType::kBookmarks,   UserSelectableType::kProductComparison,
      UserSelectableType::kReadingList, UserSelectableType::kPasswords,
      UserSelectableType::kAutofill,    UserSelectableType::kPayments,
      UserSelectableType::kPreferences};

#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)
  // On Desktop, kPasswords and kAutofill are disabled by default.
  expected_types.Remove(UserSelectableType::kPasswords);
  expected_types.Remove(UserSelectableType::kAutofill);
#endif

  EXPECT_EQ(sync_prefs_->GetSelectedTypesForAccount(gaia_id_hash_),
            expected_types);
}

#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)

class SyncPrefsExplicitBrowserSigninTest : public SyncPrefsTest {
 public:
  SyncPrefsExplicitBrowserSigninTest() {
    scoped_feature_list_.InitWithFeatures(
        /*enabled_features=*/{syncer::
                                  kSyncEnableContactInfoDataTypeInTransportMode,
                              kSyncEnableExtensionsInTransportMode,
                              switches::kExplicitBrowserSigninUIOnDesktop},
        /*disabled_features=*/{});
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

TEST_F(SyncPrefsExplicitBrowserSigninTest, DefaultWithExplicitBrowserSignin) {
  // If no explicit browser sign in occurred, then the type is still disabled
  // by default.
  ASSERT_FALSE(pref_service_.GetBoolean(::prefs::kExplicitBrowserSignin));
  EXPECT_FALSE(sync_prefs_->GetSelectedTypesForAccount(gaia_id_hash_)
                   .Has(UserSelectableType::kAutofill));
  EXPECT_FALSE(sync_prefs_->GetSelectedTypesForAccount(gaia_id_hash_)
                   .Has(UserSelectableType::kPasswords));
  EXPECT_FALSE(sync_prefs_->GetSelectedTypesForAccount(gaia_id_hash_)
                   .Has(UserSelectableType::kExtensions));

  // Set an explicit browser signin.
  pref_service_.SetBoolean(::prefs::kExplicitBrowserSignin, true);

  // With an explicit sign in, apps, extensions, themes, passwords and autofill
  // are enabled by default.
  EXPECT_TRUE(sync_prefs_->GetSelectedTypesForAccount(gaia_id_hash_)
                  .Has(UserSelectableType::kAutofill));
  EXPECT_TRUE(sync_prefs_->GetSelectedTypesForAccount(gaia_id_hash_)
                  .Has(UserSelectableType::kPasswords));
  EXPECT_TRUE(sync_prefs_->GetSelectedTypesForAccount(gaia_id_hash_)
                  .Has(UserSelectableType::kExtensions));
}

#endif

TEST_F(SyncPrefsTest, SetSelectedTypesForAccountInTransportMode) {
  const UserSelectableTypeSet default_selected_types =
      sync_prefs_->GetSelectedTypesForAccount(gaia_id_hash_);
  ASSERT_TRUE(default_selected_types.Has(UserSelectableType::kPayments));

  StrictMock<MockSyncPrefObserver> mock_sync_pref_observer;
  sync_prefs_->AddObserver(&mock_sync_pref_observer);

  // Change one of the default values, for example kPayments. This should
  // result in an observer notification.
  EXPECT_CALL(mock_sync_pref_observer, OnSelectedTypesPrefChange);
  sync_prefs_->SetSelectedTypeForAccount(UserSelectableType::kPayments, false,
                                         gaia_id_hash_);

  sync_prefs_->RemoveObserver(&mock_sync_pref_observer);

  // kPayments should be disabled, other default values should be unaffected.
  EXPECT_EQ(
      sync_prefs_->GetSelectedTypesForAccount(gaia_id_hash_),
      Difference(default_selected_types, {UserSelectableType::kPayments}));
  // Other accounts should be unnafected.
  EXPECT_EQ(sync_prefs_->GetSelectedTypesForAccount(
                signin::GaiaIdHash::FromGaiaId("account_gaia_2")),
            default_selected_types);
}

TEST_F(SyncPrefsTest,
       SetSelectedTypesForAccountInTransportModeWithPolicyRestrictedType) {
  base::test::ScopedFeatureList features(
      kEnablePasswordsAccountStorageForNonSyncingUsers);

  StrictMock<MockSyncPrefObserver> mock_sync_pref_observer;
  sync_prefs_->AddObserver(&mock_sync_pref_observer);

  // Passwords gets disabled by policy. This should result in an observer
  // notification.
  EXPECT_CALL(mock_sync_pref_observer, OnSelectedTypesPrefChange);
  pref_service_.SetManagedPref(prefs::internal::kSyncPasswords,
                               base::Value(false));

  sync_prefs_->RemoveObserver(&mock_sync_pref_observer);

  // kPasswords should be disabled.
  UserSelectableTypeSet selected_types =
      sync_prefs_->GetSelectedTypesForAccount(gaia_id_hash_);
  ASSERT_FALSE(selected_types.empty());
  EXPECT_FALSE(selected_types.Has(UserSelectableType::kPasswords));

  // User tries to enable kPasswords.
  sync_prefs_->SetSelectedTypeForAccount(UserSelectableType::kPasswords, true,
                                         gaia_id_hash_);

  // kPasswords should still be disabled.
  EXPECT_FALSE(sync_prefs_->GetSelectedTypesForAccount(gaia_id_hash_)
                   .Has(UserSelectableType::kPasswords));
}

TEST_F(SyncPrefsTest, KeepAccountSettingsPrefsOnlyForUsers) {
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
      sync_prefs_->GetSelectedTypesForSyncingUser();

  sync_prefs_->SetSelectedOsTypes(
      /*sync_all_os_types=*/false,
      /*registered_types=*/UserSelectableOsTypeSet::All(),
      /*selected_types=*/UserSelectableOsTypeSet());
  EXPECT_EQ(UserSelectableOsTypeSet(), sync_prefs_->GetSelectedOsTypes());
  // Browser types are not changed.
  EXPECT_EQ(browser_types, sync_prefs_->GetSelectedTypesForSyncingUser());

  for (UserSelectableOsType type : UserSelectableOsTypeSet::All()) {
    sync_prefs_->SetSelectedOsTypes(
        /*sync_all_os_types=*/false,
        /*registered_types=*/UserSelectableOsTypeSet::All(),
        /*selected_types=*/{type});
    EXPECT_EQ(UserSelectableOsTypeSet({type}),
              sync_prefs_->GetSelectedOsTypes());
    // Browser types are not changed.
    EXPECT_EQ(browser_types, sync_prefs_->GetSelectedTypesForSyncingUser());
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

  EXPECT_CALL(mock_sync_pref_observer, OnSelectedTypesPrefChange);
  sync_prefs_->SetAppsSyncEnabledByOs(/*apps_sync_enabled=*/true);
  EXPECT_TRUE(sync_prefs_->IsAppsSyncEnabledByOs());

  testing::Mock::VerifyAndClearExpectations(&mock_sync_pref_observer);
  EXPECT_CALL(mock_sync_pref_observer, OnSelectedTypesPrefChange);
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

#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)
TEST_F(SyncPrefsTest, GetNumberOfAccountsWithPasswordsSelected) {
  EXPECT_EQ(sync_prefs_->GetNumberOfAccountsWithPasswordsSelected(), 0);

  sync_prefs_->SetSelectedTypeForAccount(UserSelectableType::kPasswords, true,
                                         gaia_id_hash_);

  EXPECT_EQ(sync_prefs_->GetNumberOfAccountsWithPasswordsSelected(), 1);

  const auto other_gaia_id_hash = signin::GaiaIdHash::FromGaiaId("other");
  sync_prefs_->SetSelectedTypeForAccount(UserSelectableType::kPasswords, true,
                                         other_gaia_id_hash);

  EXPECT_EQ(sync_prefs_->GetNumberOfAccountsWithPasswordsSelected(), 2);

  sync_prefs_->SetSelectedTypeForAccount(UserSelectableType::kPasswords, false,
                                         gaia_id_hash_);

  EXPECT_EQ(sync_prefs_->GetNumberOfAccountsWithPasswordsSelected(), 1);

  sync_prefs_->KeepAccountSettingsPrefsOnlyForUsers({});

  EXPECT_EQ(sync_prefs_->GetNumberOfAccountsWithPasswordsSelected(), 0);
}
#endif

TEST_F(SyncPrefsTest, PasswordSyncAllowed_DefaultValue) {
  // Passwords is in its default state. For syncing users, it's enabled. For
  // non-syncing users, it depends on the platform.
  ASSERT_TRUE(sync_prefs_->GetSelectedTypesForSyncingUser().Has(
      UserSelectableType::kPasswords));
  StrictMock<MockSyncPrefObserver> observer;
  sync_prefs_->AddObserver(&observer);
  EXPECT_CALL(observer, OnSelectedTypesPrefChange);

  sync_prefs_->SetPasswordSyncAllowed(false);

  EXPECT_FALSE(sync_prefs_->GetSelectedTypesForSyncingUser().Has(
      UserSelectableType::kPasswords));
  EXPECT_FALSE(sync_prefs_->GetSelectedTypesForAccount(gaia_id_hash_)
                   .Has(UserSelectableType::kPasswords));
  sync_prefs_->RemoveObserver(&observer);
}

TEST_F(SyncPrefsTest, PasswordSyncAllowed_ExplicitValue) {
  // Make passwords explicitly enabled (no default value).
  sync_prefs_->SetSelectedTypesForSyncingUser(
      /*keep_everything_synced=*/false,
      /*registered_types=*/UserSelectableTypeSet::All(),
      /*selected_types=*/{UserSelectableType::kPasswords});
  sync_prefs_->SetSelectedTypeForAccount(UserSelectableType::kPasswords, true,
                                         gaia_id_hash_);

  sync_prefs_->SetPasswordSyncAllowed(false);

  EXPECT_FALSE(sync_prefs_->GetSelectedTypesForSyncingUser().Has(
      UserSelectableType::kPasswords));
  EXPECT_FALSE(sync_prefs_->GetSelectedTypesForAccount(gaia_id_hash_)
                   .Has(UserSelectableType::kPasswords));
}

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
        /*enabled_features=*/{kSyncEnableBookmarksInTransportMode,
#if !BUILDFLAG(IS_IOS)
                              kReadingListEnableSyncTransportModeUponSignIn,
#endif  // !BUILDFLAG(IS_IOS)
#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)
                              switches::kExplicitBrowserSigninUIOnDesktop,
#endif
                              kEnablePasswordsAccountStorageForNonSyncingUsers,
                              kSyncEnableContactInfoDataTypeInTransportMode,
                              kEnablePreferencesAccountStorage},
        /*disabled_features=*/{});

    SyncPrefs::RegisterProfilePrefs(pref_service_.registry());
    gaia_id_hash_ = signin::GaiaIdHash::FromGaiaId("account_gaia");
#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)
    // Pref is registered in signin internal `PrimaryAccountManager`.
    pref_service_.registry()->RegisterBooleanPref(
        ::prefs::kExplicitBrowserSignin, false);
    pref_service_.SetBoolean(::prefs::kExplicitBrowserSignin, true);
    pref_service_.registry()->RegisterStringPref(
        ::prefs::kGoogleServicesLastSyncingGaiaId, std::string());
#endif
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

#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)
TEST_F(SyncPrefsMigrationTest,
       DoNotMigratePasswordsToPerAccountPrefIfLastGaiaIdMissing) {
  base::test::ScopedFeatureList feature_list(
      switches::kExplicitBrowserSigninUIOnDesktop);
  ASSERT_EQ(pref_service_.GetString(::prefs::kGoogleServicesLastSyncingGaiaId),
            std::string());
  pref_service_.SetBoolean(prefs::internal::kSyncKeepEverythingSynced, false);
  ASSERT_FALSE(pref_service_.GetBoolean(kGlobalPasswordsPref));
  ASSERT_TRUE(
      SyncPrefs(&pref_service_)
          .GetSelectedTypesForAccount(signin::GaiaIdHash::FromGaiaId(kGaiaId))
          .Has(UserSelectableType::kPasswords));

  SyncPrefs::MaybeMigrateAutofillToPerAccountPref(&pref_service_);

  EXPECT_TRUE(
      SyncPrefs(&pref_service_)
          .GetSelectedTypesForAccount(signin::GaiaIdHash::FromGaiaId(kGaiaId))
          .Has(UserSelectableType::kPasswords));
}

TEST_F(SyncPrefsMigrationTest,
       DoNotMigratePasswordsToPerAccountPrefIfSyncEverythingEnabled) {
  base::test::ScopedFeatureList feature_list(
      switches::kExplicitBrowserSigninUIOnDesktop);
  pref_service_.SetString(::prefs::kGoogleServicesLastSyncingGaiaId, kGaiaId);
  ASSERT_TRUE(
      pref_service_.GetBoolean(prefs::internal::kSyncKeepEverythingSynced));
  ASSERT_FALSE(pref_service_.GetBoolean(kGlobalPasswordsPref));
  ASSERT_TRUE(
      SyncPrefs(&pref_service_)
          .GetSelectedTypesForAccount(signin::GaiaIdHash::FromGaiaId(kGaiaId))
          .Has(UserSelectableType::kPasswords));

  SyncPrefs::MaybeMigrateAutofillToPerAccountPref(&pref_service_);

  EXPECT_TRUE(
      SyncPrefs(&pref_service_)
          .GetSelectedTypesForAccount(signin::GaiaIdHash::FromGaiaId(kGaiaId))
          .Has(UserSelectableType::kPasswords));
}

TEST_F(SyncPrefsMigrationTest,
       DoNotMigratePasswordsToPerAccountPrefIfPasswordsEnabled) {
  base::test::ScopedFeatureList feature_list(
      switches::kExplicitBrowserSigninUIOnDesktop);
  pref_service_.SetString(::prefs::kGoogleServicesLastSyncingGaiaId, kGaiaId);
  pref_service_.SetBoolean(prefs::internal::kSyncKeepEverythingSynced, false);
  pref_service_.SetBoolean(kGlobalPasswordsPref, true);
  ASSERT_TRUE(
      SyncPrefs(&pref_service_)
          .GetSelectedTypesForAccount(signin::GaiaIdHash::FromGaiaId(kGaiaId))
          .Has(UserSelectableType::kPasswords));

  SyncPrefs::MaybeMigrateAutofillToPerAccountPref(&pref_service_);

  EXPECT_TRUE(
      SyncPrefs(&pref_service_)
          .GetSelectedTypesForAccount(signin::GaiaIdHash::FromGaiaId(kGaiaId))
          .Has(UserSelectableType::kPasswords));
}

TEST_F(SyncPrefsMigrationTest,
       DoNotMigratePasswordsToPerAccountPrefIfFlagDisabled) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(
      switches::kExplicitBrowserSigninUIOnDesktop);
  pref_service_.SetString(::prefs::kGoogleServicesLastSyncingGaiaId, kGaiaId);
  pref_service_.SetBoolean(prefs::internal::kSyncKeepEverythingSynced, false);
  ASSERT_FALSE(pref_service_.GetBoolean(kGlobalPasswordsPref));
  ASSERT_TRUE(
      SyncPrefs(&pref_service_)
          .GetSelectedTypesForAccount(signin::GaiaIdHash::FromGaiaId(kGaiaId))
          .Has(UserSelectableType::kPasswords));

  SyncPrefs::MaybeMigrateAutofillToPerAccountPref(&pref_service_);

  EXPECT_TRUE(
      SyncPrefs(&pref_service_)
          .GetSelectedTypesForAccount(signin::GaiaIdHash::FromGaiaId(kGaiaId))
          .Has(UserSelectableType::kPasswords));
}

TEST_F(SyncPrefsMigrationTest, MigratePasswordsToPerAccountPrefRunsOnce) {
  base::test::ScopedFeatureList feature_list(
      switches::kExplicitBrowserSigninUIOnDesktop);
  pref_service_.SetString(::prefs::kGoogleServicesLastSyncingGaiaId, kGaiaId);
  pref_service_.SetBoolean(prefs::internal::kSyncKeepEverythingSynced, false);
  ASSERT_FALSE(pref_service_.GetBoolean(kGlobalPasswordsPref));
  ASSERT_TRUE(
      SyncPrefs(&pref_service_)
          .GetSelectedTypesForAccount(signin::GaiaIdHash::FromGaiaId(kGaiaId))
          .Has(UserSelectableType::kPasswords));

  SyncPrefs::MaybeMigrateAutofillToPerAccountPref(&pref_service_);

  EXPECT_FALSE(
      SyncPrefs(&pref_service_)
          .GetSelectedTypesForAccount(signin::GaiaIdHash::FromGaiaId(kGaiaId))
          .Has(UserSelectableType::kPasswords));

  // Manually re-enable and attempt to run the migration again.
  SyncPrefs(&pref_service_)
      .SetSelectedTypeForAccount(UserSelectableType::kPasswords, true,
                                 signin::GaiaIdHash::FromGaiaId(kGaiaId));
  SyncPrefs::MaybeMigrateAutofillToPerAccountPref(&pref_service_);

  // This time the migration didn't run, because it was one-off.
  EXPECT_TRUE(
      SyncPrefs(&pref_service_)
          .GetSelectedTypesForAccount(signin::GaiaIdHash::FromGaiaId(kGaiaId))
          .Has(UserSelectableType::kPasswords));
}

TEST_F(SyncPrefsMigrationTest, MigrateAddressesToPerAccountPref) {
  base::test::ScopedFeatureList feature_list(
      switches::kExplicitBrowserSigninUIOnDesktop);
  pref_service_.SetString(::prefs::kGoogleServicesLastSyncingGaiaId, kGaiaId);
  pref_service_.SetBoolean(prefs::internal::kSyncKeepEverythingSynced, false);
  ASSERT_FALSE(pref_service_.GetBoolean(kGlobalAutofillPref));
  ASSERT_TRUE(
      SyncPrefs(&pref_service_)
          .GetSelectedTypesForAccount(signin::GaiaIdHash::FromGaiaId(kGaiaId))
          .Has(UserSelectableType::kAutofill));

  SyncPrefs::MaybeMigrateAutofillToPerAccountPref(&pref_service_);

  EXPECT_FALSE(
      SyncPrefs(&pref_service_)
          .GetSelectedTypesForAccount(signin::GaiaIdHash::FromGaiaId(kGaiaId))
          .Has(UserSelectableType::kAutofill));
}
#endif  // !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)

TEST_F(SyncPrefsMigrationTest, NoPassphraseMigrationForSignoutUsers) {
  SyncPrefs prefs(&pref_service_);
  // Passphrase is not set.
  ASSERT_TRUE(
      pref_service_.GetString(prefs::internal::kSyncEncryptionBootstrapToken)
          .empty());

  auto gaia_id_hash_empty = signin::GaiaIdHash::FromGaiaId("");
  prefs.MaybeMigrateCustomPassphrasePref(gaia_id_hash_empty);
  EXPECT_TRUE(
      pref_service_.GetString(prefs::internal::kSyncEncryptionBootstrapToken)
          .empty());
  EXPECT_TRUE(
      prefs.GetEncryptionBootstrapTokenForAccount(gaia_id_hash_empty).empty());
}

TEST_F(SyncPrefsMigrationTest, PassphraseMigrationDone) {
  SyncPrefs prefs(&pref_service_);
  pref_service_.SetString(prefs::internal::kSyncEncryptionBootstrapToken,
                          "token");
  prefs.MaybeMigrateCustomPassphrasePref(gaia_id_hash_);
  EXPECT_EQ(
      pref_service_.GetString(prefs::internal::kSyncEncryptionBootstrapToken),
      "token");
  EXPECT_EQ(prefs.GetEncryptionBootstrapTokenForAccount(gaia_id_hash_),
            "token");
  signin::GaiaIdHash gaia_id_hash_2 =
      signin::GaiaIdHash::FromGaiaId("account_gaia_2");
  EXPECT_TRUE(
      prefs.GetEncryptionBootstrapTokenForAccount(gaia_id_hash_2).empty());
}

TEST_F(SyncPrefsMigrationTest, PassphraseMigrationOnlyOnce) {
  SyncPrefs prefs(&pref_service_);
  pref_service_.SetString(prefs::internal::kSyncEncryptionBootstrapToken,
                          "token");
  prefs.MaybeMigrateCustomPassphrasePref(gaia_id_hash_);
  EXPECT_EQ(
      pref_service_.GetString(prefs::internal::kSyncEncryptionBootstrapToken),
      "token");
  EXPECT_EQ(prefs.GetEncryptionBootstrapTokenForAccount(gaia_id_hash_),
            "token");

  // Force old pref to change for testing purposes.
  pref_service_.SetString(prefs::internal::kSyncEncryptionBootstrapToken,
                          "token2");
  prefs.MaybeMigrateCustomPassphrasePref(gaia_id_hash_);
  // The migration should not run again.
  EXPECT_EQ(
      pref_service_.GetString(prefs::internal::kSyncEncryptionBootstrapToken),
      "token2");
  EXPECT_EQ(prefs.GetEncryptionBootstrapTokenForAccount(gaia_id_hash_),
            "token");
}

TEST_F(SyncPrefsMigrationTest, PassphraseMigrationOnlyOnceWithBrowserRestart) {
  {
    SyncPrefs prefs(&pref_service_);
    pref_service_.SetString(prefs::internal::kSyncEncryptionBootstrapToken,
                            "token");
    prefs.MaybeMigrateCustomPassphrasePref(gaia_id_hash_);
    EXPECT_EQ(
        pref_service_.GetString(prefs::internal::kSyncEncryptionBootstrapToken),
        "token");
    EXPECT_EQ(prefs.GetEncryptionBootstrapTokenForAccount(gaia_id_hash_),
              "token");
    // Force old pref to change for testing purposes.
    pref_service_.SetString(prefs::internal::kSyncEncryptionBootstrapToken,
                            "token2");
  }

  // The browser is restarted.
  {
    SyncPrefs prefs(&pref_service_);
    prefs.MaybeMigrateCustomPassphrasePref(gaia_id_hash_);
    // No migration should run.
    EXPECT_EQ(
        pref_service_.GetString(prefs::internal::kSyncEncryptionBootstrapToken),
        "token2");
    EXPECT_EQ(prefs.GetEncryptionBootstrapTokenForAccount(gaia_id_hash_),
              "token");
  }
}

TEST_F(SyncPrefsMigrationTest, NoMigrationForSignedOutUser) {
  base::test::ScopedFeatureList enable_sync_to_signin(
      kReplaceSyncPromosWithSignInPromos);

  EXPECT_FALSE(
      SyncPrefs(&pref_service_)
          .MaybeMigratePrefsForSyncToSigninPart1(
              SyncPrefs::SyncAccountState::kNotSignedIn, signin::GaiaIdHash()));
  // Part 2 isn't called because the engine isn't initialized.
}

TEST_F(SyncPrefsMigrationTest, NoMigrationForSyncingUser) {
  base::test::ScopedFeatureList enable_sync_to_signin(
      kReplaceSyncPromosWithSignInPromos);

  SyncPrefs prefs(&pref_service_);
  EXPECT_FALSE(prefs.MaybeMigratePrefsForSyncToSigninPart1(
      SyncPrefs::SyncAccountState::kSyncing, gaia_id_hash_));
  EXPECT_FALSE(prefs.MaybeMigratePrefsForSyncToSigninPart2(
      gaia_id_hash_,
      /*is_using_explicit_passphrase=*/true));
}

TEST_F(SyncPrefsMigrationTest, RunsOnlyOnce) {
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

TEST_F(SyncPrefsMigrationTest, RunsAgainAfterFeatureReenabled) {
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

TEST_F(SyncPrefsMigrationTest, GlobalPrefsAreUnchanged) {
  base::test::ScopedFeatureList enable_sync_to_signin(
      kReplaceSyncPromosWithSignInPromos);

  for (UserSelectableType type : UserSelectableTypeSet::All()) {
    ASSERT_EQ(
        GetBooleanUserPrefValue(SyncPrefs::GetPrefNameForTypeForTesting(type)),
        BooleanPrefState::PREF_UNSET);
  }

  SyncPrefs prefs(&pref_service_);

  ASSERT_TRUE(prefs.MaybeMigratePrefsForSyncToSigninPart1(
      SyncPrefs::SyncAccountState::kSignedInNotSyncing, gaia_id_hash_));
  ASSERT_TRUE(prefs.MaybeMigratePrefsForSyncToSigninPart2(
      gaia_id_hash_,
      /*is_using_explicit_passphrase=*/true));

  for (UserSelectableType type : UserSelectableTypeSet::All()) {
    EXPECT_EQ(
        GetBooleanUserPrefValue(SyncPrefs::GetPrefNameForTypeForTesting(type)),
        BooleanPrefState::PREF_UNSET);
  }
}

TEST_F(SyncPrefsMigrationTest, TurnsPreferencesOff) {
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

TEST_F(SyncPrefsMigrationTest, MigratesBookmarksOptedIn) {
  {
    // The SyncToSignin feature starts disabled.
    base::test::ScopedFeatureList disable_sync_to_signin;
    disable_sync_to_signin.InitAndDisableFeature(
        kReplaceSyncPromosWithSignInPromos);

    // The user enables Bookmarks and Reading List.
    SyncPrefs prefs(&pref_service_);
    prefs.SetSelectedTypeForAccount(UserSelectableType::kBookmarks, true,
                                    gaia_id_hash_);
    prefs.SetSelectedTypeForAccount(UserSelectableType::kReadingList, true,
                                    gaia_id_hash_);

    ASSERT_TRUE(prefs.GetSelectedTypesForAccount(gaia_id_hash_)
                    .Has(UserSelectableType::kBookmarks));
    ASSERT_TRUE(prefs.GetSelectedTypesForAccount(gaia_id_hash_)
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

TEST_F(SyncPrefsMigrationTest, MigratesBookmarksNotOptedIn) {
  {
    // The SyncToSignin feature starts disabled.
    base::test::ScopedFeatureList disable_sync_to_signin;
    disable_sync_to_signin.InitAndDisableFeature(
        kReplaceSyncPromosWithSignInPromos);

    SyncPrefs prefs(&pref_service_);

    // With the feature disabled, Bookmarks and ReadingList are disabled by
    // default.
    ASSERT_FALSE(prefs.GetSelectedTypesForAccount(gaia_id_hash_)
                     .Has(UserSelectableType::kBookmarks));
    ASSERT_FALSE(prefs.GetSelectedTypesForAccount(gaia_id_hash_)
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

TEST_F(SyncPrefsMigrationTest, TurnsAutofillOffForCustomPassphraseUser) {
  base::test::ScopedFeatureList enable_sync_to_signin(
      kReplaceSyncPromosWithSignInPromos);

  SyncPrefs prefs(&pref_service_);

  // Autofill is enabled (by default).
  ASSERT_TRUE(prefs.GetSelectedTypesForAccount(gaia_id_hash_)
                  .Has(UserSelectableType::kAutofill));

  // Run the first phase of the migration.
  prefs.MaybeMigratePrefsForSyncToSigninPart1(
      SyncPrefs::SyncAccountState::kSignedInNotSyncing, gaia_id_hash_);

  // Autofill should still be unaffected for now, since the passphrase state
  // wasn't known yet.
  ASSERT_TRUE(prefs.GetSelectedTypesForAccount(gaia_id_hash_)
                  .Has(UserSelectableType::kAutofill));

  // Now run the second phase, once the passphrase state is known (and it's
  // a custom passphrase).
  prefs.MaybeMigratePrefsForSyncToSigninPart2(
      gaia_id_hash_,
      /*is_using_explicit_passphrase=*/true);

  // Now Autofill should've been turned off in the account-scoped settings.
  EXPECT_FALSE(prefs.GetSelectedTypesForAccount(gaia_id_hash_)
                   .Has(UserSelectableType::kAutofill));
}

TEST_F(SyncPrefsMigrationTest,
       LeavesAutofillAloneForUserWithoutExplicitPassphrase) {
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

TEST_F(SyncPrefsMigrationTest, Part2RunsOnSecondAttempt) {
  base::test::ScopedFeatureList enable_sync_to_signin(
      kReplaceSyncPromosWithSignInPromos);

  {
    SyncPrefs prefs(&pref_service_);

    // Autofill is enabled (by default).
    ASSERT_TRUE(prefs.GetSelectedTypesForAccount(gaia_id_hash_)
                    .Has(UserSelectableType::kAutofill));

    // Run the first phase of the migration.
    prefs.MaybeMigratePrefsForSyncToSigninPart1(
        SyncPrefs::SyncAccountState::kSignedInNotSyncing, gaia_id_hash_);

    // The account-scoped settings should still be unaffected for now, since the
    // passphrase state wasn't known yet.
    ASSERT_TRUE(prefs.GetSelectedTypesForAccount(gaia_id_hash_)
                    .Has(UserSelectableType::kAutofill));
  }

  // Before the second phase runs, Chrome gets restarted.
  {
    SyncPrefs prefs(&pref_service_);

    // The first phase runs again. This should effectively do nothing.
    prefs.MaybeMigratePrefsForSyncToSigninPart1(
        SyncPrefs::SyncAccountState::kSignedInNotSyncing, gaia_id_hash_);

    ASSERT_TRUE(prefs.GetSelectedTypesForAccount(gaia_id_hash_)
                    .Has(UserSelectableType::kAutofill));

    // Now run the second phase.
    prefs.MaybeMigratePrefsForSyncToSigninPart2(
        gaia_id_hash_,
        /*is_using_explicit_passphrase=*/true);

    // Now the type should've been turned off in the account-scoped settings.
    EXPECT_FALSE(prefs.GetSelectedTypesForAccount(gaia_id_hash_)
                     .Has(UserSelectableType::kAutofill));
  }
}

TEST_F(SyncPrefsMigrationTest, GlobalToAccount_DefaultState) {
  base::test::ScopedFeatureList enable_sync_to_signin(
      kReplaceSyncPromosWithSignInPromos);

  // Everything is in the default state. Notably, "Sync Everything" is true.

  // Pre-migration (without any explicit per-account settings), most supported
  // types are considered selected by default - except for kHistory and kTabs,
  // and kPasswords on desktop.
  // Note that this is not exhaustive - depending on feature flags, additional
  // types may be supported and default-enabled.
  const UserSelectableTypeSet default_enabled_types{
      UserSelectableType::kAutofill, UserSelectableType::kBookmarks,
      UserSelectableType::kPayments, UserSelectableType::kPreferences,
      UserSelectableType::kReadingList};
  ASSERT_TRUE(SyncPrefs(&pref_service_)
                  .GetSelectedTypesForAccount(gaia_id_hash_)
                  .HasAll(default_enabled_types));
  ASSERT_FALSE(
      SyncPrefs(&pref_service_)
          .GetSelectedTypesForAccount(gaia_id_hash_)
          .HasAny({UserSelectableType::kHistory, UserSelectableType::kTabs}));

  SyncPrefs::MigrateGlobalDataTypePrefsToAccount(&pref_service_, gaia_id_hash_);

  // All supported types should be considered selected for this account now,
  // including kHistory and kTabs.
  SyncPrefs prefs(&pref_service_);
  UserSelectableTypeSet selected_types =
      prefs.GetSelectedTypesForAccount(gaia_id_hash_);
  EXPECT_TRUE(selected_types.HasAll(default_enabled_types));
  EXPECT_TRUE(selected_types.Has(UserSelectableType::kHistory));
  EXPECT_TRUE(selected_types.Has(UserSelectableType::kTabs));
  // Also kPasswords which (depending on the platform) may or may not be
  // considered enabled by default, should be selected now.
  EXPECT_TRUE(selected_types.Has(UserSelectableType::kPasswords));
}

TEST_F(SyncPrefsMigrationTest, GlobalToAccount_CustomState) {
  base::test::ScopedFeatureList enable_sync_to_signin(
      kReplaceSyncPromosWithSignInPromos);

  // The user has chosen specific data types to sync. In this example, Bookmarks
  // and Preferences are disabled.
  const UserSelectableTypeSet old_selected_types{
      UserSelectableType::kAutofill,    UserSelectableType::kHistory,
      UserSelectableType::kPasswords,   UserSelectableType::kPayments,
      UserSelectableType::kReadingList, UserSelectableType::kTabs};
  {
    SyncPrefs old_prefs(&pref_service_);
    old_prefs.SetSelectedTypesForSyncingUser(
        /*keep_everything_synced=*/false,
        /*registered_types=*/UserSelectableTypeSet::All(), old_selected_types);
  }

  // Pre-migration (without any explicit per-account settings), most supported
  // types are considered selected by default, including Bookmarks and
  // Preferences - but not History or Tabs (or, on desktop, Passwords).
  // Note that this is not exhaustive - depending on feature flags, additional
  // types may be supported and default-enabled.
  ASSERT_TRUE(SyncPrefs(&pref_service_)
                  .GetSelectedTypesForAccount(gaia_id_hash_)
                  .HasAll({UserSelectableType::kAutofill,
                           UserSelectableType::kBookmarks,
                           UserSelectableType::kPayments,
                           UserSelectableType::kPreferences,
                           UserSelectableType::kReadingList}));
  ASSERT_FALSE(
      SyncPrefs(&pref_service_)
          .GetSelectedTypesForAccount(gaia_id_hash_)
          .HasAny({UserSelectableType::kHistory, UserSelectableType::kTabs}));

  SyncPrefs::MigrateGlobalDataTypePrefsToAccount(&pref_service_, gaia_id_hash_);

  // After the migration, exactly the same types should be selected as before.
  SyncPrefs prefs(&pref_service_);
  EXPECT_EQ(prefs.GetSelectedTypesForAccount(gaia_id_hash_),
            old_selected_types);
}

TEST_F(SyncPrefsMigrationTest, GlobalToAccount_HistoryDisabled) {
  base::test::ScopedFeatureList enable_sync_to_signin(
      kReplaceSyncPromosWithSignInPromos);

  // All types except for kHistory are selected in the global prefs.
  {
    SyncPrefs old_prefs(&pref_service_);
    UserSelectableTypeSet selected_types = UserSelectableTypeSet::All();
    selected_types.Remove(UserSelectableType::kHistory);
    old_prefs.SetSelectedTypesForSyncingUser(
        /*keep_everything_synced=*/false,
        /*registered_types=*/UserSelectableTypeSet::All(), selected_types);
  }

  SyncPrefs::MigrateGlobalDataTypePrefsToAccount(&pref_service_, gaia_id_hash_);

  // After the migration, both kHistory and kTabs should be disabled, since
  // there is only a single toggle for both of them.
  SyncPrefs prefs(&pref_service_);
  UserSelectableTypeSet selected_types =
      prefs.GetSelectedTypesForAccount(gaia_id_hash_);
  EXPECT_FALSE(selected_types.Has(UserSelectableType::kHistory));
  EXPECT_FALSE(selected_types.Has(UserSelectableType::kTabs));
}

TEST_F(SyncPrefsMigrationTest, GlobalToAccount_TabsDisabled) {
  base::test::ScopedFeatureList enable_sync_to_signin(
      kReplaceSyncPromosWithSignInPromos);

  // All types except for kTabs are selected in the global prefs.
  {
    SyncPrefs old_prefs(&pref_service_);
    UserSelectableTypeSet selected_types = UserSelectableTypeSet::All();
    selected_types.Remove(UserSelectableType::kTabs);
    old_prefs.SetSelectedTypesForSyncingUser(
        /*keep_everything_synced=*/false,
        /*registered_types=*/UserSelectableTypeSet::All(), selected_types);
  }

  SyncPrefs::MigrateGlobalDataTypePrefsToAccount(&pref_service_, gaia_id_hash_);

  // After the migration, both kHistory and kTabs should be disabled, since
  // there is only a single toggle for both of them.
  SyncPrefs prefs(&pref_service_);
  UserSelectableTypeSet selected_types =
      prefs.GetSelectedTypesForAccount(gaia_id_hash_);
  EXPECT_FALSE(selected_types.Has(UserSelectableType::kHistory));
  EXPECT_FALSE(selected_types.Has(UserSelectableType::kTabs));
}

TEST_F(SyncPrefsMigrationTest, GlobalToAccount_CustomPassphrase) {
  base::test::ScopedFeatureList enable_sync_to_signin(
      kReplaceSyncPromosWithSignInPromos);

  // All types are enabled ("Sync Everything" is true), but the user has a
  // custom passphrase.
  {
    SyncPrefs old_prefs(&pref_service_);
    old_prefs.SetCachedPassphraseType(PassphraseType::kCustomPassphrase);
  }

  // Pre-migration (without any explicit per-account settings), most supported
  // types are considered selected by default - except for kHistory and kTabs,
  // and kPasswords on desktop.
  // Note that this is not exhaustive - depending on feature flags, additional
  // types may be supported and default-enabled.
  const UserSelectableTypeSet default_enabled_types{
      UserSelectableType::kAutofill, UserSelectableType::kBookmarks,
      UserSelectableType::kPayments, UserSelectableType::kPreferences,
      UserSelectableType::kReadingList};
  ASSERT_TRUE(SyncPrefs(&pref_service_)
                  .GetSelectedTypesForAccount(gaia_id_hash_)
                  .HasAll(default_enabled_types));

  SyncPrefs::MigrateGlobalDataTypePrefsToAccount(&pref_service_, gaia_id_hash_);

  // All supported types should be considered selected for this account now,
  // except for kAutofill ("Addresses and more") which should've been disabled
  // for custom passphrase users.
  const UserSelectableTypeSet expected_types =
      base::Difference(default_enabled_types, {UserSelectableType::kAutofill});
  SyncPrefs prefs(&pref_service_);
  UserSelectableTypeSet selected_types =
      prefs.GetSelectedTypesForAccount(gaia_id_hash_);
  EXPECT_TRUE(selected_types.HasAll(expected_types));
}

TEST_F(SyncPrefsMigrationTest,
       GlobalToAccount_SuppressesSyncToSigninMigration) {
  base::test::ScopedFeatureList enable_sync_to_signin(
      kReplaceSyncPromosWithSignInPromos);

  SyncPrefs::MigrateGlobalDataTypePrefsToAccount(&pref_service_, gaia_id_hash_);

  // After the GlobalToAccount migration has run, the SyncToSignin migration
  // should not have any effect anymore.
  EXPECT_FALSE(
      SyncPrefs(&pref_service_)
          .MaybeMigratePrefsForSyncToSigninPart1(
              SyncPrefs::SyncAccountState::kSignedInNotSyncing, gaia_id_hash_));
}

TEST_F(SyncPrefsTest, IsTypeDisabledByUserForAccount) {
  base::test::ScopedFeatureList enable_sync_to_signin(
      kReplaceSyncPromosWithSignInPromos);

  ASSERT_FALSE(sync_prefs_->IsTypeDisabledByUserForAccount(
      UserSelectableType::kBookmarks, gaia_id_hash_));
  ASSERT_FALSE(sync_prefs_->IsTypeDisabledByUserForAccount(
      UserSelectableType::kReadingList, gaia_id_hash_));
  ASSERT_FALSE(sync_prefs_->IsTypeDisabledByUserForAccount(
      UserSelectableType::kPasswords, gaia_id_hash_));

  // Set up a policy to disable Bookmarks.
  PrefValueMap policy_prefs;
  SyncPrefs::SetTypeDisabledByPolicy(&policy_prefs,
                                     UserSelectableType::kBookmarks);
  // Copy the policy prefs map over into the PrefService.
  for (const auto& policy_pref : policy_prefs) {
    pref_service_.SetManagedPref(policy_pref.first, policy_pref.second.Clone());
  }

  // Disable Reading List.
  sync_prefs_->SetSelectedTypeForAccount(UserSelectableType::kReadingList,
                                         false, gaia_id_hash_);

  // Enable Passwords.
  sync_prefs_->SetSelectedTypeForAccount(UserSelectableType::kPasswords, true,
                                         gaia_id_hash_);

  // Check for a disabled type by policy.
  EXPECT_FALSE(sync_prefs_->IsTypeDisabledByUserForAccount(
      UserSelectableType::kBookmarks, gaia_id_hash_));
  // Check for a disabled type by user choice.
  EXPECT_TRUE(sync_prefs_->IsTypeDisabledByUserForAccount(
      UserSelectableType::kReadingList, gaia_id_hash_));
  // Check for an enabled type by user choice.
  EXPECT_FALSE(sync_prefs_->IsTypeDisabledByUserForAccount(
      UserSelectableType::kPasswords, gaia_id_hash_));
  // Check for a type with default value.
  EXPECT_FALSE(sync_prefs_->IsTypeDisabledByUserForAccount(
      UserSelectableType::kPreferences, gaia_id_hash_));
}

}  // namespace

}  // namespace syncer
