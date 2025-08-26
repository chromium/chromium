// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_AUTO_SIGN_OUT_AUTO_SIGN_OUT_SERVICE_H_
#define CHROMEOS_ASH_COMPONENTS_AUTO_SIGN_OUT_AUTO_SIGN_OUT_SERVICE_H_

#include "base/callback_list.h"
#include "base/component_export.h"
#include "base/memory/raw_ref.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"

namespace syncer {
class DeviceInfoSyncService;
}  // namespace syncer

namespace ash {

// A service that automatically signs out a user out of their previous
// session when they sign in to a new ChromeOS device.
// When a new sign-in is detected, an automatic sign-out is triggered which
// ensures that only one active session exists for a user at any given time.
class COMPONENT_EXPORT(AUTO_SIGN_OUT) AutoSignOutService {
 public:
  explicit AutoSignOutService(
      // A pointer to the DeviceInfoSyncService. This is a KeyedService whose
      // lifetime is managed by the user's Profile. As such, it is guaranteed
      // to outlive AutoSignOutService using a DependsOn() relationship.
      syncer::DeviceInfoSyncService* device_info_sync_service);
  AutoSignOutService(const AutoSignOutService&) = delete;
  AutoSignOutService& operator=(const AutoSignOutService&) = delete;

  ~AutoSignOutService();

 private:
  // Updates the local device info with the new sign-in time.
  void UpdateLocalDeviceInfo();

  const raw_ref<syncer::DeviceInfoSyncService> device_info_sync_service_;

  base::Time initialization_time_;

  base::CallbackListSubscription local_device_info_ready_subscription_;

  base::WeakPtrFactory<AutoSignOutService> weak_pointer_factory_{this};
};

}  // namespace ash

#endif  // CHROMEOS_ASH_COMPONENTS_AUTO_SIGN_OUT_AUTO_SIGN_OUT_SERVICE_H_
