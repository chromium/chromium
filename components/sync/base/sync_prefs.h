// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_BASE_SYNC_PREFS_H_
#define COMPONENTS_SYNC_BASE_SYNC_PREFS_H_

#include <stdint.h>

#include <map>
#include <memory>
#include <string>

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/sequence_checker.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "components/prefs/pref_member.h"
#include "components/sync/base/model_type.h"
#include "components/sync/base/user_demographics.h"
#include "components/sync/base/user_selectable_type.h"
#include "components/sync/protocol/sync.pb.h"
#include "third_party/metrics_proto/user_demographics.pb.h"

class PrefService;

namespace user_prefs {
class PrefRegistrySyncable;
}

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

// Use this for crypto/passphrase-related parts of sync prefs.
class CryptoSyncPrefs {
 public:
  virtual ~CryptoSyncPrefs();

  // Use this encryption bootstrap token if we're using an explicit passphrase.
  virtual std::string GetEncryptionBootstrapToken() const = 0;
  virtual void SetEncryptionBootstrapToken(const std::string& token) = 0;

  // Use this keystore bootstrap token if we're not using an explicit
  // passphrase.
  virtual std::string GetKeystoreEncryptionBootstrapToken() const = 0;
  virtual void SetKeystoreEncryptionBootstrapToken(
      const std::string& token) = 0;
};

