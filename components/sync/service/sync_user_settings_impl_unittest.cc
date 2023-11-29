// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/service/sync_user_settings_impl.h"

#include <memory>

#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "build/chromeos_buildflags.h"
#include "components/password_manager/core/browser/features/password_features.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/testing_pref_service.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "components/sync/base/features.h"
#include "components/sync/base/model_type.h"
#include "components/sync/base/user_selectable_type.h"
#include "components/sync/service/sync_prefs.h"
#include "components/sync/service/sync_service_crypto.h"
#include "components/trusted_vault/test/fake_trusted_vault_client.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace syncer {

namespace {

ModelTypeSet GetUserTypes() {
  ModelTypeSet user_types = UserTypes();
#if !BUILDFLAG(IS_CHROMEOS_ASH)
  // Ignore all Chrome OS types on non-Chrome OS platforms.
  user_types.RemoveAll(
      {APP_LIST, ARC_PACKAGE, OS_PREFERENCES, OS_PRIORITY_PREFERENCES, PRINTERS,
       PRINTERS_AUTHORIZATION_SERVERS, WIFI_CONFIGURATIONS, WORKSPACE_DESK});
#endif
  return user_types;
}

ModelTypeSet GetPreferredUserTypes(
    const SyncUserSettingsImpl& sync_user_settings) {
  return Intersection(UserTypes(), sync_user_settings.GetPreferredDataTypes());
}

class MockSyncServiceCryptoDelegate : public SyncServiceCrypto::Delegate {
 public:
  MockSyncServiceCryptoDelegate() = default;
  ~MockSyncServiceCryptoDelegate() override = default;

  MOCK_METHOD(void, CryptoStateChanged, (), (override));
  MOCK_METHOD(void, CryptoRequiredUserActionChanged, (), (override));
  MOCK_METHOD(void, ReconfigureDataTypesDueToCrypto, (), (override));
  MOCK_METHOD(void, PassphraseTypeChanged, (PassphraseType), (override));
  MOCK_METHOD(absl::optional<PassphraseType>,
              GetPassphraseType,
              (),
              (const override));
  MOCK_METHOD(void,
              SetEncryptionBootstrapToken,
              (const std::string&),
              (override));
  MOCK_METHOD(std::string, GetEncryptionBootstrapToken, (), (const override));
};

class SyncUserSettingsImplTest : public testing::Test,
                                 public SyncUserSettingsImpl::Delegate {
 protected:
  SyncUserSettingsImplTest() {
    SyncPrefs::RegisterProfilePrefs(pref_service_.registry());
    sync_prefs_ = std::make_unique<SyncPrefs>(&pref_service_);

    sync_service_crypto_ = std::make_unique<SyncServiceCrypto>(
        &sync_service_crypto_delegate_, &trusted_vault_client_);
  }

  // SyncUserSettingsImpl::Delegate implementation.
  bool IsCustomPassphraseAllowed() const override { return true; }

  SyncPrefs::SyncAccountState GetSyncAccountStateForPrefs() const override {
    return sync_account_state_;
  }

  CoreAccountInfo GetSyncAccountInfoForPrefs() const override {
    CoreAccountInfo account;
    account.email = "name@account.com";
    account.gaia = "name";
    account.account_id = CoreAccountId::FromGaiaId(account.gaia);
    return account;
  }

  void SetSyncAccountState(SyncPrefs::SyncAccountState sync_account_state) {
    sync_account_state_ = sync_account_state;
  }

  std::unique_ptr<SyncUserSettingsImpl> MakeSyncUserSettings(
      ModelTypeSet registered_types) {
    return std::make_unique<SyncUserSettingsImpl>(
        /*delegate=*/this, sync_service_crypto_.get(), sync_prefs_.get(),
        registered_types);
  }

  // The order of fields matters because it determines destruction order and
  // fields are dependent.
  TestingPrefServiceSimple pref_service_;
  std::unique_ptr<SyncPrefs> sync_prefs_;
  testing::NiceMock<MockSyncServiceCryptoDelegate>
      sync_service_crypto_delegate_;
  trusted_vault::FakeTrustedVaultClient trusted_vault_client_;
  std::unique_ptr<SyncServiceCrypto> sync_service_crypto_;
  SyncPrefs::SyncAccountState sync_account_state_ =
      SyncPrefs::SyncAccountState::kSyncing;
};

TEST_F(SyncUserSettingsImplTest, PreferredTypesSyncEverything) {
  std::unique_ptr<SyncUserSettingsImpl> sync_user_settings =
      MakeSyncUserSettings(GetUserTypes());

  ModelTypeSet expected_types = GetUserTypes();
  UserSelectableTypeSet all_registered_types =
      sync_user_settings->GetRegisteredSelectableTypes();

#if BUILDFLAG(IS_CHROMEOS_LACROS)
  // Apps sync is controlled by a dedicated preference on Lacros,
  // corresponding to the Apps toggle in OS Sync settings. That pref
  // isn't set up in this test.
  if (base::FeatureList::IsEnabled(kSyncChromeOSAppsToggleSharing)) {
    ASSERT_TRUE(all_registered_types.Has(UserSelectableType::kApps));
    ASSERT_FALSE(sync_prefs_->IsAppsSyncEnabledByOs());
    expected_types.RemoveAll({APPS, APP_SETTINGS, WEB_APPS});
    all_registered_types.Remove(UserSelectableType::kApps);
  }
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)

