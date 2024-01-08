// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_SERVICES_MULTIDEVICE_SETUP_GRANDFATHERED_EASY_UNLOCK_HOST_DISABLER_H_
#define CHROMEOS_ASH_SERVICES_MULTIDEVICE_SETUP_GRANDFATHERED_EASY_UNLOCK_HOST_DISABLER_H_

#include <memory>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chromeos/ash/services/device_sync/public/mojom/device_sync.mojom.h"
#include "chromeos/ash/services/multidevice_setup/host_backend_delegate.h"

class PrefRegistrySimple;
class PrefService;

namespace base {
class OneShotTimer;
}  // namespace base

namespace ash {

namespace device_sync {
class DeviceSyncClient;
}

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
//                           +--- OnDisableEasyUnlockResult ------------------+
class GrandfatheredEasyUnlockHostDisabler
    : public HostBackendDelegate::Observer {
 public:
  class Factory {
   public:
    static std::unique_ptr<GrandfatheredEasyUnlockHostDisabler> Create(
        HostBackendDelegate* host_backend_delegate,
        device_sync::DeviceSyncClient* device_sync_client,
        PrefService* pref_service,
        std::unique_ptr<base::OneShotTimer> timer =
            std::make_unique<base::OneShotTimer>());
    static void SetFactoryForTesting(Factory* test_factory);

   protected:
    virtual ~Factory();
    virtual std::unique_ptr<GrandfatheredEasyUnlockHostDisabler> CreateInstance(
        HostBackendDelegate* host_backend_delegate,
        device_sync::DeviceSyncClient* device_sync_client,
        PrefService* pref_service,
        std::unique_ptr<base::OneShotTimer> timer) = 0;

   private:
    static Factory* test_factory_;
  };

  static void RegisterPrefs(PrefRegistrySimple* registry);

  GrandfatheredEasyUnlockHostDisabler(
      const GrandfatheredEasyUnlockHostDisabler&) = delete;
  GrandfatheredEasyUnlockHostDisabler& operator=(
      const GrandfatheredEasyUnlockHostDisabler&) = delete;

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
  void OnDisableEasyUnlockHostResult(
      multidevice::RemoteDeviceRef device,
      device_sync::mojom::NetworkRequestResult result_code);
  void SetPotentialEasyUnlockHostToDisable(
      std::optional<multidevice::RemoteDeviceRef> device);
  std::optional<multidevice::RemoteDeviceRef> GetEasyUnlockHostToDisable();

  raw_ptr<HostBackendDelegate> host_backend_delegate_;
  raw_ptr<device_sync::DeviceSyncClient> device_sync_client_;
  raw_ptr<PrefService> pref_service_;
  std::unique_ptr<base::OneShotTimer> timer_;
  std::optional<multidevice::RemoteDeviceRef> current_better_together_host_;

  base::WeakPtrFactory<GrandfatheredEasyUnlockHostDisabler> weak_ptr_factory_{
      this};
};

}  // namespace multidevice_setup

}  // namespace ash

#endif  // CHROMEOS_ASH_SERVICES_MULTIDEVICE_SETUP_GRANDFATHERED_EASY_UNLOCK_HOST_DISABLER_H_