// SyncPrefs is a helper class that manages getting, setting, and persisting
// global sync preferences. It is not thread-safe, and lives on the UI thread.
class SyncPrefs : public CryptoSyncPrefs,
                  public base::SupportsWeakPtr<SyncPrefs> {
 public:
  // |pref_service| must not be null and must outlive this object.
  explicit SyncPrefs(PrefService* pref_service);
  ~SyncPrefs() override;

  static void RegisterProfilePrefs(user_prefs::PrefRegistrySyncable* registry);

  void AddSyncPrefObserver(SyncPrefObserver* sync_pref_observer);
  void RemoveSyncPrefObserver(SyncPrefObserver* sync_pref_observer);

  // Clears "bookkeeping" sync preferences, such as the last synced time,
  // whether the last shutdown was clean, etc. Does *not* clear sync preferences
  // which are directly user-controlled, such as the set of selected types.
  void ClearPreferences();

  // Clears only the subset of preferences that are redundant with the sync
  // directory and used only for verifying consistency with prefs.
  // TODO(crbug.com/923285): Remove this function and instead rely solely on
  // ClearPreferences() once investigations are finalized are we understand the
  // source of discrepancies for UMA Sync.DirectoryVsPrefsConsistency.
  void ClearDirectoryConsistencyPreferences();

  // Getters and setters for global sync prefs.

  bool IsFirstSetupComplete() const;
  void SetFirstSetupComplete();

  bool IsSyncRequested() const;
  void SetSyncRequested(bool is_requested);
  void SetSyncRequestedIfNotSetExplicitly();

  base::Time GetLastSyncedTime() const;
  void SetLastSyncedTime(base::Time time);

  base::Time GetLastPollTime() const;
  void SetLastPollTime(base::Time time);

  base::TimeDelta GetPollInterval() const;
  void SetPollInterval(base::TimeDelta interval);

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

#if defined(OS_CHROMEOS)
  // Chrome OS provides a separate settings UI surface for sync of OS types,
  // including a separate "Sync All" toggle for OS types.
  bool IsSyncAllOsTypesEnabled() const;
  UserSelectableOsTypeSet GetSelectedOsTypes() const;
  void SetSelectedOsTypes(bool sync_all_os_types,
                          UserSelectableOsTypeSet registered_types,
                          UserSelectableOsTypeSet selected_types);
  bool GetOsSyncFeatureEnabled() const;
  void SetOsSyncFeatureEnabled(bool enabled);
#endif

  // Whether Sync is forced off by enterprise policy. Note that this only covers
  // one out of two types of policy, "browser" policy. The second kind, "cloud"
  // policy, is handled directly in ProfileSyncService.
  bool IsManaged() const;

  // Use this encryption bootstrap token if we're using an explicit passphrase.
  std::string GetEncryptionBootstrapToken() const override;
  void SetEncryptionBootstrapToken(const std::string& token) override;

  // Use this keystore bootstrap token if we're not using an explicit
  // passphrase.
  std::string GetKeystoreEncryptionBootstrapToken() const override;
  void SetKeystoreEncryptionBootstrapToken(const std::string& token) override;

  // Maps |type| to its corresponding preference name.
  static const char* GetPrefNameForTypeForTesting(UserSelectableType type);

  // Copy of various fields historically owned and persisted by the Directory.
  // This is a future-proof approach to ultimately replace the Directory once
  // most users have populated prefs and the Directory is about to be removed.
  // TODO(crbug.com/923287): Figure out if this is an appropriate place.
  void SetCacheGuid(const std::string& cache_guid);
  std::string GetCacheGuid() const;
  void SetBirthday(const std::string& birthday);
  std::string GetBirthday() const;
  void SetBagOfChips(const std::string& bag_of_chips);
  std::string GetBagOfChips() const;

  // Out of band sync passphrase prompt getter/setter.
  bool IsPassphrasePrompted() const;
  void SetPassphrasePrompted(bool value);

  // For testing.
  void SetManagedForTest(bool is_managed);

  // Get/set for the last known sync invalidation versions.
  void GetInvalidationVersions(
      std::map<ModelType, int64_t>* invalidation_versions) const;
  void UpdateInvalidationVersions(
      const std::map<ModelType, int64_t>& invalidation_versions);

  // Will return the contents of the LastRunVersion preference. This may be an
  // empty string if no version info was present, and is only valid at
  // Sync startup time (after which the LastRunVersion preference will have been
  // updated to the current version).
  std::string GetLastRunVersion() const;
  void SetLastRunVersion(const std::string& current_version);

  // Gets the local sync backend enabled state.
  bool IsLocalSyncEnabled() const;

  // Gets the synced user’s birth year and gender from synced prefs and adds
  // noise to the birth year, see doc of UserDemographicsStatus in
  // components/sync/base/user_demographics.h for more details. You need to
  // provide an accurate |now| time that represents the current time.
  UserDemographicsResult GetUserNoisedBirthYearAndGender(base::Time now);

 private:
  static void RegisterTypeSelectedPref(user_prefs::PrefRegistrySyncable* prefs,
                                       UserSelectableType type);

  void OnSyncManagedPrefChanged();
  void OnFirstSetupCompletePrefChange();
  void OnSyncRequestedPrefChange();

  // Never null.
  PrefService* const pref_service_;

  base::ObserverList<SyncPrefObserver>::Unchecked sync_pref_observers_;

  // The preference that controls whether sync is under control by
  // configuration management.
  BooleanPrefMember pref_sync_managed_;

  BooleanPrefMember pref_first_setup_complete_;

  BooleanPrefMember pref_sync_requested_;

  bool local_sync_enabled_;

  SEQUENCE_CHECKER(sequence_checker_);

  DISALLOW_COPY_AND_ASSIGN(SyncPrefs);
};

void MigrateSessionsToProxyTabsPrefs(PrefService* pref_service);
void ClearObsoleteUserTypePrefs(PrefService* pref_service);
void ClearObsoleteClearServerDataPrefs(PrefService* pref_service);
void ClearObsoleteAuthErrorPrefs(PrefService* pref_service);
void ClearObsoleteFirstSyncTime(PrefService* pref_service);
void ClearObsoleteSyncLongPollIntervalSeconds(PrefService* pref_service);
#if defined(OS_CHROMEOS)
void ClearObsoleteSyncSpareBootstrapToken(PrefService* pref_service);
#endif  // defined(OS_CHROMEOS)
void MigrateSyncSuppressedPref(PrefService* pref_service);
void ClearObsoleteMemoryPressurePrefs(PrefService* pref_service);

}  // namespace syncer

#endif  // COMPONENTS_SYNC_BASE_SYNC_PREFS_H_