  EXPECT_TRUE(sync_user_settings->IsSyncEverythingEnabled());
  EXPECT_EQ(expected_types, GetPreferredUserTypes(*sync_user_settings));

  for (UserSelectableType type : all_registered_types) {
    sync_user_settings->SetSelectedTypes(/*sync_everything=*/true, {type});
    EXPECT_EQ(expected_types, GetPreferredUserTypes(*sync_user_settings));
  }
}

TEST_F(SyncUserSettingsImplTest, GetSelectedTypesWhileSignedOut) {
  // Sanity check: signed-in there are selected types.
  SetSyncAccountState(SyncPrefs::SyncAccountState::kSignedInNotSyncing);
  ASSERT_FALSE(
      MakeSyncUserSettings(GetUserTypes())->GetSelectedTypes().Empty());

  // But signed out there are none.
  SetSyncAccountState(SyncPrefs::SyncAccountState::kNotSignedIn);
  EXPECT_TRUE(MakeSyncUserSettings(GetUserTypes())->GetSelectedTypes().Empty());
}

TEST_F(SyncUserSettingsImplTest, DefaultSelectedTypesWhileSignedIn) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      /*enabled_features=*/{kReplaceSyncPromosWithSignInPromos,
                            kEnableBookmarksAccountStorage,
                            kReadingListEnableSyncTransportModeUponSignIn,
                            password_manager::features::
                                kEnablePasswordsAccountStorage,
                            kSyncEnableContactInfoDataTypeInTransportMode,
                            kEnablePreferencesAccountStorage},
      /*disabled_features=*/{});

  std::unique_ptr<SyncUserSettingsImpl> sync_user_settings =
      MakeSyncUserSettings(GetUserTypes());
  SetSyncAccountState(SyncPrefs::SyncAccountState::kSignedInNotSyncing);

  UserSelectableTypeSet registered_types =
      sync_user_settings->GetRegisteredSelectableTypes();
  UserSelectableTypeSet selected_types = sync_user_settings->GetSelectedTypes();
  // History and Tabs require a separate opt-in.
  // Apps, Extensions, Themes, and SavedTabGroups are not supported in transport
  // mode.
  UserSelectableTypeSet expected_disabled_types = {
      UserSelectableType::kHistory, UserSelectableType::kTabs,
      UserSelectableType::kApps,    UserSelectableType::kExtensions,
      UserSelectableType::kThemes,  UserSelectableType::kSavedTabGroups};

