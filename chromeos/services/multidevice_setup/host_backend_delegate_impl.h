// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_SERVICES_MULTIDEVICE_SETUP_HOST_BACKEND_DELEGATE_IMPL_H_
#define CHROMEOS_SERVICES_MULTIDEVICE_SETUP_HOST_BACKEND_DELEGATE_IMPL_H_

#include "base/callback_forward.h"
#include "base/containers/flat_map.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/optional.h"
#include "base/timer/timer.h"
#include "chromeos/components/multidevice/remote_device_ref.h"
#include "chromeos/services/device_sync/public/cpp/device_sync_client.h"
#include "chromeos/services/multidevice_setup/host_backend_delegate.h"

class PrefRegistrySimple;
class PrefService;

namespace chromeos {

namespace multidevice_setup {

class EligibleHostDevicesProvider;

// Concrete HostBackendDelegate implementation, which utilizes
// DeviceSyncClient to communicate with the back-end.
class HostBackendDelegateImpl : public HostBackendDelegate,
                                public device_sync::DeviceSyncClient::Observer {
 public:
  class Factory {
   public:
    static Factory* Get();
    static void SetFactoryForTesting(Factory* test_factory);
    virtual ~Factory();
    virtual std::unique_ptr<HostBackendDelegate> BuildInstance(
        EligibleHostDevicesProvider* eligible_host_devices_provider,
        PrefService* pref_service,
        device_sync::DeviceSyncClient* device_sync_client,
        std::unique_ptr<base::OneShotTimer> timer =
            std::make_unique<base::OneShotTimer>());

   private:
    static Factory* test_factory_;
  };

  static void RegisterPrefs(PrefRegistrySimple* registry);

  ~HostBackendDelegateImpl() override;

 private:
  HostBackendDelegateImpl(
      EligibleHostDevicesProvider* eligible_host_devices_provider,
      PrefService* pref_service,
      device_sync::DeviceSyncClient* device_sync_client,
      std::unique_ptr<base::OneShotTimer> timer);

  // HostBackendDelegate:
  void AttemptToSetMultiDeviceHostOnBackend(
      const base::Optional<multidevice::RemoteDeviceRef>& host_device) override;
  bool HasPendingHostRequest() override;
  base::Optional<multidevice::RemoteDeviceRef> GetPendingHostRequest()
      const override;
  base::Optional<multidevice::RemoteDeviceRef> GetMultiDeviceHostFromBackend()
      const override;

  // DeviceSyncClient::Observer:
  void OnNewDevicesSynced() override;

  bool IsHostEligible(const multidevice::RemoteDeviceRef& provided_host);

  // Sets the pending host request. To signal that the request is to remove the
  // current host, pass kPendingRemovalOfCurrentHost. To signal that there is no
  // pending request, pass kNoPendingRequest.
  void SetPendingHostRequest(const std::string& host_device_id);

  void AttemptNetworkRequest(bool is_retry);
  base::Optional<multidevice::RemoteDeviceRef> GetHostFromDeviceSync();
  void OnSetSoftwareFeatureStateResult(
      multidevice::RemoteDeviceRef device_for_request,
      bool attempted_to_enable,
      device_sync::mojom::NetworkRequestResult result_code);

  EligibleHostDevicesProvider* eligible_host_devices_provider_;
  PrefService* pref_service_;
  device_sync::DeviceSyncClient* device_sync_client_;
  std::unique_ptr<base::OneShotTimer> timer_;

  // The most-recent snapshot of the host on the back-end.
  base::Optional<multidevice::RemoteDeviceRef> host_from_last_sync_;

  base::WeakPtrFactory<HostBackendDelegateImpl> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(HostBackendDelegateImpl);
};

}  // namespace multidevice_setup

}  // namespace chromeos

#endif  // CHROMEOS_SERVICES_MULTIDEVICE_SETUP_HOST_BACKEND_DELEGATE_IMPL_H_
