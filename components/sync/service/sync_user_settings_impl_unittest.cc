// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/service/sync_user_settings_impl.h"

#include <memory>

#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/testing_pref_service.h"
#include "components/saved_tab_groups/public/pref_names.h"
#include "components/signin/public/base/signin_pref_names.h"
#include "components/signin/public/base/signin_prefs.h"
#include "components/signin/public/base/signin_switches.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/sync/base/data_type.h"
#include "components/sync/base/features.h"
#include "components/sync/base/user_selectable_type.h"
#include "components/sync/service/glue/sync_transport_data_prefs.h"
#include "components/sync/service/sync_prefs.h"
#include "components/sync/service/sync_service_crypto.h"
#include "components/trusted_vault/test/fake_trusted_vault_client.h"
#include "google_apis/gaia/gaia_id.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace syncer {

namespace {

using testing::ContainerEq;
using testing::Return;

constexpr GaiaId::Literal kTestGaiaId("1111");

DataTypeSet GetUserTypes() {
  DataTypeSet user_types = UserTypes();
#if !BUILDFLAG(IS_CHROMEOS)
  // Ignore all Chrome OS types on non-Chrome OS platforms.
  user_types.RemoveAll({APP_LIST, ARC_PACKAGE, OS_PREFERENCES,
                        OS_PRIORITY_PREFERENCES, PRINTERS,
                        PRINTERS_AUTHORIZATION_SERVERS, WIFI_CONFIGURATIONS});
#endif
  return user_types;
}

DataTypeSet GetPreferredUserTypes(
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
  MOCK_METHOD(std::optional<PassphraseType>,
              GetPassphraseType,
              (),
              (const override));
  MOCK_METHOD(void,
              SetEncryptionBootstrapToken,
              (const std::string&),
              (override));
  MOCK_METHOD(std::string, GetEncryptionBootstrapToken, (), (const override));
};

class MockDelegate : public SyncUserSettingsImpl::Delegate {
 public:
  MockDelegate() = default;
  ~MockDelegate() override = default;

  MOCK_METHOD(bool, IsCustomPassphraseAllowed, (), (const override));
  MOCK_METHOD(SyncPrefs::SyncAccountState,
              GetSyncAccountStateForPrefs,
              (),
              (const override));
  MOCK_METHOD(CoreAccountInfo,
              GetSyncAccountInfoForPrefs,
              (),
              (const override));
  MOCK_METHOD(void, OnSyncClientDisabledByPolicyChanged, (), (override));
  MOCK_METHOD(void, OnSelectedTypesChanged, (), (override));
#if BUILDFLAG(IS_CHROMEOS)
  MOCK_METHOD(void, OnSyncFeatureDisabledViaDashboardCleared, (), (override));
#else   // BUILDFLAG(IS_CHROMEOS)
  MOCK_METHOD(void, OnInitialSyncFeatureSetupCompleted, (), (override));
#endif  // BUILDFLAG(IS_CHROMEOS)
};

class SyncUserSettingsImplTest : public testing::Test {
 protected:
  SyncUserSettingsImplTest() {
    SyncPrefs::RegisterProfilePrefs(pref_service_.registry());
    SigninPrefs::RegisterProfilePrefs(pref_service_.registry());
    SyncTransportDataPrefs::RegisterProfilePrefs(pref_service_.registry());
    signin::IdentityManager::RegisterProfilePrefs(pref_service_.registry());
    // TODO(crbug.com/368409110): Necessary for a workaround in
    // SyncPrefs::KeepAccountSettingsPrefsOnlyForUsers(); see TODO there.
    pref_service_.registry()->RegisterDictionaryPref(
        tab_groups::prefs::kLocallyClosedRemoteTabGroupIds,
        base::Value::Dict());
    sync_prefs_ = std::make_unique<SyncPrefs>(&pref_service_);

    sync_service_crypto_ = std::make_unique<SyncServiceCrypto>(
        &sync_service_crypto_delegate_, &trusted_vault_client_);

    ON_CALL(delegate_, IsCustomPassphraseAllowed).WillByDefault(Return(true));
    ON_CALL(delegate_, GetSyncAccountStateForPrefs)
        .WillByDefault(Return(SyncPrefs::SyncAccountState::kSyncing));
    ON_CALL(delegate_, GetSyncAccountInfoForPrefs).WillByDefault([]() {
      CoreAccountInfo account;
      account.email = "name@account.com";
      account.gaia = kTestGaiaId;
      account.account_id = CoreAccountId::FromGaiaId(account.gaia);
      return account;
    });
  }