#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)
  // On Desktop, kPasswords is disabled by default.
  expected_disabled_types.Put(UserSelectableType::kPasswords);
#endif

  EXPECT_EQ(selected_types,
            Difference(registered_types, expected_disabled_types));
}

TEST_F(SyncUserSettingsImplTest, SetSelectedTypeInTransportMode) {
  SetSyncAccountState(SyncPrefs::SyncAccountState::kSignedInNotSyncing);
  std::unique_ptr<SyncUserSettingsImpl> sync_user_settings =
      MakeSyncUserSettings(GetUserTypes());
  const UserSelectableTypeSet default_types =
      sync_user_settings->GetSelectedTypes();

  sync_user_settings->SetSelectedType(UserSelectableType::kPayments, false);

  EXPECT_EQ(sync_user_settings->GetSelectedTypes(),
            Difference(default_types, {UserSelectableType::kPayments}));

  sync_user_settings->SetSelectedType(UserSelectableType::kPayments, true);

  EXPECT_EQ(sync_user_settings->GetSelectedTypes(), default_types);
}

TEST_F(SyncUserSettingsImplTest, SetSelectedTypeInFullSyncMode) {
  std::unique_ptr<SyncUserSettingsImpl> sync_user_settings =
      MakeSyncUserSettings(GetUserTypes());
  SetSyncAccountState(SyncPrefs::SyncAccountState::kSyncing);

  UserSelectableTypeSet registered_types =
      sync_user_settings->GetRegisteredSelectableTypes();

#if BUILDFLAG(IS_CHROMEOS_LACROS)
  if (base::FeatureList::IsEnabled(kSyncChromeOSAppsToggleSharing)) {
    // Apps sync is controlled by a dedicated preference on Lacros,
    // corresponding to the Apps toggle in OS Sync settings. That pref
    // isn't set up in this test.
    ASSERT_TRUE(registered_types.Has(UserSelectableType::kApps));
    ASSERT_FALSE(sync_prefs_->IsAppsSyncEnabledByOs());
    registered_types.Remove(UserSelectableType::kApps);
  }
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)

  const UserSelectableTypeSet registered_types_except_passwords =
      base::Difference(registered_types,
                       UserSelectableTypeSet({UserSelectableType::kPasswords}));

  ASSERT_NE(registered_types, registered_types_except_passwords);
  ASSERT_EQ(sync_user_settings->GetSelectedTypes(), registered_types);

  // Disable the sync-everything toggle first, which is required to change
  // individual toggles.
  sync_user_settings->SetSelectedTypes(/*sync_everything=*/false,
                                       /*types=*/registered_types);
  ASSERT_EQ(sync_user_settings->GetSelectedTypes(), registered_types);
  ASSERT_FALSE(sync_user_settings->IsSyncEverythingEnabled());

  sync_user_settings->SetSelectedType(UserSelectableType::kPasswords, false);
  EXPECT_EQ(sync_user_settings->GetSelectedTypes(),
            registered_types_except_passwords);

  sync_user_settings->SetSelectedType(UserSelectableType::kPasswords, true);
  EXPECT_EQ(sync_user_settings->GetSelectedTypes(), registered_types);
}

#if BUILDFLAG(IS_CHROMEOS_ASH)
TEST_F(SyncUserSettingsImplTest, PreferredTypesSyncAllOsTypes) {
  std::unique_ptr<SyncUserSettingsImpl> sync_user_settings =
      MakeSyncUserSettings(GetUserTypes());

  ModelTypeSet expected_types = GetUserTypes();
  EXPECT_TRUE(sync_user_settings->IsSyncAllOsTypesEnabled());
  EXPECT_EQ(expected_types, GetPreferredUserTypes(*sync_user_settings));

  for (UserSelectableOsType type : UserSelectableOsTypeSet::All()) {
    sync_user_settings->SetSelectedOsTypes(/*sync_all_os_types=*/true,
                                           /*types=*/{type});
    EXPECT_EQ(expected_types, GetPreferredUserTypes(*sync_user_settings));
  }
}
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

