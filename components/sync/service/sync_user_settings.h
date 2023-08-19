// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_SERVICE_SYNC_USER_SETTINGS_H_
#define COMPONENTS_SYNC_SERVICE_SYNC_USER_SETTINGS_H_

#include <memory>
#include <string>
#include <vector>

#include "base/time/time.h"
#include "build/chromeos_buildflags.h"
#include "components/signin/public/base/gaia_id_hash.h"
#include "components/sync/base/model_type.h"
#include "components/sync/base/passphrase_enums.h"
#include "components/sync/base/user_selectable_type.h"

namespace syncer {

class Nigori;

// GENERATED_JAVA_ENUM_PACKAGE: org.chromium.chrome.browser
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class SyncFirstSetupCompleteSource {
  BASIC_FLOW = 0,
  ADVANCED_FLOW_CONFIRM = 1,
  ADVANCED_FLOW_INTERRUPTED_TURN_SYNC_ON = 2,
  ADVANCED_FLOW_INTERRUPTED_LEAVE_SYNC_OFF = 3,
  ENGINE_INITIALIZED_WITH_AUTO_START = 4,
  ANDROID_BACKUP_RESTORE = 5,
  kMaxValue = ANDROID_BACKUP_RESTORE,
};

// This class encapsulates all the user-configurable bits of Sync.
class SyncUserSettings {
 public:
  virtual ~SyncUserSettings() = default;

  // Whether the initial Sync Feature setup has been completed, meaning the
  // user has turned on Sync-the-Feature.
  // NOTE: On ChromeOS, this gets set automatically, so it doesn't really mean
  // anything.
  virtual bool IsInitialSyncFeatureSetupComplete() const = 0;
  virtual void SetInitialSyncFeatureSetupComplete(
      SyncFirstSetupCompleteSource source) = 0;

  // Getting selected types, for both Sync-the-feature and Sync-the-transport
  // users.
  virtual UserSelectableTypeSet GetSelectedTypes() const = 0;
  virtual bool IsTypeManagedByPolicy(UserSelectableType type) const = 0;
  virtual bool IsTypeManagedByCustodian(UserSelectableType type) const = 0;

  // Whether the "Sync everything" is enabled. This only has an effect if
  // Sync-the-feature is enabled. Note that even if this is true, some types may
  // be disabled e.g. due to enterprise policy.
  virtual bool IsSyncEverythingEnabled() const = 0;
  // Sets user's selected types. Should only be called if Sync-the-feature is
  // active, or in the process of being configured; otherwise use the singular
  // SetSelectedType().
  virtual void SetSelectedTypes(bool sync_everything,
                                UserSelectableTypeSet types) = 0;

  // Sets an individual type selection. For non-transport-mode cases, invoking
  // this function is only allowed while IsSyncEverythingEnabled() returns
  // false.
  virtual void SetSelectedType(UserSelectableType type, bool is_type_on) = 0;

  // Clears per account prefs for all users *except* the ones in the passed-in
  // |available_gaia_ids|.
  virtual void KeepAccountSettingsPrefsOnlyForUsers(
      const std::vector<signin::GaiaIdHash>& available_gaia_ids) = 0;

#if BUILDFLAG(IS_IOS)
  // Enables the account storage for bookmark and reading list datatype.
  virtual void SetBookmarksAndReadingListAccountStorageOptIn(bool value) = 0;
#endif  // BUILDFLAG(IS_IOS)

  // Registered user selectable types are derived from registered model types.
  // A UserSelectableType is registered if any of its ModelTypes is registered.
  virtual UserSelectableTypeSet GetRegisteredSelectableTypes() const = 0;

#if BUILDFLAG(IS_CHROMEOS_ASH)
  // As above, but for Chrome OS-specific data types. These are controlled by
  // toggles in the OS Settings UI.
  virtual bool IsSyncAllOsTypesEnabled() const = 0;
  virtual UserSelectableOsTypeSet GetSelectedOsTypes() const = 0;
  virtual bool IsOsTypeManagedByPolicy(UserSelectableOsType type) const = 0;
  virtual void SetSelectedOsTypes(bool sync_all_os_types,
                                  UserSelectableOsTypeSet types) = 0;
  virtual UserSelectableOsTypeSet GetRegisteredSelectableOsTypes() const = 0;
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

#if BUILDFLAG(IS_CHROMEOS_LACROS)
  // On Lacros, apps sync in the primary profile is controlled by the OS Sync
  // settings.
  virtual void SetAppsSyncEnabledByOs(bool apps_sync_enabled) = 0;
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)

