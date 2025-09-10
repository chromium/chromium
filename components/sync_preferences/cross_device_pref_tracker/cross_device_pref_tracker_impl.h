// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_PREFERENCES_CROSS_DEVICE_PREF_TRACKER_CROSS_DEVICE_PREF_TRACKER_IMPL_H_
#define COMPONENTS_SYNC_PREFERENCES_CROSS_DEVICE_PREF_TRACKER_CROSS_DEVICE_PREF_TRACKER_IMPL_H_

#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/observer_list.h"
#include "base/scoped_observation.h"
#include "build/build_config.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/sync_device_info/device_info_tracker.h"
#include "components/sync_preferences/cross_device_pref_tracker/cross_device_pref_tracker.h"

#if BUILDFLAG(IS_ANDROID)
#include "base/android/scoped_java_ref.h"
#endif  // BUILDFLAG(IS_ANDROID)

class PrefService;

namespace syncer {
class DeviceInfoSyncService;
}  // namespace syncer

namespace sync_preferences {

class CrossDevicePrefTrackerImpl : public CrossDevicePrefTracker,
                                   public syncer::DeviceInfoTracker::Observer {
 public:
  CrossDevicePrefTrackerImpl(
      PrefService* profile_pref_service,
      PrefService* local_pref_service,
      syncer::DeviceInfoSyncService* device_info_sync_service);
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

  // `KeyedService` override
  void Shutdown() override;

  // `syncer::DeviceInfoTracker::Observer` overrides
  void OnDeviceInfoChange() override;

#if BUILDFLAG(IS_ANDROID)
  // Return the java object that allows access to the SyncService.
  base::android::ScopedJavaLocalRef<jobject> GetJavaObject() override;
  // Java versions of query methods.
  base::android::ScopedJavaLocalRef<jobjectArray> GetValues(
      JNIEnv* env,
      const base::android::JavaParamRef<jstring>& pref_name,
      std::optional<int> os_type,
      std::optional<int> form_factor) const override;
  base::android::ScopedJavaLocalRef<jobject> GetMostRecentValue(
      JNIEnv* env,
      const base::android::JavaParamRef<jstring>& pref_name,
      std::optional<int> os_type,
      std::optional<int> form_factor) const override;
#endif  // BUILDFLAG(IS_ANDROID)

 private:
  // `PrefService` for Profile-based preferences (including syncable prefs).
  // Must outlive this object until Shutdown().
  raw_ptr<PrefService> profile_pref_service_;

  // `PrefService` for local-state preferences.
  // Must outlive this object until Shutdown().
  raw_ptr<PrefService> local_pref_service_;

  // Provides access to `LocalDeviceInfoProvider` (for local Cache GUID) and
  // `DeviceInfoTracker` (for remote metadata).
  // Must outlive this object until Shutdown().
  raw_ptr<syncer::DeviceInfoSyncService> device_info_sync_service_;

  // Registrars for observing changes to tracked prefs.
  PrefChangeRegistrar profile_pref_registrar_;
  PrefChangeRegistrar local_pref_registrar_;

  // Observation for changes in `DeviceInfo`.
  base::ScopedObservation<syncer::DeviceInfoTracker,
                          syncer::DeviceInfoTracker::Observer>
      device_info_observation_{this};

  // List of observers notified of remote preference changes.
  base::ObserverList<CrossDevicePrefTracker::Observer, true> observers_;

#if BUILDFLAG(IS_ANDROID)
  base::android::ScopedJavaGlobalRef<jobject> java_object_;
#endif  // BUILDFLAG(IS_ANDROID)
};

}  // namespace sync_preferences

#endif  // COMPONENTS_SYNC_PREFERENCES_CROSS_DEVICE_PREF_TRACKER_CROSS_DEVICE_PREF_TRACKER_IMPL_H_