TEST_F(SyncUserSettingsImplTest, PreferredTypesNotKeepEverythingSynced) {
  std::unique_ptr<SyncUserSettingsImpl> sync_user_settings =
      MakeSyncUserSettings(GetUserTypes());

  sync_user_settings->SetSelectedTypes(
      /*sync_everything=*/false,
      /*types=*/UserSelectableTypeSet());
#if BUILDFLAG(IS_CHROMEOS_ASH)
  // GetPreferredUserTypes() returns ModelTypes, which includes both browser
  // and OS types. However, this test exercises browser UserSelectableTypes,
  // so disable OS selectable types.
  sync_user_settings->SetSelectedOsTypes(/*sync_all_os_types=*/false,
                                         UserSelectableOsTypeSet());
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
  // No user selectable types are enabled, so only the "always preferred" types
  // are preferred.
  ASSERT_EQ(AlwaysPreferredUserTypes(),
            GetPreferredUserTypes(*sync_user_settings));

  UserSelectableTypeSet all_registered_types =
      sync_user_settings->GetRegisteredSelectableTypes();

#if BUILDFLAG(IS_CHROMEOS_LACROS)
  if (base::FeatureList::IsEnabled(kSyncChromeOSAppsToggleSharing)) {
    // Apps sync is controlled by a dedicated preference on Lacros,
    // corresponding to the Apps toggle in OS Sync settings. That pref
    // isn't set up in this test.
    ASSERT_TRUE(all_registered_types.Has(UserSelectableType::kApps));
    ASSERT_FALSE(sync_prefs_->IsAppsSyncEnabledByOs());
    all_registered_types.Remove(UserSelectableType::kApps);
  }
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)

  for (UserSelectableType type : all_registered_types) {
    ModelTypeSet expected_preferred_types =
        UserSelectableTypeToAllModelTypes(type);
    expected_preferred_types.PutAll(AlwaysPreferredUserTypes());
    sync_user_settings->SetSelectedTypes(/*sync_everything=*/false,
                                         /*types=*/{type});
    EXPECT_EQ(expected_preferred_types,
              GetPreferredUserTypes(*sync_user_settings));
  }
}

#if BUILDFLAG(IS_CHROMEOS_ASH)
TEST_F(SyncUserSettingsImplTest, PreferredTypesNotAllOsTypesSynced) {
  std::unique_ptr<SyncUserSettingsImpl> sync_user_settings =
      MakeSyncUserSettings(GetUserTypes());

  sync_user_settings->SetSelectedTypes(
      /*sync_everything=*/false,
      /*types=*/UserSelectableTypeSet());
  sync_user_settings->SetSelectedOsTypes(
      /*sync_all_os_types=*/false,
      /*types=*/UserSelectableOsTypeSet());
  EXPECT_FALSE(sync_user_settings->IsSyncEverythingEnabled());
  EXPECT_FALSE(sync_user_settings->IsSyncAllOsTypesEnabled());
  EXPECT_EQ(AlwaysPreferredUserTypes(),
            GetPreferredUserTypes(*sync_user_settings));

  for (UserSelectableOsType type : UserSelectableOsTypeSet::All()) {
    ModelTypeSet expected_preferred_types =
        UserSelectableOsTypeToAllModelTypes(type);
    expected_preferred_types.PutAll(AlwaysPreferredUserTypes());
    sync_user_settings->SetSelectedOsTypes(/*sync_all_os_types=*/false,
                                           /*types=*/{type});
    EXPECT_EQ(expected_preferred_types,
              GetPreferredUserTypes(*sync_user_settings));
  }
}
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