  void SetSyncAccountState(SyncPrefs::SyncAccountState sync_account_state) {
    ON_CALL(delegate_, GetSyncAccountStateForPrefs)
        .WillByDefault(Return(sync_account_state));
#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)
    if (sync_account_state ==
        SyncPrefs::SyncAccountState::kSignedInWithoutSyncConsent) {
      pref_service_.SetBoolean(prefs::kExplicitBrowserSignin, true);
    }
#endif
  }

  std::unique_ptr<SyncUserSettingsImpl> MakeSyncUserSettings(
      DataTypeSet registered_types) {
    return std::make_unique<SyncUserSettingsImpl>(
        &delegate_, sync_service_crypto_.get(), sync_prefs_.get(),
        registered_types);
  }

  base::test::SingleThreadTaskEnvironment task_environment_;
  // The order of fields matters because it determines destruction order and
  // fields are dependent.
  TestingPrefServiceSimple pref_service_;
  std::unique_ptr<SyncPrefs> sync_prefs_;
  testing::NiceMock<MockSyncServiceCryptoDelegate>
      sync_service_crypto_delegate_;
  testing::NiceMock<MockDelegate> delegate_;
  trusted_vault::FakeTrustedVaultClient trusted_vault_client_;
  std::unique_ptr<SyncServiceCrypto> sync_service_crypto_;
};

TEST_F(SyncUserSettingsImplTest, PreferredTypesSyncEverything) {
  std::unique_ptr<SyncUserSettingsImpl> sync_user_settings =
      MakeSyncUserSettings(GetUserTypes());

  DataTypeSet expected_types = GetUserTypes();
  UserSelectableTypeSet all_registered_types =
      sync_user_settings->GetRegisteredSelectableTypes();

  // TODO(crbug.com/445841720): In CL #3, delete (AI_THREAD is now mapped to a
  // selectable type.
  expected_types.Remove(AI_THREAD);
  // TODO(crbug.com/445840788): In CL #3, delete (CONTEXTUAL_TASK is now mapped
  // to a selectable type.
  expected_types.Remove(CONTEXTUAL_TASK);

#if BUILDFLAG(IS_CHROMEOS)
  expected_types.RemoveAll({WEB_APKS});
#endif  // BUILDFLAG(IS_CHROMEOS)

  EXPECT_TRUE(sync_user_settings->IsSyncEverythingEnabled());
  EXPECT_THAT(GetPreferredUserTypes(*sync_user_settings),
              ContainerEq(expected_types));

  for (UserSelectableType type : all_registered_types) {
    sync_user_settings->SetSelectedTypes(/*sync_everything=*/true, {type});
    EXPECT_THAT(GetPreferredUserTypes(*sync_user_settings),
                ContainerEq(expected_types));
  }
}

TEST_F(SyncUserSettingsImplTest, GetSelectedTypesWhileSignedOut) {
  // Sanity check: signed-in there are selected types.
  SetSyncAccountState(SyncPrefs::SyncAccountState::kSignedInWithoutSyncConsent);
  ASSERT_FALSE(
      MakeSyncUserSettings(GetUserTypes())->GetSelectedTypes().empty());

  // But signed out there are none.
  SetSyncAccountState(SyncPrefs::SyncAccountState::kNotSignedIn);
  EXPECT_TRUE(MakeSyncUserSettings(GetUserTypes())->GetSelectedTypes().empty());
}

// kReplaceSyncPromosWithSignInPromos has been enabled by default on mobile
// platforms for a long time, so the feature-disabled case is not worth testing.
#if !BUILDFLAG(IS_IOS) && !BUILDFLAG(IS_ANDROID)
TEST_F(SyncUserSettingsImplTest,
       DefaultSelectedTypesWhileSignedIn_SyncToSigninDisabled) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      /*enabled_features=*/{switches::kSyncEnableBookmarksInTransportMode,
                            kReadingListEnableSyncTransportModeUponSignIn,
                            kSeparateLocalAndAccountSearchEngines,
                            syncer::kSeparateLocalAndAccountThemes,
                            switches::kEnablePreferencesAccountStorage},
      /*disabled_features=*/{kReplaceSyncPromosWithSignInPromos});

  std::unique_ptr<SyncUserSettingsImpl> sync_user_settings =
      MakeSyncUserSettings(GetUserTypes());
  SetSyncAccountState(SyncPrefs::SyncAccountState::kSignedInWithoutSyncConsent);

  UserSelectableTypeSet expected_types = {UserSelectableType::kPasswords,
                                          UserSelectableType::kAutofill,
                                          UserSelectableType::kPayments};

  EXPECT_THAT(sync_user_settings->GetSelectedTypes(),
              ContainerEq(expected_types));

  // Some types may be enabled via opt-in.
  pref_service_.SetBoolean(
      ::prefs::kPrefsThemesSearchEnginesAccountStorageEnabled, true);
  expected_types.Put(UserSelectableType::kPreferences);
  expected_types.Put(UserSelectableType::kThemes);
  EXPECT_THAT(sync_user_settings->GetSelectedTypes(),
              ContainerEq(expected_types));

  SigninPrefs(pref_service_)
      .SetBookmarksExplicitBrowserSignin(kTestGaiaId, true);
  expected_types.Put(UserSelectableType::kBookmarks);
  expected_types.Put(UserSelectableType::kReadingList);
  EXPECT_THAT(sync_user_settings->GetSelectedTypes(),
              ContainerEq(expected_types));
}
#endif  // !BUILDFLAG(IS_IOS) && !BUILDFLAG(IS_ANDROID)

