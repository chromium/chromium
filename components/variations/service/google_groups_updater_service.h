// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VARIATIONS_SERVICE_GOOGLE_GROUPS_UPDATER_SERVICE_H_
#define COMPONENTS_VARIATIONS_SERVICE_GOOGLE_GROUPS_UPDATER_SERVICE_H_

#include "base/files/file_path.h"
#include "base/scoped_observation.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/prefs/pref_service.h"
#include "components/sync/service/sync_service_observer.h"

namespace syncer {
class SyncService;
}

namespace user_prefs {
class PrefRegistrySyncable;
}

namespace variations {
// Per-profile preference for the sync data containing the list of dogfood group
// gaia IDs for a given syncing user.
// The variables below are the pref name, and the key for the gaia ID within
// the dictionary value.
#if BUILDFLAG(IS_CHROMEOS_ASH)
inline constexpr char kOsDogfoodGroupsSyncPrefName[] = "sync.os_dogfood_groups";
#else
inline constexpr char kDogfoodGroupsSyncPrefName[] = "sync.dogfood_groups";
#endif

inline constexpr char kDogfoodGroupsSyncPrefGaiaIdKey[] = "gaia_id";
}  // namespace variations

// Service responsible for one-way synchronization of Google group information
// from per-profile sync data to local-state.
class GoogleGroupsUpdaterService : public KeyedService,
                                   public syncer::SyncServiceObserver {
 public:
  explicit GoogleGroupsUpdaterService(PrefService& target_prefs,
                                      const std::string& key,
                                      PrefService& source_prefs);

  GoogleGroupsUpdaterService(const GoogleGroupsUpdaterService&) = delete;
  GoogleGroupsUpdaterService& operator=(const GoogleGroupsUpdaterService&) =
      delete;

  ~GoogleGroupsUpdaterService() override;

  static void RegisterProfilePrefs(user_prefs::PrefRegistrySyncable* registry);

  // KeyedService overrides.
  void Shutdown() override;

  void OnSyncServiceInitialized(syncer::SyncService* sync_service);

  // syncer::SyncServiceObserver overrides.
  void OnStateChanged(syncer::SyncService* sync) override;
  void OnSyncShutdown(syncer::SyncService* sync) override;

 private:
  // Clears state that should only exist for a signed in user.
  // This should be called when the user signs out or disables sync, as the
  // server is the source-of-truth for this state, not the client.
  void ClearSigninScopedState();

  // Update the group memberships in `target_prefs_`.
  // Called when `source_prefs_` have been initialized or modified.
  void UpdateGoogleGroups();

  // The preferences to write to. These are the local-state prefs.
  // Preferences are guaranteed to outlive keyed services, so this reference
  // will stay valid for the lifetime of this service.
  const raw_ref<PrefService> target_prefs_;

  // The key to use in the `target_prefs_` dictionary.
  // This key is immutable (it will not change for eg. a given profile, even
  // across Chrome restarts).
  const std::string key_;

  // The preferences to read from. These are the profile prefs.
  // Preferences are guaranteed to outlive keyed services, so this reference
  // will stay valid for the lifetime of this service.
  const raw_ref<PrefService> source_prefs_;

  PrefChangeRegistrar pref_change_registrar_;

  // The SyncService observation. When Sync gets disabled (most likely due to
  // user signout), the groups-related preferences must be cleared.
  base::ScopedObservation<syncer::SyncService, syncer::SyncServiceObserver>
      sync_service_observation_{this};
};

#endif  // COMPONENTS_VARIATIONS_SERVICE_GOOGLE_GROUPS_UPDATER_SERVICE_H_