// Device info should always be enabled.
TEST_F(SyncUserSettingsImplTest, DeviceInfo) {
  std::unique_ptr<SyncUserSettingsImpl> sync_user_settings =
      MakeSyncUserSettings(GetUserTypes());
  EXPECT_TRUE(sync_user_settings->GetPreferredDataTypes().Has(DEVICE_INFO));

  UserSelectableTypeSet all_registered_types =
      sync_user_settings->GetRegisteredSelectableTypes();
  sync_user_settings->SetSelectedTypes(
      /*sync_everything=*/true,
      /*types=*/all_registered_types);
  EXPECT_TRUE(sync_user_settings->GetPreferredDataTypes().Has(DEVICE_INFO));

  sync_user_settings->SetSelectedTypes(
      /*sync_everything=*/false,
      /*types=*/all_registered_types);
  EXPECT_TRUE(sync_user_settings->GetPreferredDataTypes().Has(DEVICE_INFO));

  sync_user_settings = MakeSyncUserSettings({DEVICE_INFO});
  sync_user_settings->SetSelectedTypes(
      /*sync_everything=*/false,
      /*types=*/UserSelectableTypeSet());
  EXPECT_TRUE(sync_user_settings->GetPreferredDataTypes().Has(DEVICE_INFO));
}

// User Consents should always be enabled.
TEST_F(SyncUserSettingsImplTest, UserConsents) {
  std::unique_ptr<SyncUserSettingsImpl> sync_user_settings =
      MakeSyncUserSettings(GetUserTypes());
  EXPECT_TRUE(sync_user_settings->GetPreferredDataTypes().Has(USER_CONSENTS));

  UserSelectableTypeSet all_registered_types =
      sync_user_settings->GetRegisteredSelectableTypes();
  sync_user_settings->SetSelectedTypes(
      /*sync_everything=*/true,
      /*types=*/all_registered_types);
  EXPECT_TRUE(sync_user_settings->GetPreferredDataTypes().Has(USER_CONSENTS));

  sync_user_settings->SetSelectedTypes(
      /*sync_everything=*/false,
      /*types=*/all_registered_types);
  EXPECT_TRUE(sync_user_settings->GetPreferredDataTypes().Has(USER_CONSENTS));

  sync_user_settings = MakeSyncUserSettings({USER_CONSENTS});
  sync_user_settings->SetSelectedTypes(
      /*sync_everything=*/false,
      /*types=*/UserSelectableTypeSet());
  EXPECT_TRUE(sync_user_settings->GetPreferredDataTypes().Has(USER_CONSENTS));
}

#if BUILDFLAG(IS_CHROMEOS_ASH)
TEST_F(SyncUserSettingsImplTest, AlwaysPreferredTypes_ChromeOS) {
  std::unique_ptr<SyncUserSettingsImpl> sync_user_settings =
      MakeSyncUserSettings(GetUserTypes());

  // Disable all browser types.
  sync_user_settings->SetSelectedTypes(
      /*sync_everything=*/false,
      /*types=*/UserSelectableTypeSet());

  // Disable all OS types.
  sync_user_settings->SetSelectedOsTypes(
      /*sync_all_os_types=*/false,
      /*types=*/UserSelectableOsTypeSet());

  // Important types are still preferred.
  ModelTypeSet preferred_types = sync_user_settings->GetPreferredDataTypes();
  EXPECT_TRUE(preferred_types.Has(DEVICE_INFO));
  EXPECT_TRUE(preferred_types.Has(USER_CONSENTS));
}

