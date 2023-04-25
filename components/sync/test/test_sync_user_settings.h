// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_TEST_TEST_SYNC_USER_SETTINGS_H_
#define COMPONENTS_SYNC_TEST_TEST_SYNC_USER_SETTINGS_H_

#include <memory>
#include <string>

#include "base/memory/raw_ptr.h"
#include "build/chromeos_buildflags.h"
#include "components/sync/base/model_type.h"
#include "components/sync/base/user_selectable_type.h"
#include "components/sync/driver/sync_user_settings.h"

namespace syncer {

class TestSyncService;

// Test implementation of SyncUserSettings that mostly forwards calls to a
// TestSyncService.
class TestSyncUserSettings : public SyncUserSettings {
 public:
  explicit TestSyncUserSettings(TestSyncService* service);
  ~TestSyncUserSettings() override;

  bool IsFirstSetupComplete() const override;
  void SetFirstSetupComplete(SyncFirstSetupCompleteSource source) override;

  bool IsSyncEverythingEnabled() const override;
  UserSelectableTypeSet GetSelectedTypes() const override;
  bool IsTypeManagedByPolicy(UserSelectableType type) const override;
  void SetSelectedTypes(bool sync_everything,
                        UserSelectableTypeSet types) override;
  ModelTypeSet GetPreferredDataTypes() const;
  UserSelectableTypeSet GetRegisteredSelectableTypes() const override;

#if BUILDFLAG(IS_CHROMEOS_ASH)
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

  syncer::ModelTypeSet GetEncryptedDataTypes() const override;
  bool IsPassphraseRequired() const override;
  bool IsPassphraseRequiredForPreferredDataTypes() const override;
  bool IsPassphrasePromptMutedForCurrentProductVersion() const override;
  void MarkPassphrasePromptMutedForCurrentProductVersion() override;
  bool IsTrustedVaultKeyRequired() const override;
  bool IsTrustedVaultKeyRequiredForPreferredDataTypes() const override;
  bool IsTrustedVaultRecoverabilityDegraded() const override;
  bool IsUsingExplicitPassphrase() const override;
  base::Time GetExplicitPassphraseTime() const override;
  PassphraseType GetPassphraseType() const override;

  void SetEncryptionPassphrase(const std::string& passphrase) override;
  bool SetDecryptionPassphrase(const std::string& passphrase) override;
  void SetDecryptionNigoriKey(std::unique_ptr<Nigori> nigori) override;
  std::unique_ptr<Nigori> GetDecryptionNigoriKey() const override;

  void SetFirstSetupComplete();
  void ClearFirstSetupComplete();
  void SetCustomPassphraseAllowed(bool allowed);
  void SetPassphraseRequired(bool required);
  void SetPassphraseRequiredForPreferredDataTypes(bool required);
  void SetTrustedVaultKeyRequired(bool required);
  void SetTrustedVaultKeyRequiredForPreferredDataTypes(bool required);
  void SetTrustedVaultRecoverabilityDegraded(bool degraded);
  void SetIsUsingExplicitPassphrase(bool enabled);

 private:
  raw_ptr<TestSyncService> service_;
#if BUILDFLAG(IS_CHROMEOS_ASH)
  UserSelectableOsTypeSet selected_os_types_;
#endif
  UserSelectableTypeSet selected_types_;

  bool first_setup_complete_ = true;
  bool sync_everything_enabled_ = true;
#if BUILDFLAG(IS_CHROMEOS_ASH)
  bool sync_all_os_types_enabled_ = true;
#endif

  bool passphrase_required_ = false;
  bool passphrase_required_for_preferred_data_types_ = false;
  bool trusted_vault_key_required_ = false;
  bool trusted_vault_key_required_for_preferred_data_types_ = false;
  bool trusted_vault_recoverability_degraded_ = false;
  bool using_explicit_passphrase_ = false;
};

}  // namespace syncer

#endif  // COMPONENTS_SYNC_TEST_TEST_SYNC_USER_SETTINGS_H_