TEST_F(SyncUserSettingsImplTest,
       DefaultSelectedTypesWhileSignedIn_SyncToSigninEnabled) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      /*enabled_features=*/{switches::kSyncEnableBookmarksInTransportMode,
                            kReplaceSyncPromosWithSignInPromos,
#if !BUILDFLAG(IS_IOS) && !BUILDFLAG(IS_ANDROID)
                            kReadingListEnableSyncTransportModeUponSignIn,
                            kSeparateLocalAndAccountSearchEngines,
                            syncer::kSeparateLocalAndAccountThemes,
#endif
                            switches::kEnablePreferencesAccountStorage},
      /*disabled_features=*/{});

  std::unique_ptr<SyncUserSettingsImpl> sync_user_settings =
      MakeSyncUserSettings(GetUserTypes());
  SetSyncAccountState(SyncPrefs::SyncAccountState::kSignedInWithoutSyncConsent);

  const UserSelectableTypeSet registered_types =
      sync_user_settings->GetRegisteredSelectableTypes();
  // History and Tabs require a separate opt-in.
  // SavedTabGroups also requires a separate opt-in, either the same one as
  // history and tabs (on mobile), or a dedicated opt-in.
  // Cookies are not supported in transport mode.
  UserSelectableTypeSet expected_disabled_types = {
      UserSelectableType::kHistory, UserSelectableType::kTabs,
      UserSelectableType::kSavedTabGroups, UserSelectableType::kCookies};

#if BUILDFLAG(IS_CHROMEOS)
  // Extensions syncing in transport mode is not supported on ChromeOS.
  expected_disabled_types.Put(UserSelectableType::kExtensions);
#endif  // BUILDFLAG(IS_CHROMEOS)

#if BUILDFLAG(IS_IOS) || BUILDFLAG(IS_ANDROID)
  // Themes is not supported on mobile.
  expected_disabled_types.Put(UserSelectableType::kThemes);
#endif

  EXPECT_THAT(
      sync_user_settings->GetSelectedTypes(),
      ContainerEq(Difference(registered_types, expected_disabled_types)));
}

TEST_F(SyncUserSettingsImplTest, SetSelectedTypeInTransportMode) {
  SetSyncAccountState(SyncPrefs::SyncAccountState::kSignedInWithoutSyncConsent);
  std::unique_ptr<SyncUserSettingsImpl> sync_user_settings =
      MakeSyncUserSettings(GetUserTypes());
  const UserSelectableTypeSet default_types =
      sync_user_settings->GetSelectedTypes();

  sync_user_settings->SetSelectedType(UserSelectableType::kPayments, false);

  EXPECT_THAT(
      sync_user_settings->GetSelectedTypes(),
      ContainerEq(Difference(default_types, {UserSelectableType::kPayments})));

  EXPECT_CALL(delegate_, OnSelectedTypesChanged());
  sync_user_settings->SetSelectedType(UserSelectableType::kPayments, true);

  EXPECT_EQ(sync_user_settings->GetSelectedTypes(), default_types);
}