TEST_F(SyncUserSettingsImplTest, AppsAreHandledByOsSettings) {
  std::unique_ptr<SyncUserSettingsImpl> settings =
      MakeSyncUserSettings(GetUserTypes());

  ASSERT_TRUE(settings->IsSyncEverythingEnabled());
  ASSERT_TRUE(settings->IsSyncAllOsTypesEnabled());

  // App model types are enabled.
  EXPECT_TRUE(settings->GetPreferredDataTypes().Has(APP_LIST));
  EXPECT_TRUE(settings->GetPreferredDataTypes().Has(APP_SETTINGS));
  EXPECT_TRUE(settings->GetPreferredDataTypes().Has(APPS));
  EXPECT_TRUE(settings->GetPreferredDataTypes().Has(ARC_PACKAGE));
  EXPECT_TRUE(settings->GetPreferredDataTypes().Has(WEB_APPS));

  // Disable browser types.
  settings->SetSelectedTypes(
      /*sync_everything=*/false,
      /*types=*/UserSelectableTypeSet());

  // App model types are still enabled.
  EXPECT_TRUE(settings->GetPreferredDataTypes().Has(APP_LIST));
  EXPECT_TRUE(settings->GetPreferredDataTypes().Has(APP_SETTINGS));
  EXPECT_TRUE(settings->GetPreferredDataTypes().Has(APPS));
  EXPECT_TRUE(settings->GetPreferredDataTypes().Has(ARC_PACKAGE));
  EXPECT_TRUE(settings->GetPreferredDataTypes().Has(WEB_APPS));

  // Disable OS types.
  settings->SetSelectedOsTypes(
      /*sync_all_os_types=*/false,
      /*types=*/UserSelectableOsTypeSet());

  // Apps are disabled.
  EXPECT_FALSE(settings->GetPreferredDataTypes().Has(APP_LIST));
  EXPECT_FALSE(settings->GetPreferredDataTypes().Has(APP_SETTINGS));
  EXPECT_FALSE(settings->GetPreferredDataTypes().Has(APPS));
  EXPECT_FALSE(settings->GetPreferredDataTypes().Has(ARC_PACKAGE));
  EXPECT_FALSE(settings->GetPreferredDataTypes().Has(WEB_APPS));
}
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

#if BUILDFLAG(IS_CHROMEOS_LACROS)
TEST_F(SyncUserSettingsImplTest, AppsAreHandledByOsSettings) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(kSyncChromeOSAppsToggleSharing);

  std::unique_ptr<SyncUserSettingsImpl> settings =
      MakeSyncUserSettings(GetUserTypes());

  ASSERT_TRUE(settings->IsSyncEverythingEnabled());

  // App model types are disabled by default, even though "Sync everything" is
  // on.
  EXPECT_FALSE(settings->GetPreferredDataTypes().Has(APP_SETTINGS));
  EXPECT_FALSE(settings->GetPreferredDataTypes().Has(APPS));
  EXPECT_FALSE(settings->GetPreferredDataTypes().Has(WEB_APPS));

  // Mimic apps toggle enabled in the OS.
  settings->SetAppsSyncEnabledByOs(true);

  // App model types should become enabled.
  EXPECT_TRUE(settings->GetPreferredDataTypes().Has(APP_SETTINGS));
  EXPECT_TRUE(settings->GetPreferredDataTypes().Has(APPS));
  EXPECT_TRUE(settings->GetPreferredDataTypes().Has(WEB_APPS));

  // Mimic "Sync everything" and all individual types toggle are disabled, app
  // model types should stay enabled.
  settings->SetSelectedTypes(/*sync_everything=*/false,
                             UserSelectableTypeSet());
  ASSERT_FALSE(settings->IsSyncEverythingEnabled());

  EXPECT_TRUE(settings->GetPreferredDataTypes().Has(APP_SETTINGS));
  EXPECT_TRUE(settings->GetPreferredDataTypes().Has(APPS));
  EXPECT_TRUE(settings->GetPreferredDataTypes().Has(WEB_APPS));
}
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)

