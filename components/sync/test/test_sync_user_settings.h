// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_TEST_TEST_SYNC_USER_SETTINGS_H_
#define COMPONENTS_SYNC_TEST_TEST_SYNC_USER_SETTINGS_H_

#include <memory>
#include <string>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "build/chromeos_buildflags.h"
#include "components/signin/public/base/gaia_id_hash.h"
#include "components/sync/base/data_type.h"
#include "components/sync/base/user_selectable_type.h"
#include "components/sync/service/sync_user_settings.h"

namespace syncer {

class TestSyncService;

// Test implementation of SyncUserSettings that mostly forwards calls to a
// TestSyncService.
class TestSyncUserSettings : public SyncUserSettings {
 public:
  explicit TestSyncUserSettings(TestSyncService* service);
  ~TestSyncUserSettings() override;

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
  SyncUserSettings::UserSelectableTypePrefState GetTypePrefStateForAccount(
      UserSelectableType type) const override;
#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)
  int GetNumberOfAccountsWithPasswordsSelected() const override;
#endif
  void SetSelectedTypes(bool sync_everything,
                        UserSelectableTypeSet types) override;
  void SetSelectedType(UserSelectableType type, bool is_type_on) override;
  void KeepAccountSettingsPrefsOnlyForUsers(
      const std::vector<signin::GaiaIdHash>& available_gaia_ids) override;
  DataTypeSet GetPreferredDataTypes() const;
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

  syncer::DataTypeSet GetAllEncryptedDataTypes() const override;
  bool IsPassphraseRequired() const override;
  bool IsPassphraseRequiredForPreferredDataTypes() const override;
  bool IsPassphrasePromptMutedForCurrentProductVersion() const override;
  void MarkPassphrasePromptMutedForCurrentProductVersion() override;
  bool IsTrustedVaultKeyRequired() const override;
  bool IsTrustedVaultKeyRequiredForPreferredDataTypes() const override;
  bool IsTrustedVaultRecoverabilityDegraded() const override;
  bool IsUsingExplicitPassphrase() const override;
  base::Time GetExplicitPassphraseTime() const override;
  std::optional<PassphraseType> GetPassphraseType() const override;

  void SetEncryptionPassphrase(const std::string& passphrase) override;
  bool SetDecryptionPassphrase(const std::string& passphrase) override;
  void SetExplicitPassphraseDecryptionNigoriKey(
      std::unique_ptr<Nigori> nigori) override;
  std::unique_ptr<Nigori> GetExplicitPassphraseDecryptionNigoriKey()
      const override;

  void SetRegisteredSelectableTypes(UserSelectableTypeSet types);
  void SetInitialSyncFeatureSetupComplete();
  void ClearInitialSyncFeatureSetupComplete();
  void SetTypeIsManaged(UserSelectableType type, bool managed);
#if BUILDFLAG(IS_CHROMEOS_ASH)
  void SetOsTypeIsManaged(UserSelectableOsType type, bool managed);
#endif
  void SetCustomPassphraseAllowed(bool allowed);
  void SetPassphraseRequired();
  void SetPassphraseRequired(const std::string& required_passphrase);
  void SetTrustedVaultKeyRequired(bool required);
  void SetTrustedVaultRecoverabilityDegraded(bool degraded);
  void SetIsUsingExplicitPassphrase(bool enabled);
  void SetPassphraseType(PassphraseType type);
  void SetExplicitPassphraseTime(base::Time t);

#if BUILDFLAG(IS_CHROMEOS_ASH)
  void SetSyncFeatureDisabledViaDashboard(bool disabled_via_dashboard);
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

  const std::string& GetEncryptionPassphrase() const;

 private:
  bool IsEncryptedDatatypePreferred() const;

  const raw_ptr<TestSyncService> service_;

  UserSelectableTypeSet registered_selectable_types_ =
      UserSelectableTypeSet::All();
#if BUILDFLAG(IS_CHROMEOS_ASH)
  UserSelectableOsTypeSet selected_os_types_ = UserSelectableOsTypeSet::All();
  UserSelectableOsTypeSet managed_os_types_;
#endif
  UserSelectableTypeSet selected_types_ = UserSelectableTypeSet::All();
  UserSelectableTypeSet managed_types_;

  bool initial_sync_feature_setup_complete_ = true;
  bool sync_everything_enabled_ = true;
#if BUILDFLAG(IS_CHROMEOS_ASH)
  bool sync_all_os_types_enabled_ = true;
#endif

  bool custom_passphrase_allowed_ = true;
  bool passphrase_required_ = false;
  bool trusted_vault_key_required_ = false;
  bool trusted_vault_recoverability_degraded_ = false;
  PassphraseType passphrase_type_ = PassphraseType::kKeystorePassphrase;
  base::Time explicit_passphrase_time_;
  std::string encryption_passphrase_;

#if BUILDFLAG(IS_CHROMEOS_ASH)
  bool sync_feature_disabled_via_dashboard_ = false;
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
};

}  // namespace syncer

#endif  // COMPONENTS_SYNC_TEST_TEST_SYNC_USER_SETTINGS_H_
