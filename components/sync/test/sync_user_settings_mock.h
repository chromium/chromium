// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_TEST_SYNC_USER_SETTINGS_MOCK_H_
#define COMPONENTS_SYNC_TEST_SYNC_USER_SETTINGS_MOCK_H_

#include <memory>
#include <string>
#include <vector>

#include "build/chromeos_buildflags.h"
#include "components/signin/public/base/gaia_id_hash.h"
#include "components/sync/engine/nigori/nigori.h"
#include "components/sync/service/sync_user_settings.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace syncer {

class SyncUserSettingsMock : public SyncUserSettings {
 public:
  SyncUserSettingsMock();
  ~SyncUserSettingsMock() override;
  MOCK_METHOD(bool, IsInitialSyncFeatureSetupComplete, (), (const override));
#if !BUILDFLAG(IS_CHROMEOS_ASH)
  MOCK_METHOD(void,
              SetInitialSyncFeatureSetupComplete,
              (SyncFirstSetupCompleteSource),
              (override));
#endif  // !BUILDFLAG(IS_CHROMEOS_ASH)
  MOCK_METHOD(bool, IsSyncEverythingEnabled, (), (const override));
  MOCK_METHOD(UserSelectableTypeSet, GetSelectedTypes, (), (const override));
  MOCK_METHOD(bool,
              IsTypeManagedByPolicy,
              (UserSelectableType),
              (const override));
  MOCK_METHOD(bool,
              IsTypeManagedByCustodian,
              (UserSelectableType),
              (const override));
  MOCK_METHOD(SyncUserSettings::UserSelectableTypePrefState,
              GetTypePrefStateForAccount,
              (UserSelectableType),
              (const override));
#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)
  MOCK_METHOD(int,
              GetNumberOfAccountsWithPasswordsSelected,
              (),
              (const override));
#endif
  MOCK_METHOD(void,
              SetSelectedTypes,
              (bool, UserSelectableTypeSet),
              (override));
  MOCK_METHOD(void, SetSelectedType, (UserSelectableType, bool), (override));
  MOCK_METHOD(void,
              KeepAccountSettingsPrefsOnlyForUsers,
              (const std::vector<signin::GaiaIdHash>&),
              (override));
  MOCK_METHOD(UserSelectableTypeSet,
              GetRegisteredSelectableTypes,
              (),
              (const override));

#if BUILDFLAG(IS_CHROMEOS_ASH)
  MOCK_METHOD(bool, IsSyncFeatureDisabledViaDashboard, (), (const override));
  MOCK_METHOD(bool, IsSyncAllOsTypesEnabled, (), (const override));
  MOCK_METHOD(UserSelectableOsTypeSet,
              GetSelectedOsTypes,
              (),
              (const override));
  MOCK_METHOD(bool,
              IsOsTypeManagedByPolicy,
              (UserSelectableOsType),
              (const override));
  MOCK_METHOD(void,
              SetSelectedOsTypes,
              (bool, UserSelectableOsTypeSet),
              (override));
  MOCK_METHOD(UserSelectableOsTypeSet,
              GetRegisteredSelectableOsTypes,
              (),
              (const override));
#endif

#if BUILDFLAG(IS_CHROMEOS_LACROS)
  MOCK_METHOD(void, SetAppsSyncEnabledByOs, (bool), (override));
#endif

  MOCK_METHOD(bool, IsCustomPassphraseAllowed, (), (const override));
  MOCK_METHOD(bool, IsEncryptEverythingEnabled, (), (const override));
  MOCK_METHOD(DataTypeSet, GetAllEncryptedDataTypes, (), (const override));
  MOCK_METHOD(bool, IsPassphraseRequired, (), (const override));
  MOCK_METHOD(bool,
              IsPassphraseRequiredForPreferredDataTypes,
              (),
              (const override));
  MOCK_METHOD(bool,
              IsPassphrasePromptMutedForCurrentProductVersion,
              (),
              (const override));
  MOCK_METHOD(void,
              MarkPassphrasePromptMutedForCurrentProductVersion,
              (),
              (override));
  MOCK_METHOD(bool, IsTrustedVaultKeyRequired, (), (const override));
  MOCK_METHOD(bool,
              IsTrustedVaultKeyRequiredForPreferredDataTypes,
              (),
              (const override));
  MOCK_METHOD(bool, IsTrustedVaultRecoverabilityDegraded, (), (const override));
  MOCK_METHOD(bool, IsUsingExplicitPassphrase, (), (const override));
  MOCK_METHOD(base::Time, GetExplicitPassphraseTime, (), (const override));
  MOCK_METHOD(std::optional<PassphraseType>,
              GetPassphraseType,
              (),
              (const override));
  MOCK_METHOD(void, SetEncryptionPassphrase, (const std::string&), (override));
  MOCK_METHOD(bool, SetDecryptionPassphrase, (const std::string&), (override));
  MOCK_METHOD(void,
              SetExplicitPassphraseDecryptionNigoriKey,
              (std::unique_ptr<Nigori>),
              (override));
  MOCK_METHOD(std::unique_ptr<Nigori>,
              GetExplicitPassphraseDecryptionNigoriKey,
              (),
              (const override));
};

}  // namespace syncer

#endif  // COMPONENTS_SYNC_TEST_SYNC_USER_SETTINGS_MOCK_H_
