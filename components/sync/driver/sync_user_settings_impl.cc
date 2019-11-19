// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/driver/sync_user_settings_impl.h"

#include "base/metrics/histogram_macros.h"
#include "components/sync/base/sync_prefs.h"
#include "components/sync/driver/sync_service_crypto.h"

namespace syncer {

namespace {

ModelTypeSet ResolvePreferredTypes(UserSelectableTypeSet selected_types) {
  ModelTypeSet preferred_types;
  for (UserSelectableType type : selected_types) {
    preferred_types.PutAll(UserSelectableTypeToAllModelTypes(type));
  }
  return preferred_types;
}

#if defined(OS_CHROMEOS)
ModelTypeSet ResolvePreferredOsTypes(UserSelectableOsTypeSet selected_types) {
  ModelTypeSet preferred_types;
  for (UserSelectableOsType type : selected_types) {
    preferred_types.PutAll(UserSelectableOsTypeToAllModelTypes(type));
  }
  return preferred_types;
}
#endif  // defined(OS_CHROMEOS)

}  // namespace

SyncUserSettingsImpl::SyncUserSettingsImpl(
    SyncServiceCrypto* crypto,
    SyncPrefs* prefs,
    const SyncTypePreferenceProvider* preference_provider,
    ModelTypeSet registered_model_types,
    const base::RepeatingCallback<void(bool)>& sync_allowed_by_platform_changed)
    : crypto_(crypto),
      prefs_(prefs),
      preference_provider_(preference_provider),
      registered_model_types_(registered_model_types),
      sync_allowed_by_platform_changed_cb_(sync_allowed_by_platform_changed) {
  DCHECK(crypto_);
  DCHECK(prefs_);
}

SyncUserSettingsImpl::~SyncUserSettingsImpl() = default;

bool SyncUserSettingsImpl::IsSyncRequested() const {
  return prefs_->IsSyncRequested();
}

void SyncUserSettingsImpl::SetSyncRequested(bool requested) {
  prefs_->SetSyncRequested(requested);
}

bool SyncUserSettingsImpl::IsSyncAllowedByPlatform() const {
  return sync_allowed_by_platform_;
}

void SyncUserSettingsImpl::SetSyncAllowedByPlatform(bool allowed) {
  if (sync_allowed_by_platform_ == allowed) {
    return;
  }

  sync_allowed_by_platform_ = allowed;

  sync_allowed_by_platform_changed_cb_.Run(sync_allowed_by_platform_);
}

bool SyncUserSettingsImpl::IsFirstSetupComplete() const {
  return prefs_->IsFirstSetupComplete();
}

void SyncUserSettingsImpl::SetFirstSetupComplete(
    SyncFirstSetupCompleteSource source) {
  if (IsFirstSetupComplete())
    return;
  UMA_HISTOGRAM_ENUMERATION("Signin.SyncFirstSetupCompleteSource", source);
  prefs_->SetFirstSetupComplete();
}

bool SyncUserSettingsImpl::IsSyncEverythingEnabled() const {
  return prefs_->HasKeepEverythingSynced();
}

UserSelectableTypeSet SyncUserSettingsImpl::GetSelectedTypes() const {
  UserSelectableTypeSet types = prefs_->GetSelectedTypes();
  types.PutAll(GetForcedTypes());
  types.RetainAll(GetRegisteredSelectableTypes());
  return types;
}

void SyncUserSettingsImpl::SetSelectedTypes(bool sync_everything,
                                            UserSelectableTypeSet types) {
  UserSelectableTypeSet registered_types = GetRegisteredSelectableTypes();
  DCHECK(registered_types.HasAll(types));
  prefs_->SetSelectedTypes(sync_everything, registered_types, types);
}

UserSelectableTypeSet SyncUserSettingsImpl::GetRegisteredSelectableTypes()
    const {
  UserSelectableTypeSet registered_types;
  for (UserSelectableType type : UserSelectableTypeSet::All()) {
    if (registered_model_types_.Has(
            UserSelectableTypeToCanonicalModelType(type))) {
      registered_types.Put(type);
    }
  }
  return registered_types;
}

#if defined(OS_CHROMEOS)
bool SyncUserSettingsImpl::IsSyncAllOsTypesEnabled() const {
  return prefs_->IsSyncAllOsTypesEnabled();
}

UserSelectableOsTypeSet SyncUserSettingsImpl::GetSelectedOsTypes() const {
  UserSelectableOsTypeSet types = prefs_->GetSelectedOsTypes();
  types.RetainAll(GetRegisteredSelectableOsTypes());
  return types;
}

void SyncUserSettingsImpl::SetSelectedOsTypes(bool sync_all_os_types,
                                              UserSelectableOsTypeSet types) {
  UserSelectableOsTypeSet registered_types = GetRegisteredSelectableOsTypes();
  DCHECK(registered_types.HasAll(types));
  prefs_->SetSelectedOsTypes(sync_all_os_types, registered_types, types);
}

UserSelectableOsTypeSet SyncUserSettingsImpl::GetRegisteredSelectableOsTypes()
    const {
  UserSelectableOsTypeSet registered_types;
  for (UserSelectableOsType type : UserSelectableOsTypeSet::All()) {
    if (registered_model_types_.Has(
            UserSelectableOsTypeToCanonicalModelType(type))) {
      registered_types.Put(type);
    }
  }
  return registered_types;
}

bool SyncUserSettingsImpl::GetOsSyncFeatureEnabled() const {
  return prefs_->GetOsSyncFeatureEnabled();
}

void SyncUserSettingsImpl::SetOsSyncFeatureEnabled(bool enabled) {
  prefs_->SetOsSyncFeatureEnabled(enabled);
}
#endif  // defined(OS_CHROMEOS)

UserSelectableTypeSet SyncUserSettingsImpl::GetForcedTypes() const {
  if (preference_provider_) {
    return preference_provider_->GetForcedTypes();
  }
  return UserSelectableTypeSet();
}

bool SyncUserSettingsImpl::IsEncryptEverythingAllowed() const {
  return !preference_provider_ ||
         preference_provider_->IsEncryptEverythingAllowed();
}

bool SyncUserSettingsImpl::IsEncryptEverythingEnabled() const {
  return crypto_->IsEncryptEverythingEnabled();
}

void SyncUserSettingsImpl::EnableEncryptEverything() {
  DCHECK(IsEncryptEverythingAllowed());
  crypto_->EnableEncryptEverything();
}

bool SyncUserSettingsImpl::IsPassphraseRequired() const {
  return crypto_->IsPassphraseRequired();
}

bool SyncUserSettingsImpl::IsPassphraseRequiredForPreferredDataTypes() const {
  // If there is an encrypted datatype enabled and we don't have the proper
  // passphrase, we must prompt the user for a passphrase. The only way for the
  // user to avoid entering their passphrase is to disable the encrypted types.
  return IsEncryptedDatatypeEnabled() && IsPassphraseRequired();
}

bool SyncUserSettingsImpl::IsTrustedVaultKeyRequiredForPreferredDataTypes()
    const {
  return IsEncryptedDatatypeEnabled() && crypto_->IsTrustedVaultKeyRequired();
}

bool SyncUserSettingsImpl::IsUsingSecondaryPassphrase() const {
  return crypto_->IsUsingSecondaryPassphrase();
}

base::Time SyncUserSettingsImpl::GetExplicitPassphraseTime() const {
  return crypto_->GetExplicitPassphraseTime();
}

PassphraseType SyncUserSettingsImpl::GetPassphraseType() const {
  return crypto_->GetPassphraseType();
}

void SyncUserSettingsImpl::SetEncryptionPassphrase(
    const std::string& passphrase) {
  crypto_->SetEncryptionPassphrase(passphrase);
}

bool SyncUserSettingsImpl::SetDecryptionPassphrase(
    const std::string& passphrase) {
  DCHECK(IsPassphraseRequired())
      << "SetDecryptionPassphrase must not be called when "
         "IsPassphraseRequired() is false.";

  DVLOG(1) << "Setting passphrase for decryption.";

  bool result = crypto_->SetDecryptionPassphrase(passphrase);
  UMA_HISTOGRAM_BOOLEAN("Sync.PassphraseDecryptionSucceeded", result);
  return result;
}

void SyncUserSettingsImpl::AddTrustedVaultDecryptionKeys(
    const std::string& gaia_id,
    const std::vector<std::string>& keys) {
  DVLOG(1) << "Adding trusted vault decryption keys.";
  crypto_->AddTrustedVaultDecryptionKeys(gaia_id, keys);
}

void SyncUserSettingsImpl::SetSyncRequestedIfNotSetExplicitly() {
  prefs_->SetSyncRequestedIfNotSetExplicitly();
}

ModelTypeSet SyncUserSettingsImpl::GetPreferredDataTypes() const {
  ModelTypeSet types;
#if defined(OS_CHROMEOS)
  bool sync_everything = IsSyncAllOsTypesEnabled() && IsSyncEverythingEnabled();
#else
  bool sync_everything = IsSyncEverythingEnabled();
#endif
  if (sync_everything) {
    // TODO(crbug.com/950874): it's possible to remove this case if we accept
    // behavioral change. When one of UserSelectableTypes() isn't registered,
    // but one of its corresponding UserTypes() is registered, current
    // implementation treats that corresponding type as preferred while
    // implementation without processing of this case won't treat that type
    // as preferred.
    types = registered_model_types_;
  } else {
    types = ResolvePreferredTypes(GetSelectedTypes());
    types.PutAll(AlwaysPreferredUserTypes());
#if defined(OS_CHROMEOS)
    types.PutAll(ResolvePreferredOsTypes(GetSelectedOsTypes()));
#endif
    types.RetainAll(registered_model_types_);
  }

  static_assert(40 == ModelType::NUM_ENTRIES,
                "If adding a new sync data type, update the list below below if"
                " you want to disable the new data type for local sync.");
  types.PutAll(ControlTypes());
  if (prefs_->IsLocalSyncEnabled()) {
    types.Remove(APP_LIST);
    types.Remove(SECURITY_EVENTS);
    types.Remove(SEND_TAB_TO_SELF);
    types.Remove(USER_CONSENTS);
    types.Remove(USER_EVENTS);
  }
  return types;
}

ModelTypeSet SyncUserSettingsImpl::GetEncryptedDataTypes() const {
  return crypto_->GetEncryptedDataTypes();
}

bool SyncUserSettingsImpl::IsEncryptedDatatypeEnabled() const {
  if (IsEncryptionPending())
    return true;
  const ModelTypeSet preferred_types = GetPreferredDataTypes();
  const ModelTypeSet encrypted_types = GetEncryptedDataTypes();
  DCHECK(encrypted_types.Has(PASSWORDS));
  return !Intersection(preferred_types, encrypted_types).Empty();
}

bool SyncUserSettingsImpl::IsEncryptionPending() const {
  return crypto_->encryption_pending();
}

// static
ModelTypeSet SyncUserSettingsImpl::ResolvePreferredTypesForTesting(
    UserSelectableTypeSet selected_types) {
  return ResolvePreferredTypes(selected_types);
}

}  // namespace syncer
