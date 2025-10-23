// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_PREFERENCES_CROSS_DEVICE_PREF_TRACKER_CROSS_DEVICE_PREF_TRACKER_IMPL_H_
#define COMPONENTS_SYNC_PREFERENCES_CROSS_DEVICE_PREF_TRACKER_CROSS_DEVICE_PREF_TRACKER_IMPL_H_

#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "base/containers/flat_map.h"
#include "base/containers/flat_set.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/scoped_observation.h"
#include "base/values.h"
#include "build/build_config.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/sync/service/sync_service_observer.h"
#include "components/sync_device_info/device_info.h"
#include "components/sync_device_info/device_info_tracker.h"
#include "components/sync_preferences/cross_device_pref_tracker/cross_device_pref_provider.h"
#include "components/sync_preferences/cross_device_pref_tracker/cross_device_pref_tracker.h"

#if BUILDFLAG(IS_ANDROID)
#include "base/android/scoped_java_ref.h"
#endif  // BUILDFLAG(IS_ANDROID)

class PrefService;

namespace syncer {
class DeviceInfoSyncService;
class SyncService;
}  // namespace syncer

namespace sync_preferences {

// Concrete implementation of `CrossDevicePrefTracker`.
class CrossDevicePrefTrackerImpl : public CrossDevicePrefTracker,
                                   public syncer::DeviceInfoTracker::Observer,
                                   public syncer::SyncServiceObserver {
 public:
  CrossDevicePrefTrackerImpl(
      PrefService* profile_pref_service,
      PrefService* local_pref_service,
      syncer::DeviceInfoSyncService* device_info_sync_service,
      syncer::SyncService* sync_service,
      std::unique_ptr<CrossDevicePrefProvider> pref_provider);
  ~CrossDevicePrefTrackerImpl() override;

  CrossDevicePrefTrackerImpl(const CrossDevicePrefTrackerImpl&) = delete;
  CrossDevicePrefTrackerImpl& operator=(const CrossDevicePrefTrackerImpl&) =
      delete;

  // `CrossDevicePrefTracker` overrides
  void AddObserver(CrossDevicePrefTracker::Observer* observer) override;
  void RemoveObserver(CrossDevicePrefTracker::Observer* observer) override;
  std::vector<TimestampedPrefValue> GetValues(
      std::string_view pref_name,
      const DeviceFilter& filter) const override;
  std::optional<TimestampedPrefValue> GetMostRecentValue(
      std::string_view pref_name,
      const DeviceFilter& filter) const override;

  // `KeyedService` overrides
  void Shutdown() override;

  // `syncer::DeviceInfoTracker::Observer` overrides
  void OnDeviceInfoChange() override;

  // `syncer::SyncServiceObserver` overrides
  void OnStateChanged(syncer::SyncService* sync) override;
  void OnSyncShutdown(syncer::SyncService* sync) override;

#if BUILDFLAG(IS_ANDROID)
  // Return the java object that allows access to the SyncService.
  base::android::ScopedJavaLocalRef<jobject> GetJavaObject() override;
  // Java versions of query methods.
  base::android::ScopedJavaLocalRef<jobjectArray> GetValues(
      JNIEnv* env,
      const base::android::JavaParamRef<jstring>& pref_name,
      std::optional<int> os_type,
      std::optional<int> form_factor,
      std::optional<jlong> max_sync_recency_microseconds) const override;
  base::android::ScopedJavaLocalRef<jobject> GetMostRecentValue(
      JNIEnv* env,
      const base::android::JavaParamRef<jstring>& pref_name,
      std::optional<int> os_type,
      std::optional<int> form_factor,
      std::optional<jlong> max_sync_recency_microseconds) const override;
#endif  // BUILDFLAG(IS_ANDROID)

  // Exposed for testing.
  const syncer::SyncService* sync_service() const { return sync_service_; }

 private:
  // Initializes observation of the cross-device storage prefs and the cache
  // structure. This also performs validation of all pref mappings.
  void StartObservingCrossDevicePrefs();

  // Starts tracking a set of pref names by validating, observing, and
  // synchronizing each pref. The associated PrefService is derived from the
  // `registrar`.
  void StartTrackingPrefs(
      const base::flat_set<std::string_view>& pref_names,
      PrefChangeRegistrar& registrar,
      const PrefChangeRegistrar::NamedChangeAsViewCallback& callback);

  // Pushes the current state of the specified prefs to the cross-device
  // storage. This is used for initial synchronization and is NOT considered
  // an observed change.
  void SyncOnDevicePrefsToCrossDevice(
      const base::flat_set<std::string_view>& pref_names,
      PrefService* tracked_pref_service);

  // Handles notifications from the `PrefChangeRegistrar` when a tracked profile
  // pref is modified.
  void OnTrackedProfilePrefChanged(std::string_view tracked_pref_name);

  // Handles notifications from the `PrefChangeRegistrar` when a tracked local
  // state pref is modified.
  void OnTrackedLocalStatePrefChanged(std::string_view tracked_pref_name);

  // Handles notifications from the `PrefChangeRegistrar` when a cross-device
  // storage dictionary pref is modified (due to local or remote changes).
  void OnCrossDevicePrefChanged(std::string_view cross_device_pref_name);

  // Compares the old and new states of a dictionary to identify changes and
  // notifies observers only if the changes are remote.
  void ProcessRemoteUpdates(const std::string& cross_device_pref_name,
                            const base::Value::Dict& old_dict,
                            const base::Value::Dict& new_dict);

  // Attempts to parse the dictionary `entry` (if provided) associated with
  // `remote_device_info` and notifies observers if successful. If `entry` is
  // null, it signifies a deletion.
  // `remote_device_info` is guaranteed to be valid for the duration of the
  // synchronous observer calls.
  void NotifyRemotePrefChanged(const std::string& cross_device_pref_name,
                               const base::Value::Dict* entry,
                               const syncer::DeviceInfo& remote_device_info);

  // Checks if local device info became ready and performs initial sync if so.
  void HandleLocalDeviceInfoIfAvailable();

  // Checks if Sync is active and ready for writes, and performs a full refresh
  // if the state changed from unconfigured to configured.
  void OnSyncStateChanged();

  // Helper to determine if sync is configured by the user for the relevant data
  // types.
  bool IsSyncConfiguredForWrites() const;

  // Pushes the current state of all tracked prefs. Used when Sync state changes
  // or local device info becomes ready.
  void SyncAllOnDevicePrefsToCrossDevice();

  // Detects newly available devices by comparing known GUIDs with the current
  // list from `DeviceInfoTracker`, and triggers notifications for their prefs.
  void HandleRemoteDeviceInfoChanges();

  // Iterates over the storage cache and notifies observers about existing
  // values for newly available devices (handles asynchronous `DeviceInfo`).
  // Pointers in `new_devices` are valid for the duration of the call.
  void NotifyObserversOfExistingPrefsForNewDevices(
      const std::vector<const syncer::DeviceInfo*>& new_devices);

  // Removes entries from cross-device storage dictionaries corresponding to
  // devices that are no longer known by `DeviceInfoTracker`. Relies on
  // `active_device_guids_` being up-to-date.
  void GarbageCollectStaleCacheGuids();

  // Returns all devices (local and remote) deemed active. A device is active if
  // it has not expired, based on its `DeviceInfo::last_updated_timestamp`.
  std::vector<const syncer::DeviceInfo*> GetActiveDevices() const;

  // `PrefService` for profile-based preferences (including syncable prefs).
  // Must outlive this object until `Shutdown()`.
  raw_ptr<PrefService> profile_pref_service_;

  // `PrefService` for local-state preferences.
  // Must outlive this object until `Shutdown()`.
  raw_ptr<PrefService> local_pref_service_;

  // Provides access to `LocalDeviceInfoProvider` (for local Cache GUID) and
  // `DeviceInfoTracker` (for remote metadata).
  // Must outlive this object until `Shutdown()`.
  raw_ptr<syncer::DeviceInfoSyncService> device_info_sync_service_;

  // Provides access to the Sync engine state.
  // Must outlive this object until `Shutdown()` or `OnSyncShutdown()`.
  raw_ptr<syncer::SyncService> sync_service_;

  // Provides the lists of prefs to be tracked.
  std::unique_ptr<CrossDevicePrefProvider> pref_provider_;

  // Registrars for observing changes to tracked prefs.
  PrefChangeRegistrar profile_pref_registrar_;
  PrefChangeRegistrar local_pref_registrar_;

  // Registrar for observing changes to cross-device storage prefs (remote
  // changes). These are Profile prefs.
  PrefChangeRegistrar cross_device_pref_registrar_;

  // Cache of the last known state for each cross-device dictionary.
  // Used to identify changes when a pref is updated on a remote device.
  // Maps `cross_device_pref_name` -> dictionary value.
  base::flat_map<std::string, base::Value::Dict> cross_device_storage_cache_;

  // Set of Cache GUIDs for devices that have available `DeviceInfo` from the
  // tracker and are considered active (i.e., not expired). A device is active
  // if it has synced within the `kDeviceExpirationTimeout` window. Stores
  // copies of the GUID strings for safety.
  base::flat_set<std::string> active_device_guids_;

  // Observation for changes in `DeviceInfo`.
  base::ScopedObservation<syncer::DeviceInfoTracker,
                          syncer::DeviceInfoTracker::Observer>
      device_info_observation_{this};

  // Observation for changes in `SyncService` state.
  base::ScopedObservation<syncer::SyncService, syncer::SyncServiceObserver>
      sync_service_observation_{this};

  // List of observers notified of remote preference changes.
  base::ObserverList<CrossDevicePrefTracker::Observer, true> observers_;

  // Tracks whether the `LocalDeviceInfo` (and thus the local Cache GUID) is
  // available. Used to ensure the system only retries pushing all prefs once
  // when the info initially becomes available.
  bool is_local_device_info_ready_ = false;

  // Tracks the last known state of Sync configuration for writes. Used to
  // detect transitions from unconfigured to configured and trigger refreshes.
  bool is_sync_configured_for_writes_ = false;

#if BUILDFLAG(IS_ANDROID)
  base::android::ScopedJavaGlobalRef<jobject> java_object_;
#endif  // BUILDFLAG(IS_ANDROID)

  base::WeakPtrFactory<CrossDevicePrefTrackerImpl> weak_ptr_factory_{this};
};

}  // namespace sync_preferences

#endif  // COMPONENTS_SYNC_PREFERENCES_CROSS_DEVICE_PREF_TRACKER_CROSS_DEVICE_PREF_TRACKER_IMPL_H_
