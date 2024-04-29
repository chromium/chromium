// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_SERVICES_MULTIDEVICE_SETUP_HOST_BACKEND_DELEGATE_IMPL_H_
#define CHROMEOS_ASH_SERVICES_MULTIDEVICE_SETUP_HOST_BACKEND_DELEGATE_IMPL_H_

#include <optional>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/timer/timer.h"
#include "chromeos/ash/components/multidevice/remote_device_ref.h"
#include "chromeos/ash/services/device_sync/public/cpp/device_sync_client.h"
#include "chromeos/ash/services/multidevice_setup/host_backend_delegate.h"

class PrefRegistrySimple;
class PrefService;

namespace ash {

namespace multidevice_setup {

class EligibleHostDevicesProvider;

// Concrete HostBackendDelegate implementation, which utilizes
// DeviceSyncClient to communicate with the back-end.
class HostBackendDelegateImpl : public HostBackendDelegate,
                                public device_sync::DeviceSyncClient::Observer {
 public:
  class Factory {
   public:
    static std::unique_ptr<HostBackendDelegate> Create(
        EligibleHostDevicesProvider* eligible_host_devices_provider,
        PrefService* pref_service,
        device_sync::DeviceSyncClient* device_sync_client,
        std::unique_ptr<base::OneShotTimer> timer =
            std::make_unique<base::OneShotTimer>());
    static void SetFactoryForTesting(Factory* test_factory);

   protected:
    virtual ~Factory();
    virtual std::unique_ptr<HostBackendDelegate> CreateInstance(
        EligibleHostDevicesProvider* eligible_host_devices_provider,
        PrefService* pref_service,
        device_sync::DeviceSyncClient* device_sync_client,
        std::unique_ptr<base::OneShotTimer> timer) = 0;

   private:
    static Factory* test_factory_;
  };

  static void RegisterPrefs(PrefRegistrySimple* registry);

  HostBackendDelegateImpl(const HostBackendDelegateImpl&) = delete;
  HostBackendDelegateImpl& operator=(const HostBackendDelegateImpl&) = delete;

  ~HostBackendDelegateImpl() override;

 private:
  HostBackendDelegateImpl(
      EligibleHostDevicesProvider* eligible_host_devices_provider,
      PrefService* pref_service,
      device_sync::DeviceSyncClient* device_sync_client,
      std::unique_ptr<base::OneShotTimer> timer);

  // HostBackendDelegate:
  void AttemptToSetMultiDeviceHostOnBackend(
      const std::optional<multidevice::RemoteDeviceRef>& host_device) override;
  bool HasPendingHostRequest() override;
  std::optional<multidevice::RemoteDeviceRef> GetPendingHostRequest()
      const override;
  std::optional<multidevice::RemoteDeviceRef> GetMultiDeviceHostFromBackend()
      const override;

  // DeviceSyncClient::Observer:
  void OnNewDevicesSynced() override;

  bool IsHostEligible(const multidevice::RemoteDeviceRef& provided_host);

  // Sets the pending host request. To signal that the request is to remove the
  // current host, pass kPendingRemovalOfCurrentHost. To signal that there is no
  // pending request, pass kNoPendingRequest.
  void SetPendingHostRequest(const std::string& pending_host_id);

  // Returns the device with either an Instance ID or encoded public key of |id|
  // in the list of synced devices. If no such device exists, returns null.
  // TODO(crbug.com/40105247): When v1 DeviceSync is disabled, only look
  // up by Instance ID since all devices are guaranteed to have one.
  std::optional<multidevice::RemoteDeviceRef> FindDeviceById(
      const std::string& id) const;

  void AttemptNetworkRequest(bool is_retry);
  std::optional<multidevice::RemoteDeviceRef> GetHostFromDeviceSync();
  void OnSetHostNetworkRequestFinished(
      multidevice::RemoteDeviceRef device_for_request,
      bool attempted_to_enable,
      device_sync::mojom::NetworkRequestResult result_code);

  raw_ptr<EligibleHostDevicesProvider> eligible_host_devices_provider_;
  raw_ptr<PrefService> pref_service_;
  raw_ptr<device_sync::DeviceSyncClient> device_sync_client_;
  std::unique_ptr<base::OneShotTimer> timer_;

  // The most-recent snapshot of the host on the back-end.
  std::optional<multidevice::RemoteDeviceRef> host_from_last_sync_;

  base::WeakPtrFactory<HostBackendDelegateImpl> weak_ptr_factory_{this};
};

}  // namespace multidevice_setup

}  // namespace ash

#endif  // CHROMEOS_ASH_SERVICES_MULTIDEVICE_SETUP_HOST_BACKEND_DELEGATE_IMPL_H_
