// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/services/sharing/nearby/test_support/fake_adapter.h"

#include <memory>

#include "base/containers/contains.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace bluetooth {

namespace {

class FakeAdvertisement : public mojom::Advertisement {
 public:
  explicit FakeAdvertisement(base::OnceClosure on_destroy_callback)
      : on_destroy_callback_(std::move(on_destroy_callback)) {}
  ~FakeAdvertisement() override { std::move(on_destroy_callback_).Run(); }

 private:
  // mojom::Advertisement:
  void Unregister(UnregisterCallback callback) override {
    std::move(callback).Run();
  }

  base::OnceClosure on_destroy_callback_;
};

class FakeDiscoverySession : public mojom::DiscoverySession {
 public:
  explicit FakeDiscoverySession(base::OnceClosure on_destroy_callback)
      : on_destroy_callback_(std::move(on_destroy_callback)) {}
  ~FakeDiscoverySession() override { std::move(on_destroy_callback_).Run(); }

 private:
  // mojom::DiscoverySession:
  void IsActive(IsActiveCallback callback) override {
    std::move(callback).Run(true);
  }
  void Stop(StopCallback callback) override { std::move(callback).Run(true); }

  base::OnceClosure on_destroy_callback_;
};

class FakeSocket : public mojom::Socket {
 public:
  FakeSocket(mojo::ScopedDataPipeProducerHandle receive_stream,
             mojo::ScopedDataPipeConsumerHandle send_stream)
      : receive_stream_(std::move(receive_stream)),
        send_stream_(std::move(send_stream)) {}
  ~FakeSocket() override = default;

 private:
  // mojom::Socket:
  void Disconnect(DisconnectCallback callback) override {
    std::move(callback).Run();
  }

  mojo::ScopedDataPipeProducerHandle receive_stream_;
  mojo::ScopedDataPipeConsumerHandle send_stream_;
};

class FakeServerSocket : public mojom::ServerSocket {
 public:
  FakeServerSocket() = default;
  ~FakeServerSocket() override = default;

 private:
  // mojom::ServerSocket:
  void Accept(AcceptCallback callback) override {
    std::move(callback).Run(/*result=*/nullptr);
  }
  void Disconnect(DisconnectCallback callback) override {
    std::move(callback).Run();
  }
};

}  // namespace

FakeAdapter::FakeAdapter() = default;

FakeAdapter::~FakeAdapter() = default;

void FakeAdapter::ConnectToDevice(const std::string& address,
                                  ConnectToDeviceCallback callback) {
  if (connect_to_device_result_ == bluetooth::mojom::ConnectResult::SUCCESS) {
    mojo::PendingRemote<mojom::Device> pending_device;
    mojo::MakeSelfOwnedReceiver(
        std::move(fake_device_),
        pending_device.InitWithNewPipeAndPassReceiver());
    std::move(callback).Run(connect_to_device_result_,
                            std::move(pending_device));
    return;
  }

  std::move(callback).Run(connect_to_device_result_, mojo::NullRemote());
}

void FakeAdapter::GetDevices(GetDevicesCallback callback) {}

void FakeAdapter::GetInfo(GetInfoCallback callback) {
  mojom::AdapterInfoPtr adapter_info = mojom::AdapterInfo::New();
  adapter_info->address = address_;
  adapter_info->name = name_;
  adapter_info->extended_advertisement_support =
      extended_advertisement_support_;
  adapter_info->present = present_;
  adapter_info->powered = powered_;
  adapter_info->discoverable = discoverable_;
  adapter_info->discovering = discovering_;
  std::move(callback).Run(std::move(adapter_info));
}

void FakeAdapter::AddObserver(
    mojo::PendingRemote<mojom::AdapterObserver> observer,
    AddObserverCallback callback) {
  observers_.Add(std::move(observer));
  std::move(callback).Run();
}

void FakeAdapter::RegisterAdvertisement(
    const device::BluetoothUUID& service_uuid,
    const std::vector<uint8_t>& service_data,
    bool use_scan_response,
    bool connectable,
    RegisterAdvertisementCallback callback) {
  if (!should_advertisement_registration_succeed_) {
    std::move(callback).Run(mojo::NullRemote());
    return;
  }

  registered_advertisements_map_.insert({service_uuid, service_data});

  auto advertisement = std::make_unique<FakeAdvertisement>(
      base::BindOnce(&FakeAdapter::OnAdvertisementDestroyed,
                     base::Unretained(this), service_uuid));

  mojo::PendingRemote<mojom::Advertisement> pending_advertisement;
  mojo::MakeSelfOwnedReceiver(
      std::move(advertisement),
      pending_advertisement.InitWithNewPipeAndPassReceiver());

  std::move(callback).Run(std::move(pending_advertisement));
}

