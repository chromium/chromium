// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync_preferences/pref_service_syncable.h"

#include <utility>

#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/observer_list.h"
#include "build/chromeos_buildflags.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/in_memory_pref_store.h"
#include "components/prefs/overlay_user_pref_store.h"
#include "components/prefs/pref_notifier_impl.h"
#include "components/prefs/pref_registry.h"
#include "components/prefs/pref_value_store.h"
#include "components/sync/base/features.h"
#include "components/sync_preferences/dual_layer_user_pref_store.h"
#include "components/sync_preferences/pref_model_associator.h"
#include "components/sync_preferences/pref_service_syncable_observer.h"
#include "components/sync_preferences/synced_pref_observer.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "ash/constants/ash_features.h"
#endif

namespace sync_preferences {

PrefServiceSyncable::PrefServiceSyncable(
    std::unique_ptr<PrefNotifierImpl> pref_notifier,
    std::unique_ptr<PrefValueStore> pref_value_store,
    scoped_refptr<PersistentPrefStore> user_prefs,
    scoped_refptr<PersistentPrefStore> standalone_browser_prefs,
    scoped_refptr<user_prefs::PrefRegistrySyncable> pref_registry,
    const PrefModelAssociatorClient* pref_model_associator_client,
    base::RepeatingCallback<void(PersistentPrefStore::PrefReadError)>
        read_error_callback,
    bool async)
    : PrefService(std::move(pref_notifier),
                  std::move(pref_value_store),
                  user_prefs,
                  standalone_browser_prefs,
                  pref_registry,
                  std::move(read_error_callback),
                  async),
      pref_sync_associator_(pref_model_associator_client,
                            user_prefs,
                            syncer::PREFERENCES),
      priority_pref_sync_associator_(pref_model_associator_client,
                                     user_prefs,
                                     syncer::PRIORITY_PREFERENCES),
#if BUILDFLAG(IS_CHROMEOS_ASH)
      os_pref_sync_associator_(pref_model_associator_client,
                               user_prefs,
                               syncer::OS_PREFERENCES),
      os_priority_pref_sync_associator_(pref_model_associator_client,
                                        user_prefs,
                                        syncer::OS_PRIORITY_PREFERENCES),
#endif
      pref_registry_(std::move(pref_registry)) {
  ConnectAssociatorsAndRegisterPreferences();
}

PrefServiceSyncable::PrefServiceSyncable(
    std::unique_ptr<PrefNotifierImpl> pref_notifier,
    std::unique_ptr<PrefValueStore> pref_value_store,
    scoped_refptr<DualLayerUserPrefStore> dual_layer_user_prefs,
    scoped_refptr<PersistentPrefStore> standalone_browser_prefs,
    scoped_refptr<user_prefs::PrefRegistrySyncable> pref_registry,
    const PrefModelAssociatorClient* pref_model_associator_client,
    base::RepeatingCallback<void(PersistentPrefStore::PrefReadError)>
        read_error_callback,
    bool async)
    : PrefService(std::move(pref_notifier),
                  std::move(pref_value_store),
                  dual_layer_user_prefs,
                  standalone_browser_prefs,
                  pref_registry,
                  std::move(read_error_callback),
                  async),
      pref_sync_associator_(pref_model_associator_client,
                            dual_layer_user_prefs,
                            syncer::PREFERENCES),
      priority_pref_sync_associator_(pref_model_associator_client,
                                     dual_layer_user_prefs,
                                     syncer::PRIORITY_PREFERENCES),
#if BUILDFLAG(IS_CHROMEOS_ASH)
      os_pref_sync_associator_(pref_model_associator_client,
                               dual_layer_user_prefs,
                               syncer::OS_PREFERENCES),
      os_priority_pref_sync_associator_(pref_model_associator_client,
                                        dual_layer_user_prefs,
                                        syncer::OS_PRIORITY_PREFERENCES),
#endif
      pref_registry_(std::move(pref_registry)) {
  CHECK(base::FeatureList::IsEnabled(syncer::kEnablePreferencesAccountStorage));
  CHECK(dual_layer_user_prefs);
  ConnectAssociatorsAndRegisterPreferences();
}

void PrefServiceSyncable::ConnectAssociatorsAndRegisterPreferences() {
  pref_sync_associator_.SetPrefService(this);
  priority_pref_sync_associator_.SetPrefService(this);
#if BUILDFLAG(IS_CHROMEOS_ASH)
  os_pref_sync_associator_.SetPrefService(this);
  os_priority_pref_sync_associator_.SetPrefService(this);
#endif

  // Add already-registered syncable preferences to PrefModelAssociator.
  for (const auto& [path, value] : *pref_registry_) {
    AddRegisteredSyncablePreference(path,
                                    pref_registry_->GetRegistrationFlags(path));
  }

  // Watch for syncable preferences registered after this point.
  static_cast<user_prefs::PrefRegistrySyncable*>(pref_registry_.get())
      ->SetSyncableRegistrationCallback(base::BindRepeating(
          &PrefServiceSyncable::AddRegisteredSyncablePreference,
          base::Unretained(this)));
}

PrefServiceSyncable::~PrefServiceSyncable() {
  // Remove our callback from the registry, since it may outlive us.
  pref_registry_->SetSyncableRegistrationCallback(base::NullCallback());
}

std::unique_ptr<PrefServiceSyncable>
PrefServiceSyncable::CreateIncognitoPrefService(
    PrefStore* incognito_extension_pref_store,
    const std::vector<const char*>& persistent_pref_names) {
  pref_service_forked_ = true;
  auto pref_notifier = std::make_unique<PrefNotifierImpl>();

  scoped_refptr<user_prefs::PrefRegistrySyncable> forked_registry =
      pref_registry_->ForkForIncognito();

  auto overlay = base::MakeRefCounted<InMemoryPrefStore>();
  auto incognito_pref_store = base::MakeRefCounted<OverlayUserPrefStore>(
      overlay.get(), user_pref_store_.get());

  for (const char* persistent_pref_name : persistent_pref_names) {
    incognito_pref_store->RegisterPersistentPref(persistent_pref_name);
  }

  auto pref_value_store = pref_value_store_->CloneAndSpecialize(
      nullptr,  // managed
      nullptr,  // supervised_user
      incognito_extension_pref_store,
      nullptr,  // standalone_browser_prefs
      nullptr,  // command_line_prefs
      incognito_pref_store.get(),
      nullptr,  // recommended
      forked_registry->defaults().get(), pref_notifier.get());
  return std::make_unique<PrefServiceSyncable>(
      std::move(pref_notifier), std::move(pref_value_store),
      incognito_pref_store,
      nullptr,  // standalone_browser_prefs
      std::move(forked_registry), pref_sync_associator_.client(),
      read_error_callback_, false);
}

bool PrefServiceSyncable::IsSyncing() {
  return pref_sync_associator_.models_associated();
}

bool PrefServiceSyncable::IsPrioritySyncing() {
  return priority_pref_sync_associator_.models_associated();
}

#if BUILDFLAG(IS_CHROMEOS_ASH)
bool PrefServiceSyncable::AreOsPrefsSyncing() {
  return os_pref_sync_associator_.models_associated();
}

bool PrefServiceSyncable::AreOsPriorityPrefsSyncing() {
  return os_priority_pref_sync_associator_.models_associated();
}
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

void PrefServiceSyncable::AddObserver(PrefServiceSyncableObserver* observer) {
  observer_list_.AddObserver(observer);
}

void PrefServiceSyncable::RemoveObserver(
    PrefServiceSyncableObserver* observer) {
  observer_list_.RemoveObserver(observer);
}

syncer::SyncableService* PrefServiceSyncable::GetSyncableService(
    const syncer::ModelType& type) {
  switch (type) {
    case syncer::PREFERENCES:
      return &pref_sync_associator_;
    case syncer::PRIORITY_PREFERENCES:
      return &priority_pref_sync_associator_;
#if BUILDFLAG(IS_CHROMEOS_ASH)
    case syncer::OS_PREFERENCES:
      return &os_pref_sync_associator_;
    case syncer::OS_PRIORITY_PREFERENCES:
      return &os_priority_pref_sync_associator_;
#endif
    default:
      NOTREACHED() << "invalid model type: " << type;
      return nullptr;
  }
}

void PrefServiceSyncable::UpdateCommandLinePrefStore(
    PrefStore* cmd_line_store) {
  // If |pref_service_forked_| is true, then this PrefService and the forked
  // copies will be out of sync.
  DCHECK(!pref_service_forked_);
  PrefService::UpdateCommandLinePrefStore(cmd_line_store);
}

void PrefServiceSyncable::AddSyncedPrefObserver(const std::string& name,
                                                SyncedPrefObserver* observer) {
  pref_sync_associator_.AddSyncedPrefObserver(name, observer);
  priority_pref_sync_associator_.AddSyncedPrefObserver(name, observer);
#if BUILDFLAG(IS_CHROMEOS_ASH)
  os_pref_sync_associator_.AddSyncedPrefObserver(name, observer);
  os_priority_pref_sync_associator_.AddSyncedPrefObserver(name, observer);
#endif
}

void PrefServiceSyncable::RemoveSyncedPrefObserver(
    const std::string& name,
    SyncedPrefObserver* observer) {
  pref_sync_associator_.RemoveSyncedPrefObserver(name, observer);
  priority_pref_sync_associator_.RemoveSyncedPrefObserver(name, observer);
#if BUILDFLAG(IS_CHROMEOS_ASH)
  os_pref_sync_associator_.RemoveSyncedPrefObserver(name, observer);
  os_priority_pref_sync_associator_.RemoveSyncedPrefObserver(name, observer);
#endif
}

void PrefServiceSyncable::AddRegisteredSyncablePreference(
    const std::string& path,
    uint32_t flags) {
  DCHECK(FindPreference(path));
  if (flags & user_prefs::PrefRegistrySyncable::SYNCABLE_PREF) {
    pref_sync_associator_.RegisterPref(path);
    return;
  }
  if (flags & user_prefs::PrefRegistrySyncable::SYNCABLE_PRIORITY_PREF) {
    priority_pref_sync_associator_.RegisterPref(path);
    return;
  }
#if BUILDFLAG(IS_CHROMEOS_ASH)
  if (flags & user_prefs::PrefRegistrySyncable::SYNCABLE_OS_PREF) {
    os_pref_sync_associator_.RegisterPref(path);
    return;
  }
  if (flags & user_prefs::PrefRegistrySyncable::SYNCABLE_OS_PRIORITY_PREF) {
    os_priority_pref_sync_associator_.RegisterPref(path);
    return;
  }
#endif
}

base::Value::Type PrefServiceSyncable::GetRegisteredPrefType(
    const std::string& pref_name) const {
  const Preference* pref = FindPreference(pref_name);
  DCHECK(pref);
  return pref->GetType();
}

void PrefServiceSyncable::OnIsSyncingChanged() {
  for (auto& observer : observer_list_) {
    observer.OnIsSyncingChanged();
  }
}

uint32_t PrefServiceSyncable::GetWriteFlags(
    const std::string& pref_name) const {
  const Preference* pref = FindPreference(pref_name);
  return PrefService::GetWriteFlags(pref);
}

}  // namespace sync_preferences
