// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_SERVICE_SYNC_PREFS_H_
#define COMPONENTS_SYNC_SERVICE_SYNC_PREFS_H_

#include <stdint.h>

#include <memory>
#include <string>
#include <vector>

#include "base/compiler_specific.h"
#include "base/memory/raw_ptr.h"
#include "base/observer_list.h"
#include "base/sequence_checker.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "components/prefs/pref_member.h"
#include "components/sync/base/passphrase_enums.h"
#include "components/sync/base/user_selectable_type.h"

class PrefRegistrySimple;
class PrefService;
class PrefValueMap;

namespace signin {
class GaiaIdHash;
}  // namespace signin

namespace syncer {

class SyncPrefObserver {
 public:
  virtual void OnSyncManagedPrefChange(bool is_sync_managed) = 0;
  virtual void OnFirstSetupCompletePrefChange(
      bool is_initial_sync_feature_setup_complete) = 0;
  virtual void OnPreferredDataTypesPrefChange(
      bool payments_integration_enabled_changed) = 0;

 protected:
  virtual ~SyncPrefObserver();
};

// SyncPrefs is a helper class that manages getting, setting, and persisting
// global sync preferences. It is not thread-safe, and lives on the UI thread.
class SyncPrefs {
 public:
  enum class SyncAccountState {
    kNotSignedIn = 0,
    // In transport mode.
    kSignedInNotSyncing = 1,
    kSyncing = 2
  };

  // |pref_service| must not be null and must outlive this object.
  explicit SyncPrefs(PrefService* pref_service);

  SyncPrefs(const SyncPrefs&) = delete;
  SyncPrefs& operator=(const SyncPrefs&) = delete;

  ~SyncPrefs();

  static void RegisterProfilePrefs(PrefRegistrySimple* registry);

  void AddSyncPrefObserver(SyncPrefObserver* sync_pref_observer);
  void RemoveSyncPrefObserver(SyncPrefObserver* sync_pref_observer);

  // Getters and setters for global sync prefs.

  // First-Setup-Complete is conceptually similar to the user's consent to
  // enable sync-the-feature.
  bool IsInitialSyncFeatureSetupComplete() const;
  void SetInitialSyncFeatureSetupComplete();
  void ClearInitialSyncFeatureSetupComplete();

  // Whether the user wants Sync to run. This is false by default, but gets set
  // to true early in the Sync setup flow, after the user has pressed "turn on
  // Sync" but before they have actually confirmed the settings (that's
  // IsInitialSyncFeatureSetupComplete()). After Sync is enabled, this can get
  // set to false via signout (which also clears
  // IsInitialSyncFeatureSetupComplete) or, on ChromeOS Ash, when Sync gets
  // reset from the dashboard.
  bool IsSyncRequested() const;
  void SetSyncRequested(bool is_requested);
  bool IsSyncRequestedSetExplicitly() const;

  // Whether the "Sync everything" toggle is enabled. This flag only has an
  // effect if Sync-the-feature is enabled. Note that even if this is true, some
  // types may be disabled e.g. due to enterprise policy.
  bool HasKeepEverythingSynced() const;

  // Returns the set of types that the user has selected to be synced.
  // If Sync-the-feature is enabled, this takes HasKeepEverythingSynced() into
  // account (i.e. returns "all types").
  // If some types are force-disabled by policy, they will not be included.
  // Note: this is used for syncing users, and for signed-in not syncing users
  // when the kReplaceSyncPromosWithSignInPromos is disabled.
  UserSelectableTypeSet GetSelectedTypes(SyncAccountState account_state) const;
  // Returns the set of types for the given gaia_id_hash for sign-in users.
  // If some types are force-disabled by policy, they will not be included.
  // Note: this is used for signed-in not syncing users when the
  // kReplaceSyncPromosWithSignInPromos is enabled only.
  UserSelectableTypeSet GetSelectedTypesForAccount(
      const signin::GaiaIdHash& gaia_id_hash) const;