void FakeAdapter::SetDiscoverable(bool discoverable,
                                  SetDiscoverableCallback callback) {
  discoverable_ = discoverable;
  std::move(callback).Run(/*success=*/true);
}

void FakeAdapter::SetName(const std::string& name, SetNameCallback callback) {
  name_ = name;
  std::move(callback).Run(/*success=*/true);
}

void FakeAdapter::StartDiscoverySession(
    const std::string& client_name,
    StartDiscoverySessionCallback callback) {
  DCHECK(!discovery_session_);

  if (!should_discovery_succeed_) {
    std::move(callback).Run(std::move(mojo::NullRemote()));
    return;
  }

  auto discovery_session =
      std::make_unique<FakeDiscoverySession>(base::BindOnce(
          &FakeAdapter::OnDiscoverySessionDestroyed, base::Unretained(this)));
  discovery_session_ = discovery_session.get();

  mojo::PendingRemote<mojom::DiscoverySession> pending_session;
  mojo::MakeSelfOwnedReceiver(std::move(discovery_session),
                              pending_session.InitWithNewPipeAndPassReceiver());

  std::move(callback).Run(std::move(pending_session));
}

void FakeAdapter::ConnectToServiceInsecurely(
    const std::string& address,
    const device::BluetoothUUID& service_uuid,
    bool should_unbond_on_error,
    ConnectToServiceInsecurelyCallback callback) {
  if (!base::Contains(allowed_connections_for_address_and_uuid_pair_,
                      std::make_pair(address, service_uuid))) {
    std::move(callback).Run(/*result=*/nullptr);
    return;
  }

  mojo::ScopedDataPipeProducerHandle receive_pipe_producer_handle;
  mojo::ScopedDataPipeConsumerHandle receive_pipe_consumer_handle;
  ASSERT_EQ(MOJO_RESULT_OK, mojo::CreateDataPipe(/*options=*/nullptr,
                                                 receive_pipe_producer_handle,
                                                 receive_pipe_consumer_handle));

  mojo::ScopedDataPipeProducerHandle send_pipe_producer_handle;
  mojo::ScopedDataPipeConsumerHandle send_pipe_consumer_handle;
  ASSERT_EQ(MOJO_RESULT_OK,
            mojo::CreateDataPipe(/*options=*/nullptr, send_pipe_producer_handle,
                                 send_pipe_consumer_handle));

  mojo::PendingRemote<mojom::Socket> pending_socket;

  mojo::MakeSelfOwnedReceiver(
      std::make_unique<FakeSocket>(std::move(receive_pipe_producer_handle),
                                   std::move(send_pipe_consumer_handle)),
      pending_socket.InitWithNewPipeAndPassReceiver());

  mojom::ConnectToServiceResultPtr connect_to_service_result =
      mojom::ConnectToServiceResult::New();
  connect_to_service_result->socket = std::move(pending_socket);
  connect_to_service_result->receive_stream =
      std::move(receive_pipe_consumer_handle);
  connect_to_service_result->send_stream = std::move(send_pipe_producer_handle);
  std::move(callback).Run(std::move(connect_to_service_result));
}

void FakeAdapter::CreateRfcommServiceInsecurely(
    const std::string& service_name,
    const device::BluetoothUUID& service_uuid,
    CreateRfcommServiceInsecurelyCallback callback) {
  if (!base::Contains(allowed_connections_for_service_name_and_uuid_pair_,
                      std::make_pair(service_name, service_uuid))) {
    std::move(callback).Run(/*server_socket=*/mojo::NullRemote());
    return;
  }

  mojo::PendingRemote<mojom::ServerSocket> pending_server_socket;

  mojo::MakeSelfOwnedReceiver(
      std::make_unique<FakeServerSocket>(),
      pending_server_socket.InitWithNewPipeAndPassReceiver());

  std::move(callback).Run(std::move(pending_server_socket));
}

