// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_SERVICES_SHARING_NEARBY_TEST_SUPPORT_FAKE_ADAPTER_H_
#define CHROME_SERVICES_SHARING_NEARBY_TEST_SUPPORT_FAKE_ADAPTER_H_

#include "base/memory/raw_ptr.h"
#include "chrome/services/sharing/nearby/test_support/fake_device.h"
#include "chrome/services/sharing/nearby/test_support/fake_gatt_service.h"
#include "device/bluetooth/public/cpp/bluetooth_uuid.h"
#include "device/bluetooth/public/mojom/adapter.mojom.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote_set.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"

namespace bluetooth {

class FakeAdapter : public mojom::Adapter {
 public:
  FakeAdapter();
  FakeAdapter(const FakeAdapter&) = delete;
  FakeAdapter& operator=(const FakeAdapter&) = delete;
  ~FakeAdapter() override;

  // mojom::Adapter
  void ConnectToDevice(const std::string& address,
                       ConnectToDeviceCallback callback) override;
  void GetDevices(GetDevicesCallback callback) override;
  void GetInfo(GetInfoCallback callback) override;
  void AddObserver(mojo::PendingRemote<mojom::AdapterObserver> observer,
                   AddObserverCallback callback) override;
  void RegisterAdvertisement(const device::BluetoothUUID& service_uuid,
                             const std::vector<uint8_t>& service_data,
                             bool use_scan_response,
                             bool connectable,
                             RegisterAdvertisementCallback callback) override;
  void SetDiscoverable(bool discoverable,
                       SetDiscoverableCallback callback) override;
  void SetName(const std::string& name, SetNameCallback callback) override;
  void StartDiscoverySession(const std::string& client_name,
                             StartDiscoverySessionCallback callback) override;
  void ConnectToServiceInsecurely(
      const std::string& address,
      const device::BluetoothUUID& service_uuid,
      bool should_unbond_on_error,
      ConnectToServiceInsecurelyCallback callback) override;
  void CreateRfcommServiceInsecurely(
      const std::string& service_name,
      const device::BluetoothUUID& service_uuid,
      CreateRfcommServiceInsecurelyCallback callback) override;
  void CreateLocalGattService(
      const device::BluetoothUUID& service_id,
      mojo::PendingRemote<mojom::GattServiceObserver> observer,
      CreateLocalGattServiceCallback callback) override;
  void IsLeScatternetDualRoleSupported(
      IsLeScatternetDualRoleSupportedCallback callback) override;

  void SetAdvertisementDestroyedCallback(base::OnceClosure callback);
  const std::vector<uint8_t>* GetRegisteredAdvertisementServiceData(
      const device::BluetoothUUID& service_uuid);
  void SetShouldAdvertisementRegistrationSucceed(
      bool should_advertisement_registration_succeed);
  void SetShouldDiscoverySucceed(bool should_discovery_succeed);
  void SetCreateLocalGattServiceCallback(base::OnceClosure callback);
  void SetCreateLocalGattServiceResult(
      std::unique_ptr<FakeGattService> fake_gatt_service);
  void SetExtendedAdvertisementSupport(bool extended_advertisement_support);
  void SetDiscoverySessionDestroyedCallback(base::OnceClosure callback);
  bool IsDiscoverySessionActive();
  void NotifyDeviceAdded(mojom::DeviceInfoPtr device_info);
  void NotifyDeviceChanged(mojom::DeviceInfoPtr device_info);
  void NotifyDeviceRemoved(mojom::DeviceInfoPtr device_info);
  void AllowConnectionForAddressAndUuidPair(
      const std::string& address,
      const device::BluetoothUUID& service_uuid);
  void AllowIncomingConnectionForServiceNameAndUuidPair(
      const std::string& service_name,
      const device::BluetoothUUID& service_uuid);
  void SetConnectToDeviceResult(bluetooth::mojom::ConnectResult result,
                                std::unique_ptr<FakeDevice> fake_device);

  mojo::Receiver<mojom::Adapter> adapter_{this};
  std::string address_ = "AdapterAddress";
  std::string name_ = "AdapterName";
  bool extended_advertisement_support_ = false;
  bool present_ = true;
  bool powered_ = true;
  bool discoverable_ = false;
  bool discovering_ = false;
  bool is_dual_role_supported_ = false;
  mojo::SelfOwnedReceiverRef<mojom::GattService> gatt_service_receiver_;

 private:
  void OnAdvertisementDestroyed(const device::BluetoothUUID& service_uuid);
  void OnDiscoverySessionDestroyed();

  bool should_advertisement_registration_succeed_ = true;
  std::map<device::BluetoothUUID, std::vector<uint8_t>>
      registered_advertisements_map_;
  base::OnceClosure on_advertisement_destroyed_callback_;
  base::OnceClosure create_local_gatt_service_callback_;

  bool should_discovery_succeed_ = true;
  raw_ptr<mojom::DiscoverySession> discovery_session_ = nullptr;
  base::OnceClosure on_discovery_session_destroyed_callback_;

  std::set<std::pair<std::string, device::BluetoothUUID>>
      allowed_connections_for_address_and_uuid_pair_;
  std::set<std::pair<std::string, device::BluetoothUUID>>
      allowed_connections_for_service_name_and_uuid_pair_;

  std::unique_ptr<FakeGattService> fake_gatt_service_;

  bluetooth::mojom::ConnectResult connect_to_device_result_;
  std::unique_ptr<FakeDevice> fake_device_;

  mojo::RemoteSet<mojom::AdapterObserver> observers_;
};

}  // namespace bluetooth

#endif  // CHROME_SERVICES_SHARING_NEARBY_TEST_SUPPORT_FAKE_ADAPTER_H_
