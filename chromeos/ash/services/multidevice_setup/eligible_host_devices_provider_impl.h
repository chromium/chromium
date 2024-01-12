// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_SERVICES_MULTIDEVICE_SETUP_ELIGIBLE_HOST_DEVICES_PROVIDER_IMPL_H_
#define CHROMEOS_ASH_SERVICES_MULTIDEVICE_SETUP_ELIGIBLE_HOST_DEVICES_PROVIDER_IMPL_H_

#include "base/memory/raw_ptr.h"
#include "chromeos/ash/components/multidevice/remote_device_ref.h"
#include "chromeos/ash/services/device_sync/public/cpp/device_sync_client.h"
#include "chromeos/ash/services/multidevice_setup/eligible_host_devices_provider.h"

namespace ash {

namespace multidevice_setup {

// Concrete EligibleHostDevicesProvider implementation, which utilizes
// DeviceSyncClient to fetch devices.
class EligibleHostDevicesProviderImpl
    : public EligibleHostDevicesProvider,
      public device_sync::DeviceSyncClient::Observer {
 public:
  static constexpr base::TimeDelta kInactiveDeviceThresholdInDays =
      base::Days(30);

  class Factory {
   public:
    static std::unique_ptr<EligibleHostDevicesProvider> Create(
        device_sync::DeviceSyncClient* device_sync_client);
    static void SetFactoryForTesting(Factory* factory);

   protected:
    virtual ~Factory();
    virtual std::unique_ptr<EligibleHostDevicesProvider> CreateInstance(
        device_sync::DeviceSyncClient* device_sync_client) = 0;

   private:
    static Factory* test_factory_;
  };

  EligibleHostDevicesProviderImpl(const EligibleHostDevicesProviderImpl&) =
      delete;
  EligibleHostDevicesProviderImpl& operator=(
      const EligibleHostDevicesProviderImpl&) = delete;

  ~EligibleHostDevicesProviderImpl() override;

 private:
  explicit EligibleHostDevicesProviderImpl(
      device_sync::DeviceSyncClient* device_sync_client);

  // EligibleHostDevicesProvider:
  multidevice::RemoteDeviceRefList GetEligibleHostDevices() const override;
  multidevice::DeviceWithConnectivityStatusList GetEligibleActiveHostDevices()
      const override;

  // device_sync::DeviceSyncClient::Observer:
  void OnNewDevicesSynced() override;

  void UpdateEligibleDevicesSet();

  void OnGetDevicesActivityStatus(
      device_sync::mojom::NetworkRequestResult,
      std::optional<std::vector<device_sync::mojom::DeviceActivityStatusPtr>>);

  raw_ptr<device_sync::DeviceSyncClient> device_sync_client_;

  multidevice::RemoteDeviceRefList eligible_devices_from_last_sync_;
  multidevice::DeviceWithConnectivityStatusList
      eligible_active_devices_from_last_sync_;
};

}  // namespace multidevice_setup

}  // namespace ash

#endif  // CHROMEOS_ASH_SERVICES_MULTIDEVICE_SETUP_ELIGIBLE_HOST_DEVICES_PROVIDER_IMPL_H_