TEST_F(SyncUserSettingsImplTest, SetSelectedTypeInFullSyncMode) {
  std::unique_ptr<SyncUserSettingsImpl> sync_user_settings =
      MakeSyncUserSettings(GetUserTypes());
  SetSyncAccountState(SyncPrefs::SyncAccountState::kSyncing);

  UserSelectableTypeSet registered_types =
      sync_user_settings->GetRegisteredSelectableTypes();

  const UserSelectableTypeSet registered_types_except_passwords =
      base::Difference(registered_types,
                       UserSelectableTypeSet({UserSelectableType::kPasswords}));

  ASSERT_NE(registered_types, registered_types_except_passwords);
  ASSERT_EQ(sync_user_settings->GetSelectedTypes(), registered_types);

  // Disable the sync-everything toggle first, which is required to change
  // individual toggles.
  EXPECT_CALL(delegate_, OnSelectedTypesChanged());
  sync_user_settings->SetSelectedTypes(/*sync_everything=*/false,
                                       /*types=*/registered_types);
  ASSERT_EQ(sync_user_settings->GetSelectedTypes(), registered_types);
  ASSERT_FALSE(sync_user_settings->IsSyncEverythingEnabled());
  testing::Mock::VerifyAndClearExpectations(&delegate_);

  EXPECT_CALL(delegate_, OnSelectedTypesChanged());
  sync_user_settings->SetSelectedType(UserSelectableType::kPasswords, false);
  EXPECT_EQ(sync_user_settings->GetSelectedTypes(),
            registered_types_except_passwords);
  testing::Mock::VerifyAndClearExpectations(&delegate_);

  EXPECT_CALL(delegate_, OnSelectedTypesChanged());
  sync_user_settings->SetSelectedType(UserSelectableType::kPasswords, true);
  EXPECT_EQ(sync_user_settings->GetSelectedTypes(), registered_types);
}

#if BUILDFLAG(IS_CHROMEOS)
TEST_F(SyncUserSettingsImplTest, PreferredTypesSyncAllOsTypes) {
  std::unique_ptr<SyncUserSettingsImpl> sync_user_settings =
      MakeSyncUserSettings(GetUserTypes());

  DataTypeSet expected_types = GetUserTypes();
  expected_types.RemoveAll({WEB_APKS});
  // TODO(crbug.com/397767033): In CL #3, delete (AI_THREAD is now mapped to a
  // selectable type.
  expected_types.Remove(AI_THREAD);
  // TODO(crbug.com/397767033): In CL #3, delete (CONTEXTUAL_TASK is now mapped
  // to a selectable type.
  expected_types.Remove(CONTEXTUAL_TASK);
  EXPECT_TRUE(sync_user_settings->IsSyncAllOsTypesEnabled());
  EXPECT_THAT(GetPreferredUserTypes(*sync_user_settings),
              ContainerEq(expected_types));

  for (UserSelectableOsType type : UserSelectableOsTypeSet::All()) {
    sync_user_settings->SetSelectedOsTypes(/*sync_all_os_types=*/true,
                                           /*types=*/{type});
    EXPECT_THAT(GetPreferredUserTypes(*sync_user_settings),
                ContainerEq(expected_types));
  }
}
#endif  // BUILDFLAG(IS_CHROMEOS)

TEST_F(SyncUserSettingsImplTest, PreferredTypesNotKeepEverythingSynced) {
  std::unique_ptr<SyncUserSettingsImpl> sync_user_settings =
      MakeSyncUserSettings(GetUserTypes());

  sync_user_settings->SetSelectedTypes(
      /*sync_everything=*/false,
      /*types=*/UserSelectableTypeSet());
#if BUILDFLAG(IS_CHROMEOS)
  // GetPreferredUserTypes() returns DataTypes, which includes both browser
  // and OS types. However, this test exercises browser UserSelectableTypes,
  // so disable OS selectable types.
  sync_user_settings->SetSelectedOsTypes(/*sync_all_os_types=*/false,
                                         UserSelectableOsTypeSet());
#endif  // BUILDFLAG(IS_CHROMEOS)
  // No user selectable types are enabled, so only the "always preferred" types
  // are preferred.
  ASSERT_EQ(AlwaysPreferredUserTypes(),
            GetPreferredUserTypes(*sync_user_settings));

  UserSelectableTypeSet all_registered_types =
      sync_user_settings->GetRegisteredSelectableTypes();

  for (UserSelectableType type : all_registered_types) {
    DataTypeSet expected_preferred_types =
        UserSelectableTypeToAllDataTypes(type);
    expected_preferred_types.PutAll(AlwaysPreferredUserTypes());
    sync_user_settings->SetSelectedTypes(/*sync_everything=*/false,
                                         /*types=*/{type});
    EXPECT_EQ(expected_preferred_types,
              GetPreferredUserTypes(*sync_user_settings));
  }
}

