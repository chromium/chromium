// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync_preferences/pref_service_syncable.h"

#include <utility>

#include "base/bind.h"
#include "base/callback.h"
#include "base/strings/string_number_conversions.h"
#include "base/value_conversions.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/default_pref_store.h"
#include "components/prefs/in_memory_pref_store.h"
#include "components/prefs/overlay_user_pref_store.h"
#include "components/prefs/pref_notifier_impl.h"
#include "components/prefs/pref_registry.h"
#include "components/prefs/pref_value_store.h"
#include "components/sync_preferences/pref_model_associator.h"
#include "components/sync_preferences/pref_service_syncable_observer.h"
#include "components/sync_preferences/synced_pref_observer.h"

#if defined(OS_CHROMEOS)
#include "chromeos/constants/chromeos_features.h"
#endif

namespace sync_preferences {

// TODO(tschumann): Handing out pointers to this in the constructor is an
// anti-pattern. Instead, introduce a factory method which first constructs
// the PrefServiceSyncable instance and then the members which need a reference
// to the PrefServiceSycnable instance.
PrefServiceSyncable::PrefServiceSyncable(
    std::unique_ptr<PrefNotifierImpl> pref_notifier,
    std::unique_ptr<PrefValueStore> pref_value_store,
    scoped_refptr<PersistentPrefStore> user_prefs,
    scoped_refptr<user_prefs::PrefRegistrySyncable> pref_registry,
    const PrefModelAssociatorClient* pref_model_associator_client,
    base::RepeatingCallback<void(PersistentPrefStore::PrefReadError)>
        read_error_callback,
    bool async)
    : PrefService(std::move(pref_notifier),
                  std::move(pref_value_store),
                  user_prefs,
                  pref_registry,
                  std::move(read_error_callback),
                  async),
      pref_service_forked_(false),
      pref_sync_associator_(pref_model_associator_client,
                            syncer::PREFERENCES,
                            user_prefs.get()),
      priority_pref_sync_associator_(pref_model_associator_client,
                                     syncer::PRIORITY_PREFERENCES,
                                     user_prefs.get()),
#if defined(OS_CHROMEOS)
      os_pref_sync_associator_(pref_model_associator_client,
                               syncer::OS_PREFERENCES,
                               user_prefs.get()),
      os_priority_pref_sync_associator_(pref_model_associator_client,
                                        syncer::OS_PRIORITY_PREFERENCES,
                                        user_prefs.get()),
#endif
      pref_registry_(std::move(pref_registry)) {
  pref_sync_associator_.SetPrefService(this);
  priority_pref_sync_associator_.SetPrefService(this);
#if defined(OS_CHROMEOS)
  os_pref_sync_associator_.SetPrefService(this);
  os_priority_pref_sync_associator_.SetPrefService(this);
#endif

  // Let PrefModelAssociators know about changes to preference values.
  pref_value_store_->set_callback(base::Bind(
      &PrefServiceSyncable::ProcessPrefChange, base::Unretained(this)));

  // Add already-registered syncable preferences to PrefModelAssociator.
  for (const auto& entry : *pref_registry_) {
    const std::string& path = entry.first;
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
  pref_registry_->SetSyncableRegistrationCallback(
      user_prefs::PrefRegistrySyncable::SyncableRegistrationCallback());
}

std::unique_ptr<PrefServiceSyncable>
PrefServiceSyncable::CreateIncognitoPrefService(
    PrefStore* incognito_extension_pref_store,
    const std::vector<const char*>& persistent_pref_names,
    std::unique_ptr<PrefValueStore::Delegate> delegate) {
  pref_service_forked_ = true;
  auto pref_notifier = std::make_unique<PrefNotifierImpl>();

  scoped_refptr<user_prefs::PrefRegistrySyncable> forked_registry =
      pref_registry_->ForkForIncognito();

  auto overlay = base::MakeRefCounted<InMemoryPrefStore>();
  if (delegate) {
    delegate->InitIncognitoUserPrefs(overlay, user_pref_store_,
                                     persistent_pref_names);
    delegate->InitPrefRegistry(forked_registry.get());
  }
  auto incognito_pref_store = base::MakeRefCounted<OverlayUserPrefStore>(
      overlay.get(), user_pref_store_.get());

  for (const char* persistent_pref_name : persistent_pref_names)
    incognito_pref_store->RegisterPersistentPref(persistent_pref_name);

  auto pref_value_store = pref_value_store_->CloneAndSpecialize(
      nullptr,  // managed
      nullptr,  // supervised_user
      incognito_extension_pref_store,
      nullptr,  // command_line_prefs
      incognito_pref_store.get(),
      nullptr,  // recommended
      forked_registry->defaults().get(), pref_notifier.get(),
      std::move(delegate));
  return std::make_unique<PrefServiceSyncable>(
      std::move(pref_notifier), std::move(pref_value_store),
      std::move(incognito_pref_store), std::move(forked_registry),
      pref_sync_associator_.client(), read_error_callback_, false);
}

bool PrefServiceSyncable::IsSyncing() {
  if (pref_sync_associator_.models_associated())
    return true;
#if defined(OS_CHROMEOS)
  if (os_pref_sync_associator_.models_associated())
    return true;
#endif
  return false;
}

bool PrefServiceSyncable::IsPrioritySyncing() {
  if (priority_pref_sync_associator_.models_associated())
    return true;
#if defined(OS_CHROMEOS)
  if (os_priority_pref_sync_associator_.models_associated())
    return true;
#endif
  return false;
}

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
#if defined(OS_CHROMEOS)
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
#if defined(OS_CHROMEOS)
  os_pref_sync_associator_.AddSyncedPrefObserver(name, observer);
  os_priority_pref_sync_associator_.AddSyncedPrefObserver(name, observer);
#endif
}

void PrefServiceSyncable::RemoveSyncedPrefObserver(
    const std::string& name,
    SyncedPrefObserver* observer) {
  pref_sync_associator_.RemoveSyncedPrefObserver(name, observer);
  priority_pref_sync_associator_.RemoveSyncedPrefObserver(name, observer);
#if defined(OS_CHROMEOS)
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
#if defined(OS_CHROMEOS)
  if (flags & user_prefs::PrefRegistrySyncable::SYNCABLE_OS_PREF) {
    if (chromeos::features::IsSplitSettingsSyncEnabled()) {
      // Register the pref under the new ModelType::OS_PREFERENCES.
      os_pref_sync_associator_.RegisterPref(path);
      // Also register under the old ModelType::PREFERENCES. This ensures that
      // local changes to OS prefs are also synced to old clients that have the
      // pref registered as a browser SYNCABLE_PREF.
      pref_sync_associator_.RegisterPrefWithLegacyModelType(path);
    } else {
      // Behave like an old client and treat this pref like it was registered
      // as a SYNCABLE_PREF under ModelType::PREFERENCES.
      pref_sync_associator_.RegisterPref(path);
    }
    return;
  }
  if (flags & user_prefs::PrefRegistrySyncable::SYNCABLE_OS_PRIORITY_PREF) {
    // See comments for SYNCABLE_OS_PREF above.
    if (chromeos::features::IsSplitSettingsSyncEnabled()) {
      os_priority_pref_sync_associator_.RegisterPref(path);
      priority_pref_sync_associator_.RegisterPrefWithLegacyModelType(path);
    } else {
      priority_pref_sync_associator_.RegisterPref(path);
    }
    return;
  }
#endif
}

void PrefServiceSyncable::OnIsSyncingChanged() {
  for (auto& observer : observer_list_)
    observer.OnIsSyncingChanged();
}

void PrefServiceSyncable::ProcessPrefChange(const std::string& name) {
  pref_sync_associator_.ProcessPrefChange(name);
  priority_pref_sync_associator_.ProcessPrefChange(name);
#if defined(OS_CHROMEOS)
  os_pref_sync_associator_.ProcessPrefChange(name);
  os_priority_pref_sync_associator_.ProcessPrefChange(name);
#endif
}

}  // namespace sync_preferences