  // Encryption state.

  // Whether the user is allowed to set a custom passphrase to encrypt all
  // their Sync data. For example, child accounts aren't allowed to do.
  virtual bool IsCustomPassphraseAllowed() const = 0;

  // Whether an explicit passphrase is in use, which means either a custom
  // passphrase or a legacy frozen implicit passphrase.
  virtual bool IsUsingExplicitPassphrase() const = 0;
  // The type of the passphrase currently in use. Returns nullopt if the state
  // isn't known, i.e. before the engine has been initialized successfully at
  // least once (in particular, it's nullopt for all signed-out users).
  virtual absl::optional<PassphraseType> GetPassphraseType() const = 0;

  // Passphrase prompt mute-state getter and setter, used on Android.
  virtual bool IsPassphrasePromptMutedForCurrentProductVersion() const = 0;
  virtual void MarkPassphrasePromptMutedForCurrentProductVersion() = 0;

  // NOTE: All of the state below may only be queried or modified if the Sync
  // engine is initialized.
  // TODO(crbug.com/1466401): Make it possible to call these APIs even without
  // the engine being initialized.

  // Whether we are currently set to encrypt all the Sync data.
  virtual bool IsEncryptEverythingEnabled() const = 0;
  // The current set of encrypted data types.
  virtual ModelTypeSet GetEncryptedDataTypes() const = 0;
  // Whether a passphrase is required for encryption or decryption to proceed.
  // Note that Sync might still be working fine if the user has disabled all
  // encrypted data types.
  virtual bool IsPassphraseRequired() const = 0;
  // Whether a passphrase is required to decrypt the data for any currently
  // enabled data type.
  virtual bool IsPassphraseRequiredForPreferredDataTypes() const = 0;
  // Whether trusted vault keys are required for encryption or decryption. Note
  // that Sync might still be working fine if the user has disabled all
  // encrypted data types.
  virtual bool IsTrustedVaultKeyRequired() const = 0;
  // Whether trusted vault keys are required for encryption or decryption to
  // proceed for currently enabled data types.
  virtual bool IsTrustedVaultKeyRequiredForPreferredDataTypes() const = 0;
  // Whether recoverability of the trusted vault keys is degraded and user
  // action is required, affecting currently enabled data types.
  virtual bool IsTrustedVaultRecoverabilityDegraded() const = 0;
  // The time the current explicit passphrase (if any) was set. If no explicit
  // passphrase is in use, or no time is available, returns an unset base::Time.
  virtual base::Time GetExplicitPassphraseTime() const = 0;

  // Asynchronously sets the passphrase to |passphrase| for encryption.
  virtual void SetEncryptionPassphrase(const std::string& passphrase) = 0;
  // Asynchronously decrypts pending keys using |passphrase|. Returns false
  // immediately if the passphrase could not be used to decrypt a locally cached
  // copy of encrypted keys; returns true otherwise. This method shouldn't be
  // called when passphrase isn't required.
  [[nodiscard]] virtual bool SetDecryptionPassphrase(
      const std::string& passphrase) = 0;

  // Asynchronously decrypts pending keys using |nigori|. |nigori| must not be
  // null. It's safe to call this method with wrong |nigori| and, unlike
  // SetDecryptionPassphrase(), when passphrase isn't required.
  virtual void SetDecryptionNigoriKey(std::unique_ptr<Nigori> nigori) = 0;
  // Returns stored decryption key, corresponding to the last successfully
  // decrypted explicit passphrase Nigori. Returns nullptr if there is no such
  // stored decryption key.
  virtual std::unique_ptr<Nigori> GetDecryptionNigoriKey() const = 0;
};

}  // namespace syncer

#endif  // COMPONENTS_SYNC_SERVICE_SYNC_USER_SETTINGS_H_
