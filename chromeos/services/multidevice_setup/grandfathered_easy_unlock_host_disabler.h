// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_SERVICES_MULTIDEVICE_SETUP_GRANDFATHERED_EASY_UNLOCK_HOST_DISABLER_H_
#define CHROMEOS_SERVICES_MULTIDEVICE_SETUP_GRANDFATHERED_EASY_UNLOCK_HOST_DISABLER_H_

#include <memory>

#include "base/macros.h"
#include "chromeos/services/device_sync/public/mojom/device_sync.mojom.h"
#include "chromeos/services/multidevice_setup/host_backend_delegate.h"

class PrefRegistrySimple;
class PrefService;

namespace base {
class OneShotTimer;
}  // namespace base

namespace chromeos {

namespace device_sync {
class DeviceSyncClient;
}  // namespace device_sync

namespace multidevice_setup {

// This class watches for BETTER_TOGETHER_HOST to be disabled on a device and
// reacts by also disabling EASY_UNLOCK_HOST on that device. See
// https://crbug.com/881612.
//
// The flow of the class is as follows:
//
// Constructor --------------+
//                           |
//                           V                              (no host to disable)
// OnHostChangedOnBackend -->+---> DisableEasyUnlockHostIfNecessary ------> Done
//                           ^                |                               ^
//                           |                |                               |
//             (retry timer) |                |                               |
//                           |                V                    (success)  |
//                           +--- OnSetSoftwareFeatureStateResult ------------+
class GrandfatheredEasyUnlockHostDisabler
    : public HostBackendDelegate::Observer {
 public:
  class Factory {
   public:
    static Factory* Get();
    static void SetFactoryForTesting(Factory* test_factory);
    virtual ~Factory();
    virtual std::unique_ptr<GrandfatheredEasyUnlockHostDisabler> BuildInstance(
        HostBackendDelegate* host_backend_delegate,
        device_sync::DeviceSyncClient* device_sync_client,
        PrefService* pref_service,
        std::unique_ptr<base::OneShotTimer> timer =
            std::make_unique<base::OneShotTimer>());

   private:
    static Factory* test_factory_;
  };

  static void RegisterPrefs(PrefRegistrySimple* registry);

  ~GrandfatheredEasyUnlockHostDisabler() override;

 private:
  GrandfatheredEasyUnlockHostDisabler(
      HostBackendDelegate* host_backend_delegate,
      device_sync::DeviceSyncClient* device_sync_client,
      PrefService* pref_service,
      std::unique_ptr<base::OneShotTimer> timer);

  // HostBackendDelegate::Observer:
  void OnHostChangedOnBackend() override;

  void DisableEasyUnlockHostIfNecessary();
  void OnSetSoftwareFeatureStateResult(
      cryptauth::RemoteDeviceRef device,
      device_sync::mojom::NetworkRequestResult result_code);
  void SetPotentialEasyUnlockHostToDisable(
      base::Optional<cryptauth::RemoteDeviceRef> device);
  base::Optional<cryptauth::RemoteDeviceRef> GetEasyUnlockHostToDisable();

  HostBackendDelegate* host_backend_delegate_;
  device_sync::DeviceSyncClient* device_sync_client_;
  PrefService* pref_service_;
  std::unique_ptr<base::OneShotTimer> timer_;
  base::Optional<cryptauth::RemoteDeviceRef> current_better_together_host_;

  DISALLOW_COPY_AND_ASSIGN(GrandfatheredEasyUnlockHostDisabler);
};

}  // namespace multidevice_setup

}  // namespace chromeos

#endif