  // Returns whether `type` is "managed" i.e. controlled by enterprise policy.
  bool IsTypeManagedByPolicy(UserSelectableType type) const;

  // Returns whether `type` is "managed" i.e. controlled by a custodian (i.e.
  // parent/guardian of a child account).
  bool IsTypeManagedByCustodian(UserSelectableType type) const;

  // Sets the selection state for all |registered_types| and "keep everything
  // synced" flag.
  // |keep_everything_synced| indicates that all current and future types
  // should be synced. If this is set to true, then GetSelectedTypes() will
  // always return UserSelectableTypeSet::All(), even if not all of them are
  // registered or individually marked as selected.
  // Changes are still made to the individual selectable type prefs even if
  // |keep_everything_synced| is true, but won't be visible until it's set to
  // false.
  // Note: this is used for syncing users, and for signed-in not syncing
  // users when the kReplaceSyncPromosWithSignInPromos is disabled.
  void SetSelectedTypes(bool keep_everything_synced,
                        UserSelectableTypeSet registered_types,
                        UserSelectableTypeSet selected_types);
  // Used to set user's selected types prefs in Sync-the-transport mode.
  // Note: this is used for signed-in not syncing users when the
  // kReplaceSyncPromosWithSignInPromos is enabled only.
  void SetSelectedTypeForAccount(UserSelectableType type,
                                 bool is_type_on,
                                 const signin::GaiaIdHash& gaia_id_hash);
  // Used to clear per account prefs for all users *except* the ones in the
  // passed-in |available_gaia_ids|.
  void KeepAccountSettingsPrefsOnlyForUsers(
      const std::vector<signin::GaiaIdHash>& available_gaia_ids);

#if BUILDFLAG(IS_IOS)
  // Sets the opt-in for bookmarks & reading list in transport mode.
  // Note that this only has an effect if `kEnableBookmarksAccountStorage`
  // and/or `kReadingListEnableDualReadingListModel` are enabled, but
  // `kReplaceSyncPromosWithSignInPromos` is NOT enabled. (It should still be
  // called if `kReplaceSyncPromosWithSignInPromos` is enabled though, to better
  // support rollbacks.)
  void SetBookmarksAndReadingListAccountStorageOptIn(bool value);

  // Gets the opt-in state for bookmarks & reading list in transport mode, for
  // testing. Production code should use `GetSelectedTypes()`
  // instead which already takes this into account.
  bool IsOptedInForBookmarksAndReadingListAccountStorageForTesting();

  // Clears the opt-in for bookmarks & reading list in transport mode.
  void ClearBookmarksAndReadingListAccountStorageOptIn();
#endif  // BUILDFLAG(IS_IOS)

#if BUILDFLAG(IS_CHROMEOS_ASH)
  // Chrome OS provides a separate settings UI surface for sync of OS types,
  // including a separate "Sync All" toggle for OS types.
  bool IsSyncAllOsTypesEnabled() const;
  UserSelectableOsTypeSet GetSelectedOsTypes() const;
  bool IsOsTypeManagedByPolicy(UserSelectableOsType type) const;
  void SetSelectedOsTypes(bool sync_all_os_types,
                          UserSelectableOsTypeSet registered_types,
                          UserSelectableOsTypeSet selected_types);

  // Maps |type| to its corresponding preference name.
  static const char* GetPrefNameForOsTypeForTesting(UserSelectableOsType type);

  // Sets |type| as disabled in the given |policy_prefs|, which should
  // correspond to the "managed" (aka policy-controlled) pref store.
  static void SetOsTypeDisabledByPolicy(PrefValueMap* policy_prefs,
                                        UserSelectableOsType type);
#endif

#if BUILDFLAG(IS_CHROMEOS_LACROS)
  bool IsAppsSyncEnabledByOs() const;
  void SetAppsSyncEnabledByOs(bool apps_sync_enabled);
#endif

