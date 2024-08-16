// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_PREFERENCES_PREF_SERVICE_SYNCABLE_H_
#define COMPONENTS_SYNC_PREFERENCES_PREF_SERVICE_SYNCABLE_H_

#include <stdint.h>

#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include "base/functional/callback_forward.h"
#include "base/functional/callback_helpers.h"
#include "base/observer_list.h"
#include "build/chromeos_buildflags.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_service.h"
#include "components/sync_preferences/pref_model_associator.h"

class PrefValueStore;

namespace syncer {
class SyncableService;
class SyncService;
}

namespace sync_preferences {

class DualLayerUserPrefStore;
class PrefModelAssociatorClient;
class PrefServiceSyncableObserver;
class SyncedPrefObserver;

// A PrefService that can be synced. Users are forced to declare
// whether preferences are syncable or not when registering them to
// this PrefService.
class PrefServiceSyncable : public PrefService,
                            public PrefServiceForAssociator {
 public:
  // You may wish to use PrefServiceFactory or one of its subclasses
  // for simplified construction.
  PrefServiceSyncable(
      std::unique_ptr<PrefNotifierImpl> pref_notifier,
      std::unique_ptr<PrefValueStore> pref_value_store,
      scoped_refptr<PersistentPrefStore> user_prefs,
      scoped_refptr<PersistentPrefStore> standalone_browser_prefs,
      scoped_refptr<user_prefs::PrefRegistrySyncable> pref_registry,
      scoped_refptr<PrefModelAssociatorClient> pref_model_associator_client,
      base::RepeatingCallback<void(PersistentPrefStore::PrefReadError)>
          read_error_callback,
      bool async);

  // Note: This must be called only if EnablePreferencesAccountStorage feature
  // is enabled. However, it is possible that the other overload gets called
  // even if EnablePreferencesAccountStorage is enabled during test when using
  // TestingPrefServiceSyncable.
  // TODO(crbug.com/40283048): Fix TestingPrefServiceSyncable or remove usages.
  // Note: Can be done using templates instead of overload but chosen not to for
  // more clarity.
  PrefServiceSyncable(
      std::unique_ptr<PrefNotifierImpl> pref_notifier,
      std::unique_ptr<PrefValueStore> pref_value_store,
      scoped_refptr<DualLayerUserPrefStore> dual_layer_user_prefs,
      scoped_refptr<PersistentPrefStore> standalone_browser_prefs,
      scoped_refptr<user_prefs::PrefRegistrySyncable> pref_registry,
      scoped_refptr<PrefModelAssociatorClient> pref_model_associator_client,
      base::RepeatingCallback<void(PersistentPrefStore::PrefReadError)>
          read_error_callback,
      bool async);

  PrefServiceSyncable(const PrefServiceSyncable&) = delete;
  PrefServiceSyncable& operator=(const PrefServiceSyncable&) = delete;

  ~PrefServiceSyncable() override;

  // Creates an incognito copy of the pref service that shares most pref stores
  // but uses a fresh non-persistent overlay for the user pref store and an
  // individual extension pref store (to cache the effective extension prefs for
  // incognito windows). |persistent_pref_names| is a list of preference names
  // whose changes will be persisted by the returned incognito pref service.
  std::unique_ptr<PrefServiceSyncable> CreateIncognitoPrefService(
      PrefStore* incognito_extension_pref_store,
      const std::vector<const char*>& persistent_pref_names);

  // Returns true if preferences state has synchronized with the remote
  // preferences. If true is returned it can be assumed the local preferences
  // has applied changes from the remote preferences. The two may not be
  // identical if a change is in flight (from either side).
  bool IsSyncing();

  // Returns true if priority preferences state has synchronized with the remote
  // priority preferences.
  bool IsPrioritySyncing();

#if BUILDFLAG(IS_CHROMEOS_ASH)
  // As above, but for OS preferences.
  bool AreOsPrefsSyncing();

  // As above, but for OS priority preferences.
  bool AreOsPriorityPrefsSyncing();
#endif

  void AddObserver(PrefServiceSyncableObserver* observer);
  void RemoveObserver(PrefServiceSyncableObserver* observer);

  syncer::SyncableService* GetSyncableService(const syncer::DataType& type);

  // Do not call this after having derived an incognito or per tab pref service.
  void UpdateCommandLinePrefStore(PrefStore* cmd_line_store) override;

  void AddSyncedPrefObserver(const std::string& name,
                             SyncedPrefObserver* observer);
  void RemoveSyncedPrefObserver(const std::string& name,
                                SyncedPrefObserver* observer);

  void OnSyncServiceInitialized(syncer::SyncService* sync_service);

 private:
  class DemographicsPrefsClearer;

  void ConnectAssociatorsAndRegisterPreferences();

  void AddRegisteredSyncablePreference(std::string_view path, uint32_t flags);

  // PrefServiceForAssociator:
  base::Value::Type GetRegisteredPrefType(
      std::string_view pref_name) const override;
  void OnIsSyncingChanged() override;
  uint32_t GetWriteFlags(std::string_view pref_name) const override;

  // Whether CreateIncognitoPrefService() has been called to create a
  // "forked" PrefService.
  bool pref_service_forked_ = false;

  PrefModelAssociator pref_sync_associator_;
  PrefModelAssociator priority_pref_sync_associator_;

#if BUILDFLAG(IS_CHROMEOS_ASH)
  // Associators for Chrome OS system preferences.
  PrefModelAssociator os_pref_sync_associator_;
  PrefModelAssociator os_priority_pref_sync_associator_;
#endif

  const scoped_refptr<user_prefs::PrefRegistrySyncable> pref_registry_;

  base::ObserverList<PrefServiceSyncableObserver>::Unchecked observer_list_;

  // DualLayerUserPrefStore instance passed to the associators. This is non-null
  // iff EnablePreferencesAccountStorage feature is enabled.
  scoped_refptr<DualLayerUserPrefStore> dual_layer_user_prefs_;

  std::unique_ptr<DemographicsPrefsClearer> demographics_prefs_clearer_;
};

}  // namespace sync_preferences

#endif  // COMPONENTS_SYNC_PREFERENCES_PREF_SERVICE_SYNCABLE_H_
