// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_BASE_SYNC_PREFS_H_
#define COMPONENTS_SYNC_BASE_SYNC_PREFS_H_

#include <stdint.h>

#include <memory>
#include <string>

#include "base/compiler_specific.h"
#include "base/memory/raw_ptr.h"
#include "base/observer_list.h"
#include "base/sequence_checker.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "components/prefs/pref_member.h"
#include "components/sync/base/user_selectable_type.h"

class PrefRegistrySimple;
class PrefService;

namespace syncer {

class SyncPrefObserver {
 public:
  virtual void OnSyncManagedPrefChange(bool is_sync_managed) = 0;
  virtual void OnFirstSetupCompletePrefChange(bool is_first_setup_complete) = 0;
  virtual void OnSyncRequestedPrefChange(bool is_sync_requested) = 0;
  virtual void OnPreferredDataTypesPrefChange() = 0;

 protected:
  virtual ~SyncPrefObserver();
};

// SyncPrefs is a helper class that manages getting, setting, and persisting
// global sync preferences. It is not thread-safe, and lives on the UI thread.
class SyncPrefs {
 public:
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
  bool IsFirstSetupComplete() const;
  void SetFirstSetupComplete();
  void ClearFirstSetupComplete();

  bool IsSyncRequested() const;
  void SetSyncRequested(bool is_requested);
  void SetSyncRequestedIfNotSetExplicitly();

  bool HasKeepEverythingSynced() const;

  // Returns UserSelectableTypeSet::All() if HasKeepEverythingSynced() is true.
  UserSelectableTypeSet GetSelectedTypes() const;

  // Sets the selection state for all |registered_types| and "keep everything
  // synced" flag.
  // |keep_everything_synced| indicates that all current and future types
  // should be synced. If this is set to true, then GetSelectedTypes() will
  // always return UserSelectableTypeSet::All(), even if not all of them are
  // registered or individually marked as selected.
  // Changes are still made to the individual selectable type prefs even if
  // |keep_everything_synced| is true, but won't be visible until it's set to
  // false.
  void SetSelectedTypes(bool keep_everything_synced,
                        UserSelectableTypeSet registered_types,
                        UserSelectableTypeSet selected_types);

#if BUILDFLAG(IS_CHROMEOS_ASH)
  // Chrome OS provides a separate settings UI surface for sync of OS types,
  // including a separate "Sync All" toggle for OS types.
  bool IsSyncAllOsTypesEnabled() const;
  UserSelectableOsTypeSet GetSelectedOsTypes() const;
  void SetSelectedOsTypes(bool sync_all_os_types,
                          UserSelectableOsTypeSet registered_types,
                          UserSelectableOsTypeSet selected_types);

  // Maps |type| to its corresponding preference name. Returns nullptr if |type|
  // isn't an OS type.
  static const char* GetPrefNameForOsType(UserSelectableOsType type);
#endif

#if BUILDFLAG(IS_CHROMEOS_LACROS)
  bool IsAppsSyncEnabledByOs() const;
  void SetAppsSyncEnabledByOs(bool apps_sync_enabled);
#endif

  // Whether Sync is disabled on the client for all profiles and accounts.
  bool IsSyncClientDisabledByPolicy() const;

  // Maps |type| to its corresponding preference name.
  static const char* GetPrefNameForType(UserSelectableType type);

  // Gets the local sync backend enabled state.
  bool IsLocalSyncEnabled() const;

  // The encryption bootstrap token is used for explicit passphrase users
  // (usually custom passphrase) and represents a user-entered passphrase.
  std::string GetEncryptionBootstrapToken() const;
  void SetEncryptionBootstrapToken(const std::string& token);
  void ClearEncryptionBootstrapToken();

  // Muting mechanism for passphrase prompts, used on Android.
  int GetPassphrasePromptMutedProductVersion() const;
  void SetPassphrasePromptMutedProductVersion(int major_version);
  void ClearPassphrasePromptMutedProductVersion();

 private:
  static void RegisterTypeSelectedPref(PrefRegistrySimple* prefs,
                                       UserSelectableType type);

  void OnSyncManagedPrefChanged();
  void OnFirstSetupCompletePrefChange();
  void OnSyncRequestedPrefChange();

  // Never null.
  const raw_ptr<PrefService> pref_service_;

  base::ObserverList<SyncPrefObserver>::Unchecked sync_pref_observers_;

  // The preference that controls whether sync is under control by
  // configuration management.
  BooleanPrefMember pref_sync_managed_;

  BooleanPrefMember pref_first_setup_complete_;

  BooleanPrefMember pref_sync_requested_;

  bool local_sync_enabled_;

  SEQUENCE_CHECKER(sequence_checker_);
};

#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_IOS)
void MigrateSyncRequestedPrefPostMice(PrefService* pref_service);
#endif  // BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_IOS)

}  // namespace syncer

#endif  // COMPONENTS_SYNC_BASE_SYNC_PREFS_H_