void FakeAdapter::CreateLocalGattService(
    const device::BluetoothUUID& service_id,
    mojo::PendingRemote<mojom::GattServiceObserver> observer,
    CreateLocalGattServiceCallback callback) {
  mojo::PendingRemote<mojom::GattService> pending_gatt_service;
  fake_gatt_service_->SetObserver(std::move(observer));
  gatt_service_receiver_ = mojo::MakeSelfOwnedReceiver(
      std::move(fake_gatt_service_),
      pending_gatt_service.InitWithNewPipeAndPassReceiver());
  std::move(callback).Run(std::move(pending_gatt_service));

  if (create_local_gatt_service_callback_) {
    std::move(create_local_gatt_service_callback_).Run();
  }
}

void FakeAdapter::SetShouldAdvertisementRegistrationSucceed(
    bool should_advertisement_registration_succeed) {
  should_advertisement_registration_succeed_ =
      should_advertisement_registration_succeed;
}

void FakeAdapter::IsLeScatternetDualRoleSupported(
    IsLeScatternetDualRoleSupportedCallback callback) {
  std::move(callback).Run(is_dual_role_supported_);
}

void FakeAdapter::SetShouldDiscoverySucceed(bool should_discovery_succeed) {
  should_discovery_succeed_ = should_discovery_succeed;
}

void FakeAdapter::SetAdvertisementDestroyedCallback(
    base::OnceClosure callback) {
  on_advertisement_destroyed_callback_ = std::move(callback);
}

void FakeAdapter::SetCreateLocalGattServiceCallback(
    base::OnceClosure callback) {
  create_local_gatt_service_callback_ = std::move(callback);
}

void FakeAdapter::SetCreateLocalGattServiceResult(
    std::unique_ptr<FakeGattService> fake_gatt_service) {
  fake_gatt_service_ = std::move(fake_gatt_service);
}

void FakeAdapter::SetExtendedAdvertisementSupport(
    bool extended_advertisement_support) {
  extended_advertisement_support_ = extended_advertisement_support;
}

const std::vector<uint8_t>* FakeAdapter::GetRegisteredAdvertisementServiceData(
    const device::BluetoothUUID& service_uuid) {
  auto it = registered_advertisements_map_.find(service_uuid);
  return it == registered_advertisements_map_.end() ? nullptr : &it->second;
}

void FakeAdapter::SetDiscoverySessionDestroyedCallback(
    base::OnceClosure callback) {
  on_discovery_session_destroyed_callback_ = std::move(callback);
}

bool FakeAdapter::IsDiscoverySessionActive() {
  return discovery_session_;
}

void FakeAdapter::NotifyDeviceAdded(mojom::DeviceInfoPtr device_info) {
  for (auto& observer : observers_)
    observer->DeviceAdded(device_info->Clone());
}

void FakeAdapter::NotifyDeviceChanged(mojom::DeviceInfoPtr device_info) {
  for (auto& observer : observers_)
    observer->DeviceChanged(device_info->Clone());
}

void FakeAdapter::NotifyDeviceRemoved(mojom::DeviceInfoPtr device_info) {
  for (auto& observer : observers_)
    observer->DeviceRemoved(device_info->Clone());
}

void FakeAdapter::AllowConnectionForAddressAndUuidPair(
    const std::string& address,
    const device::BluetoothUUID& service_uuid) {
  allowed_connections_for_address_and_uuid_pair_.emplace(address, service_uuid);
}

void FakeAdapter::AllowIncomingConnectionForServiceNameAndUuidPair(
    const std::string& service_name,
    const device::BluetoothUUID& service_uuid) {
  allowed_connections_for_service_name_and_uuid_pair_.emplace(service_name,
                                                              service_uuid);
}

void FakeAdapter::SetConnectToDeviceResult(
    bluetooth::mojom::ConnectResult result,
    std::unique_ptr<FakeDevice> fake_device) {
  connect_to_device_result_ = result;
  fake_device_ = std::move(fake_device);
}

void FakeAdapter::OnAdvertisementDestroyed(
    const device::BluetoothUUID& service_uuid) {
  DCHECK(!registered_advertisements_map_.empty());
  registered_advertisements_map_.erase(service_uuid);
  if (on_advertisement_destroyed_callback_)
    std::move(on_advertisement_destroyed_callback_).Run();
}

void FakeAdapter::OnDiscoverySessionDestroyed() {
  DCHECK(discovery_session_);
  discovery_session_ = nullptr;
  if (on_discovery_session_destroyed_callback_)
    std::move(on_discovery_session_destroyed_callback_).Run();
}

}  // namespace bluetooth