#if BUILDFLAG(IS_CHROMEOS)
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
    DataTypeSet expected_preferred_types =
        UserSelectableOsTypeToAllDataTypes(type);
    expected_preferred_types.PutAll(AlwaysPreferredUserTypes());
    sync_user_settings->SetSelectedOsTypes(/*sync_all_os_types=*/false,
                                           /*types=*/{type});
    EXPECT_EQ(expected_preferred_types,
              GetPreferredUserTypes(*sync_user_settings));
  }
}
#endif  // BUILDFLAG(IS_CHROMEOS)

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

#if BUILDFLAG(IS_CHROMEOS)
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
  DataTypeSet preferred_types = sync_user_settings->GetPreferredDataTypes();
  EXPECT_TRUE(preferred_types.Has(DEVICE_INFO));
  EXPECT_TRUE(preferred_types.Has(USER_CONSENTS));
}

TEST_F(SyncUserSettingsImplTest, AppsAreHandledByOsSettings) {
  std::unique_ptr<SyncUserSettingsImpl> settings =
      MakeSyncUserSettings(GetUserTypes());

  ASSERT_TRUE(settings->IsSyncEverythingEnabled());
  ASSERT_TRUE(settings->IsSyncAllOsTypesEnabled());

  // App data types are enabled.
  EXPECT_TRUE(settings->GetPreferredDataTypes().Has(APP_LIST));
  EXPECT_TRUE(settings->GetPreferredDataTypes().Has(APP_SETTINGS));
  EXPECT_TRUE(settings->GetPreferredDataTypes().Has(APPS));
  EXPECT_TRUE(settings->GetPreferredDataTypes().Has(ARC_PACKAGE));
  EXPECT_TRUE(settings->GetPreferredDataTypes().Has(WEB_APPS));

  // Disable browser types.
  settings->SetSelectedTypes(
      /*sync_everything=*/false,
      /*types=*/UserSelectableTypeSet());

  // App data types are still enabled.
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
#endif  // BUILDFLAG(IS_CHROMEOS)

TEST_F(SyncUserSettingsImplTest, ShouldSyncSessionsOnlyIfOpenTabsIsSelected) {
  ASSERT_FALSE(AlwaysPreferredUserTypes().Has(HISTORY));
  ASSERT_FALSE(AlwaysPreferredUserTypes().Has(HISTORY_DELETE_DIRECTIVES));
  ASSERT_FALSE(AlwaysPreferredUserTypes().Has(SESSIONS));

  std::unique_ptr<SyncUserSettingsImpl> sync_user_settings =
      MakeSyncUserSettings(GetUserTypes());

#if BUILDFLAG(IS_CHROMEOS)
  // GetPreferredUserTypes() returns DataTypes, which includes both browser
  // and OS types. However, this test exercises browser UserSelectableTypes,
  // so disable OS selectable types.
  sync_user_settings->SetSelectedOsTypes(/*sync_all_os_types=*/false,
                                         UserSelectableOsTypeSet());
#endif  // BUILDFLAG(IS_CHROMEOS)

  // History and OpenTabs enabled: All the history-related DataTypes should be
  // enabled.
  sync_user_settings->SetSelectedTypes(
      /*sync_everything=*/false,
      /*types=*/{UserSelectableType::kHistory, UserSelectableType::kTabs});
#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_IOS)
  // For android and iOS, we enable SAVED_TAB_GROUP under OpenTabs as well.
  EXPECT_EQ(
      GetPreferredUserTypes(*sync_user_settings),
      Union(AlwaysPreferredUserTypes(),
            {COLLABORATION_GROUP, HISTORY, HISTORY_DELETE_DIRECTIVES,
             SAVED_TAB_GROUP, SHARED_COMMENT, SHARED_TAB_GROUP_DATA, SESSIONS,
             USER_EVENTS, SHARED_TAB_GROUP_ACCOUNT_DATA, WORKSPACE_DESK}));
#else
  EXPECT_EQ(GetPreferredUserTypes(*sync_user_settings),
            Union(AlwaysPreferredUserTypes(),
                  {HISTORY, HISTORY_DELETE_DIRECTIVES, SESSIONS, USER_EVENTS,
                   WORKSPACE_DESK}));
#endif  // BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_IOS)

  // History only: SESSIONS-related types are gone.
  sync_user_settings->SetSelectedTypes(
      /*sync_everything=*/false,
      /*types=*/{UserSelectableType::kHistory});
  EXPECT_EQ(GetPreferredUserTypes(*sync_user_settings),
            Union(AlwaysPreferredUserTypes(),
                  {HISTORY, HISTORY_DELETE_DIRECTIVES, USER_EVENTS}));

  // OpenTabs only: HISTORY-related types are gone.
  sync_user_settings->SetSelectedTypes(
      /*sync_everything=*/false,
      /*types=*/{UserSelectableType::kTabs});
#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_IOS)
  EXPECT_EQ(GetPreferredUserTypes(*sync_user_settings),
            Union(AlwaysPreferredUserTypes(),
                  {COLLABORATION_GROUP, SAVED_TAB_GROUP, SESSIONS,
                   SHARED_TAB_GROUP_DATA, SHARED_TAB_GROUP_ACCOUNT_DATA,
                   WORKSPACE_DESK, SHARED_COMMENT}));
#else
  EXPECT_EQ(GetPreferredUserTypes(*sync_user_settings),
            Union(AlwaysPreferredUserTypes(), {SESSIONS, WORKSPACE_DESK}));
#endif  // BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_IOS)

// SavedTabGroups enabled on desktop. It should enable both saved tab groups and
// shared tab groups.
#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)
  sync_user_settings->SetSelectedTypes(
      /*sync_everything=*/false,
      /*types=*/{UserSelectableType::kSavedTabGroups});
  EXPECT_EQ(GetPreferredUserTypes(*sync_user_settings),
            Union(AlwaysPreferredUserTypes(),
                  {COLLABORATION_GROUP, SAVED_TAB_GROUP, SHARED_COMMENT,
                   SHARED_TAB_GROUP_DATA, SHARED_TAB_GROUP_ACCOUNT_DATA}));
#endif  // !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)
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

TEST_F(SyncUserSettingsImplTest, EncryptionBootstrapTokenForSyncingUser) {
  SetSyncAccountState(SyncPrefs::SyncAccountState::kSyncing);
  std::unique_ptr<SyncUserSettingsImpl> sync_user_settings =
      MakeSyncUserSettings(GetUserTypes());
  ASSERT_TRUE(sync_user_settings->GetEncryptionBootstrapToken().empty());
  sync_user_settings->SetEncryptionBootstrapToken("token");
  EXPECT_EQ("token", sync_user_settings->GetEncryptionBootstrapToken());
  EXPECT_EQ(sync_user_settings->GetEncryptionBootstrapToken(),
            sync_prefs_->GetEncryptionBootstrapTokenForAccount(kTestGaiaId));
  sync_prefs_->ClearEncryptionBootstrapTokenForAccount(kTestGaiaId);
  EXPECT_TRUE(sync_user_settings->GetEncryptionBootstrapToken().empty());
}

TEST_F(SyncUserSettingsImplTest, EncryptionBootstrapTokenPerAccountSignedOut) {
  SetSyncAccountState(SyncPrefs::SyncAccountState::kNotSignedIn);
  std::unique_ptr<SyncUserSettingsImpl> sync_user_settings =
      MakeSyncUserSettings(GetUserTypes());
  EXPECT_TRUE(sync_user_settings->GetEncryptionBootstrapToken().empty());
}

TEST_F(SyncUserSettingsImplTest, EncryptionBootstrapTokenPerAccount) {
  SetSyncAccountState(SyncPrefs::SyncAccountState::kSignedInWithoutSyncConsent);
  std::unique_ptr<SyncUserSettingsImpl> sync_user_settings =
      MakeSyncUserSettings(GetUserTypes());
  ASSERT_TRUE(sync_user_settings->GetEncryptionBootstrapToken().empty());
  sync_user_settings->SetEncryptionBootstrapToken("token");
  EXPECT_EQ("token", sync_user_settings->GetEncryptionBootstrapToken());
  EXPECT_EQ(sync_user_settings->GetEncryptionBootstrapToken(),
            sync_prefs_->GetEncryptionBootstrapTokenForAccount(kTestGaiaId));
}

TEST_F(SyncUserSettingsImplTest, ClearEncryptionBootstrapTokenPerAccount) {
  SetSyncAccountState(SyncPrefs::SyncAccountState::kSignedInWithoutSyncConsent);
  std::unique_ptr<SyncUserSettingsImpl> sync_user_settings =
      MakeSyncUserSettings(GetUserTypes());
  ASSERT_TRUE(sync_user_settings->GetEncryptionBootstrapToken().empty());
  sync_user_settings->SetEncryptionBootstrapToken("token");
  sync_user_settings->KeepAccountSettingsPrefsOnlyForUsers({kTestGaiaId});
  EXPECT_EQ("token", sync_user_settings->GetEncryptionBootstrapToken());
  sync_user_settings->KeepAccountSettingsPrefsOnlyForUsers({});
  EXPECT_TRUE(sync_user_settings->GetEncryptionBootstrapToken().empty());
}

#if BUILDFLAG(IS_CHROMEOS)
TEST_F(SyncUserSettingsImplTest, SyncFeatureDisabledViaDashboard) {
  std::unique_ptr<SyncUserSettingsImpl> sync_user_settings =
      MakeSyncUserSettings(GetUserTypes());

  ASSERT_FALSE(sync_user_settings->IsSyncFeatureDisabledViaDashboard());

  EXPECT_CALL(delegate_, OnSyncFeatureDisabledViaDashboardCleared).Times(0);
  sync_user_settings->SetSyncFeatureDisabledViaDashboard();
  EXPECT_TRUE(sync_user_settings->IsSyncFeatureDisabledViaDashboard());

  EXPECT_CALL(delegate_, OnSyncFeatureDisabledViaDashboardCleared);
  sync_user_settings->ClearSyncFeatureDisabledViaDashboard();
  EXPECT_FALSE(sync_user_settings->IsSyncFeatureDisabledViaDashboard());

  // Calling it for the second time should be harmless (no-op).
  EXPECT_CALL(delegate_, OnSyncFeatureDisabledViaDashboardCleared).Times(0);
  sync_user_settings->ClearSyncFeatureDisabledViaDashboard();
  EXPECT_FALSE(sync_user_settings->IsSyncFeatureDisabledViaDashboard());
}

TEST_F(SyncUserSettingsImplTest,
       PreferredDataTypesWhileSyncFeatureDisabledViaDashboard) {
  std::unique_ptr<SyncUserSettingsImpl> sync_user_settings =
      MakeSyncUserSettings(GetUserTypes());

  ASSERT_FALSE(sync_user_settings->IsSyncFeatureDisabledViaDashboard());
  ASSERT_TRUE(sync_user_settings->GetPreferredDataTypes().HasAll(
      {NIGORI, DEVICE_INFO, BOOKMARKS}));

  sync_user_settings->SetSyncFeatureDisabledViaDashboard();

  ASSERT_TRUE(sync_user_settings->IsSyncFeatureDisabledViaDashboard());
  EXPECT_TRUE(sync_user_settings->GetPreferredDataTypes().HasAll(
      {NIGORI, DEVICE_INFO}));
  EXPECT_FALSE(sync_user_settings->GetPreferredDataTypes().Has(BOOKMARKS));
}
#else   // BUILDFLAG(IS_CHROMEOS)
TEST_F(SyncUserSettingsImplTest, SetInitialSyncFeatureSetupComplete) {
  std::unique_ptr<SyncUserSettingsImpl> sync_user_settings =
      MakeSyncUserSettings(GetUserTypes());

  ASSERT_FALSE(sync_user_settings->IsInitialSyncFeatureSetupComplete());

  EXPECT_CALL(delegate_, OnInitialSyncFeatureSetupCompleted());
  sync_user_settings->SetInitialSyncFeatureSetupComplete(
      SyncFirstSetupCompleteSource::BASIC_FLOW);

  EXPECT_TRUE(sync_user_settings->IsInitialSyncFeatureSetupComplete());
}
#endif  // BUILDFLAG(IS_CHROMEOS)

}  // namespace

}  // namespace syncer
