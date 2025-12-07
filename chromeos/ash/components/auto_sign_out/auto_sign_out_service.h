// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_AUTO_SIGN_OUT_AUTO_SIGN_OUT_SERVICE_H_
#define CHROMEOS_ASH_COMPONENTS_AUTO_SIGN_OUT_AUTO_SIGN_OUT_SERVICE_H_

#include "base/callback_list.h"
#include "base/component_export.h"
#include "base/memory/raw_ref.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "base/time/time.h"
#include "chromeos/dbus/power/power_manager_client.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/session_manager/core/session_manager_observer.h"
#include "components/sync_device_info/device_info_tracker.h"

class PrefService;

namespace session_manager {
class SessionManager;
}  // namespace session_manager

namespace syncer {
class DeviceInfoSyncService;
}  // namespace syncer

namespace ash {

// A service that automatically signs out a user out of their previous
// session when they sign in to a new ChromeOS device.
// When a new sign-in is detected, an automatic sign-out is triggered which
// ensures that only one active session exists for a user at any given time.
class COMPONENT_EXPORT(AUTO_SIGN_OUT) AutoSignOutService
    : public syncer::DeviceInfoTracker::Observer,
      public session_manager::SessionManagerObserver,
      public chromeos::PowerManagerClient::Observer {
 public:
  // DeviceInfoSyncService is a KeyedServices whose lifetime is managed by the
  // user's Profile. The owner of AutoSignOutService instance is responsible for
  // ensuring that it doesn't outlive these KeyedServices.
  // SessionManager shutdown happens after primary profile shutdown, which is
  // where AutoSignOutService is destroyed, so it is guaranteed to outlive it.
  // PrefService is tied to the user's Profile, which is guaranteed to outlive
  // AutoSignOutService.
  AutoSignOutService(syncer::DeviceInfoSyncService* device_info_sync_service,
                     session_manager::SessionManager* session_manager,
                     PrefService* prefs);
  AutoSignOutService(const AutoSignOutService&) = delete;
  AutoSignOutService& operator=(const AutoSignOutService&) = delete;

  ~AutoSignOutService() override;

  // syncer::DeviceInfoTracker::Observer override:
  void OnDeviceInfoChange() override;

  // session_manager::SessionManagerObserver override:
  void OnUnlockScreenAttempt(
      const bool success,
      const session_manager::UnlockType unlock_type) override;

  // chromeos::PowerManagerClient::Observer override:
  void SuspendDone(base::TimeDelta sleep_duration) override;

 private:
  // Registers listeners to call `UpdateObservations` upon a change to any
  // relevant pref value.
  void RegisterPrefListeners();

  // Updates observers to needed services based on relevant pref values. It is
  // responsible for starting or stopping the AutoSignOutService dynamically.
  void UpdateObservations();

  // Checks if local device info is ready before updating device.
  void UpdateLocalDeviceInfoWhenReady();

  void OnLocalDeviceInfoProviderReady();

  // Updates the local device info with the new sign-in time.
  void UpdateLocalDeviceInfo();

  const raw_ref<syncer::DeviceInfoSyncService> device_info_sync_service_;

  const raw_ref<session_manager::SessionManager> session_manager_;

  const raw_ref<PrefService> prefs_;

  PrefChangeRegistrar pref_change_registrar_;

  base::Time initialization_time_;

  base::ScopedObservation<syncer::DeviceInfoTracker,
                          syncer::DeviceInfoTracker::Observer>
      device_info_tracker_observation_{this};

  base::ScopedObservation<session_manager::SessionManager,
                          session_manager::SessionManagerObserver>
      session_manager_observation_{this};

  base::ScopedObservation<chromeos::PowerManagerClient,
                          chromeos::PowerManagerClient::Observer>
      power_manager_client_observation_{this};

  base::CallbackListSubscription local_device_info_ready_subscription_;

  base::WeakPtrFactory<AutoSignOutService> weak_pointer_factory_{this};
};

}  // namespace ash

#endif  // CHROMEOS_ASH_COMPONENTS_AUTO_SIGN_OUT_AUTO_SIGN_OUT_SERVICE_H_