TEST_F(SyncUserSettingsImplTest, ShouldSyncSessionsOnlyIfOpenTabsIsSelected) {
  ASSERT_FALSE(AlwaysPreferredUserTypes().Has(HISTORY));
  ASSERT_FALSE(AlwaysPreferredUserTypes().Has(HISTORY_DELETE_DIRECTIVES));
  ASSERT_FALSE(AlwaysPreferredUserTypes().Has(SESSIONS));

  std::unique_ptr<SyncUserSettingsImpl> sync_user_settings =
      MakeSyncUserSettings(GetUserTypes());

#if BUILDFLAG(IS_CHROMEOS_ASH)
  // GetPreferredUserTypes() returns ModelTypes, which includes both browser
  // and OS types. However, this test exercises browser UserSelectableTypes,
  // so disable OS selectable types.
  sync_user_settings->SetSelectedOsTypes(/*sync_all_os_types=*/false,
                                         UserSelectableOsTypeSet());
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

  // History and OpenTabs enabled: All the history-related ModelTypes should be
  // enabled.
  sync_user_settings->SetSelectedTypes(
      /*sync_everything=*/false,
      /*types=*/{UserSelectableType::kHistory, UserSelectableType::kTabs});
  EXPECT_EQ(GetPreferredUserTypes(*sync_user_settings),
            Union(AlwaysPreferredUserTypes(),
                  {HISTORY, HISTORY_DELETE_DIRECTIVES, SESSIONS, USER_EVENTS}));

  // History only: SESSIONS is gone.
  sync_user_settings->SetSelectedTypes(
      /*sync_everything=*/false,
      /*types=*/{UserSelectableType::kHistory});
  EXPECT_EQ(GetPreferredUserTypes(*sync_user_settings),
            Union(AlwaysPreferredUserTypes(),
                  {HISTORY, HISTORY_DELETE_DIRECTIVES, USER_EVENTS}));

  // OpenTabs only: Only SESSIONS is there.
  sync_user_settings->SetSelectedTypes(
      /*sync_everything=*/false,
      /*types=*/{UserSelectableType::kTabs});
  EXPECT_EQ(GetPreferredUserTypes(*sync_user_settings),
            Union(AlwaysPreferredUserTypes(), {SESSIONS}));
}

TEST_F(SyncUserSettingsImplTest, ShouldMutePassphrasePrompt) {
  std::unique_ptr<SyncUserSettingsImpl> sync_user_settings =
      MakeSyncUserSettings(GetUserTypes());

  EXPECT_FALSE(
      sync_user_settings->IsPassphrasePromptMutedForCurrentProductVersion());

  sync_user_settings->MarkPassphrasePromptMutedForCurrentProductVersion();
  EXPECT_TRUE(
      sync_user_settings->IsPassphrasePromptMutedForCurrentProductVersion());

  // Clearing the preference should unmute the prompt.
  sync_prefs_->ClearPassphrasePromptMutedProductVersion();
  EXPECT_FALSE(
      sync_user_settings->IsPassphrasePromptMutedForCurrentProductVersion());
}

TEST_F(SyncUserSettingsImplTest, ShouldClearPassphrasePromptMuteUponUpgrade) {
  // Mimic an old product version being written to prefs.
  sync_prefs_->SetPassphrasePromptMutedProductVersion(/*major_version=*/73);

  std::unique_ptr<SyncUserSettingsImpl> sync_user_settings =
      MakeSyncUserSettings(GetUserTypes());

  EXPECT_FALSE(
      sync_user_settings->IsPassphrasePromptMutedForCurrentProductVersion());

  // Muting should still work.
  sync_user_settings->MarkPassphrasePromptMutedForCurrentProductVersion();
  EXPECT_TRUE(
      sync_user_settings->IsPassphrasePromptMutedForCurrentProductVersion());
}

// Protects against GetSelectedTypes() incorrectly requiring a
// SetBookmarksAndReadingListAccountStorageOptIn() for syncing users.
// TODO(crbug.com/1440628): Remove when the temporary opt-in is deleted.
TEST_F(SyncUserSettingsImplTest, BookmarksOnByDefaultForSyncingUsers) {
  SetSyncAccountState(SyncPrefs::SyncAccountState::kSyncing);
  std::unique_ptr<SyncUserSettingsImpl> sync_user_settings =
      MakeSyncUserSettings(GetUserTypes());

  EXPECT_TRUE(sync_user_settings->GetSelectedTypes().Has(
      UserSelectableType::kBookmarks));
}

}  // namespace

}  // namespace syncer