  // Whether Sync is disabled on the client for all profiles and accounts.
  bool IsSyncClientDisabledByPolicy() const;

  // Maps |type| to its corresponding preference name.
  static const char* GetPrefNameForTypeForTesting(UserSelectableType type);

  // Sets |type| as disabled in the given |policy_prefs|, which should
  // correspond to the "managed" (aka policy-controlled) pref store.
  static void SetTypeDisabledByPolicy(PrefValueMap* policy_prefs,
                                      UserSelectableType type);

  // Sets |type| as disabled in the given |supervised_user_prefs|, which should
  // correspond to the custodian-controlled pref store (i.e. controlled by
  // parent/guardian of a child account).
  static void SetTypeDisabledByCustodian(PrefValueMap* supervised_user_prefs,
                                         UserSelectableType type);

  // Gets the local sync backend enabled state.
  bool IsLocalSyncEnabled() const;

  // The user's passphrase type, determined the first time the engine is
  // successfully initialized.
  absl::optional<PassphraseType> GetCachedPassphraseType() const;
  void SetCachedPassphraseType(PassphraseType passphrase_type);
  void ClearCachedPassphraseType();

  // The encryption bootstrap token is used for explicit passphrase users
  // (usually custom passphrase) and represents a user-entered passphrase.
  std::string GetEncryptionBootstrapToken() const;
  void SetEncryptionBootstrapToken(const std::string& token);
  void ClearEncryptionBootstrapToken();

  // Muting mechanism for passphrase prompts, used on Android.
  int GetPassphrasePromptMutedProductVersion() const;
  void SetPassphrasePromptMutedProductVersion(int major_version);
  void ClearPassphrasePromptMutedProductVersion();

  // Migrates any user settings for pre-existing signed-in users, for the
  // feature `kReplaceSyncPromosWithSignInPromos`. For signed-out users or
  // syncing users, no migration is necessary - this also covers new users (or
  // more precisely, new profiles).
  // This should be called early during browser startup.
  void MaybeMigratePrefsForSyncToSigninPart1(SyncAccountState account_state,
                                             signin::GaiaIdHash gaia_id_hash);

  // Second part of the above migration, which depends on the user's passphrase
  // type, which isn't known yet during browser startup. This should be called
  // as soon as the passphrase type is known, and will only do any migration if
  // the above method has flagged that it's necessary.
  void MaybeMigratePrefsForSyncToSigninPart2(signin::GaiaIdHash gaia_id_hash,
                                             bool is_using_explicit_passphrase);

  // Should be called when Sync gets disabled / the user signs out. Clears any
  // temporary state from the above migration.
  void MarkPartialSyncToSigninMigrationFullyDone();

  static void MigrateAutofillWalletImportEnabledPref(PrefService* pref_service);

 private:
  static void RegisterTypeSelectedPref(PrefRegistrySimple* prefs,
                                       UserSelectableType type);

  static const char* GetPrefNameForType(UserSelectableType type);
#if BUILDFLAG(IS_CHROMEOS_ASH)
  static const char* GetPrefNameForOsType(UserSelectableOsType type);
#endif

  static bool IsTypeSupportedInTransportMode(UserSelectableType type);

  void OnSyncManagedPrefChanged();
  void OnFirstSetupCompletePrefChange();

  // Never null.
  const raw_ptr<PrefService> pref_service_;

  base::ObserverList<SyncPrefObserver>::Unchecked sync_pref_observers_;

  // The preference that controls whether sync is under control by
  // configuration management.
  BooleanPrefMember pref_sync_managed_;

  BooleanPrefMember pref_initial_sync_feature_setup_complete_;

  bool local_sync_enabled_;

  SEQUENCE_CHECKER(sequence_checker_);
};

}  // namespace syncer

#endif  // COMPONENTS_SYNC_SERVICE_SYNC_PREFS_H_
