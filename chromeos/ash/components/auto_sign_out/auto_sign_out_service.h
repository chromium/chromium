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
#include "components/sync/service/sync_service_observer.h"

namespace session_manager {
class SessionManager;
}  // namespace session_manager

namespace syncer {
class DeviceInfoSyncService;
class SyncService;
}  // namespace syncer

namespace ash {

// A service that automatically signs out a user out of their previous
// session when they sign in to a new ChromeOS device.
// When a new sign-in is detected, an automatic sign-out is triggered which
// ensures that only one active session exists for a user at any given time.
class COMPONENT_EXPORT(AUTO_SIGN_OUT) AutoSignOutService
    : public syncer::SyncServiceObserver {
 public:
  // DeviceInfoSyncService and SyncService are KeyedServices whose lifetime
  // is managed by the user's Profile. The owner of AutoSignOutService instance
  // is responsible for ensuring that it doesn't outlive these KeyedServices.
  // SessionManager shutdown happens after primary profile shutdown, which is
  // where AutoSignOutService is destroyed, so it is guaranteed to outlive it.
  AutoSignOutService(syncer::DeviceInfoSyncService* device_info_sync_service,
                     syncer::SyncService* sync_service,
                     session_manager::SessionManager* session_manager);
  AutoSignOutService(const AutoSignOutService&) = delete;
  AutoSignOutService& operator=(const AutoSignOutService&) = delete;

  ~AutoSignOutService() override;

  // syncer::SyncServiceObserver override:
  void OnStateChanged(syncer::SyncService* sync) override;

 private:
  // Updates the local device info with the new sign-in time.
  void UpdateLocalDeviceInfo();

  const raw_ref<syncer::DeviceInfoSyncService> device_info_sync_service_;

  const raw_ref<syncer::SyncService> sync_service_;

  const raw_ref<session_manager::SessionManager> session_manager_;

  base::Time initialization_time_;

  base::ScopedObservation<syncer::SyncService, syncer::SyncServiceObserver>
      sync_service_observation_{this};

  base::CallbackListSubscription local_device_info_ready_subscription_;

  base::WeakPtrFactory<AutoSignOutService> weak_pointer_factory_{this};
};

}  // namespace ash

#endif  // CHROMEOS_ASH_COMPONENTS_AUTO_SIGN_OUT_AUTO_SIGN_OUT_SERVICE_H_
