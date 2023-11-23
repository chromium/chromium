// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_SERVICE_SYNC_USER_SETTINGS_IMPL_H_
#define COMPONENTS_SYNC_SERVICE_SYNC_USER_SETTINGS_IMPL_H_

#include <memory>
#include <string>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "components/sync/base/model_type.h"
#include "components/sync/base/user_selectable_type.h"
#include "components/sync/service/sync_prefs.h"
#include "components/sync/service/sync_user_settings.h"

namespace syncer {

class SyncServiceCrypto;

class SyncUserSettingsImpl : public SyncUserSettings {
 public:
  class Delegate {
   public:
    Delegate() = default;
    virtual ~Delegate() = default;

    virtual bool IsCustomPassphraseAllowed() const = 0;
    virtual SyncPrefs::SyncAccountState GetSyncAccountStateForPrefs() const = 0;
    virtual CoreAccountInfo GetSyncAccountInfoForPrefs() const = 0;
  };

  // `delegate`, `crypto` and `prefs` must not be null and must outlive this
  // object.
  SyncUserSettingsImpl(Delegate* delegate,
                       SyncServiceCrypto* crypto,
                       SyncPrefs* prefs,
                       ModelTypeSet registered_types);
  ~SyncUserSettingsImpl() override;

  ModelTypeSet GetPreferredDataTypes() const;
  bool IsEncryptedDatatypeEnabled() const;

#if BUILDFLAG(IS_CHROMEOS_ASH)
  void SetSyncFeatureDisabledViaDashboard();
  void ClearSyncFeatureDisabledViaDashboard();
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

  // SyncUserSettings implementation.
  bool IsInitialSyncFeatureSetupComplete() const override;
#if !BUILDFLAG(IS_CHROMEOS_ASH)
  void SetInitialSyncFeatureSetupComplete(
      SyncFirstSetupCompleteSource source) override;
#endif  // !BUILDFLAG(IS_CHROMEOS_ASH)
  bool IsSyncEverythingEnabled() const override;
  UserSelectableTypeSet GetSelectedTypes() const override;
  bool IsTypeManagedByPolicy(UserSelectableType type) const override;
  bool IsTypeManagedByCustodian(UserSelectableType type) const override;
#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)
  int GetNumberOfAccountsWithPasswordsSelected() const override;
#endif
  void SetSelectedTypes(bool sync_everything,
                        UserSelectableTypeSet types) override;
  void SetSelectedType(UserSelectableType type, bool is_type_on) override;
  void KeepAccountSettingsPrefsOnlyForUsers(
      const std::vector<signin::GaiaIdHash>& available_gaia_ids) override;
#if BUILDFLAG(IS_IOS)
  void SetBookmarksAndReadingListAccountStorageOptIn(bool value) override;
#endif  // BUILDFLAG(IS_IOS)
  UserSelectableTypeSet GetRegisteredSelectableTypes() const override;
#if BUILDFLAG(IS_CHROMEOS_ASH)
  bool IsSyncFeatureDisabledViaDashboard() const override;
  bool IsSyncAllOsTypesEnabled() const override;
  UserSelectableOsTypeSet GetSelectedOsTypes() const override;
  bool IsOsTypeManagedByPolicy(UserSelectableOsType type) const override;
  void SetSelectedOsTypes(bool sync_all_os_types,
                          UserSelectableOsTypeSet types) override;
  UserSelectableOsTypeSet GetRegisteredSelectableOsTypes() const override;
#endif
#if BUILDFLAG(IS_CHROMEOS_LACROS)
  void SetAppsSyncEnabledByOs(bool apps_sync_enabled) override;
#endif
  bool IsCustomPassphraseAllowed() const override;
  bool IsEncryptEverythingEnabled() const override;
  ModelTypeSet GetEncryptedDataTypes() const override;
  bool IsPassphraseRequired() const override;
  bool IsPassphraseRequiredForPreferredDataTypes() const override;
  bool IsPassphrasePromptMutedForCurrentProductVersion() const override;
  void MarkPassphrasePromptMutedForCurrentProductVersion() override;
  bool IsTrustedVaultKeyRequired() const override;
  bool IsTrustedVaultKeyRequiredForPreferredDataTypes() const override;
  bool IsTrustedVaultRecoverabilityDegraded() const override;
  bool IsUsingExplicitPassphrase() const override;
  base::Time GetExplicitPassphraseTime() const override;
  absl::optional<PassphraseType> GetPassphraseType() const override;
  void SetEncryptionPassphrase(const std::string& passphrase) override;
  bool SetDecryptionPassphrase(const std::string& passphrase) override;
  void SetDecryptionNigoriKey(std::unique_ptr<Nigori> nigori) override;
  std::unique_ptr<Nigori> GetDecryptionNigoriKey() const override;

 private:
  bool ShouldUsePerAccountPrefs() const;

  const raw_ptr<Delegate> delegate_;
  const raw_ptr<SyncServiceCrypto> crypto_;
  const raw_ptr<SyncPrefs> prefs_;
  const ModelTypeSet registered_model_types_;
};

}  // namespace syncer

#endif  // COMPONENTS_SYNC_SERVICE_SYNC_USER_SETTINGS_IMPL_H_
