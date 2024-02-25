// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/services/secure_channel/secure_channel_initializer.h"

#include "base/functional/bind.h"
#include "base/memory/ptr_util.h"
#include "chromeos/ash/components/multidevice/logging/logging.h"
#include "chromeos/ash/services/secure_channel/secure_channel_impl.h"
#include "device/bluetooth/bluetooth_adapter_factory.h"
#include "device/bluetooth/dbus/bluez_dbus_manager.h"
#include "device/bluetooth/floss/floss_dbus_manager.h"
#include "device/bluetooth/floss/floss_features.h"
#include "mojo/public/cpp/bindings/pending_remote.h"

namespace ash::secure_channel {

// static
SecureChannelInitializer::Factory*
    SecureChannelInitializer::Factory::test_factory_ = nullptr;

// static
std::unique_ptr<SecureChannelBase> SecureChannelInitializer::Factory::Create(
    scoped_refptr<base::TaskRunner> task_runner) {
  if (test_factory_)
    return test_factory_->CreateInstance(std::move(task_runner));

  return base::WrapUnique(new SecureChannelInitializer(std::move(task_runner)));
}

// static
void SecureChannelInitializer::Factory::SetFactoryForTesting(
    Factory* test_factory) {
  test_factory_ = test_factory;
}

SecureChannelInitializer::Factory::~Factory() = default;

SecureChannelInitializer::ConnectionRequestArgs::ConnectionRequestArgs(
    const multidevice::RemoteDevice& device_to_connect,
    const multidevice::RemoteDevice& local_device,
    const std::string& feature,
    ConnectionMedium connection_medium,
    ConnectionPriority connection_priority,
    mojo::PendingRemote<mojom::ConnectionDelegate> delegate,
    mojo::PendingRemote<mojom::SecureChannelStructuredMetricsLogger>
        secure_channel_structured_metrics_logger,
    bool is_listen_request)
    : device_to_connect(device_to_connect),
      local_device(local_device),
      feature(feature),
      connection_medium(connection_medium),
      connection_priority(connection_priority),
      delegate(std::move(delegate)),
      secure_channel_structured_metrics_logger(
          std::move(secure_channel_structured_metrics_logger)),
      is_listen_request(is_listen_request) {}

SecureChannelInitializer::ConnectionRequestArgs::~ConnectionRequestArgs() =
    default;

SecureChannelInitializer::SecureChannelInitializer(
    scoped_refptr<base::TaskRunner> task_runner) {
  bool is_initialized = false;
  if (floss::features::IsFlossEnabled()) {
    is_initialized = floss::FlossDBusManager::IsInitialized();
  } else {
    is_initialized = bluez::BluezDBusManager::IsInitialized();
  }

  // May not be initialized in tests.
  if (!is_initialized)
    return;

  PA_LOG(VERBOSE) << "SecureChannelInitializer::SecureChannelInitializer(): "
                  << "Fetching Bluetooth adapter. All requests received before "
                  << "the adapter is fetched will be queued.";

  // device::BluetoothAdapterFactory::SetAdapterForTesting() causes the
  // GetAdapter() callback to return synchronously. Thus, post the GetAdapter()
  // call as a task to ensure that it is returned asynchronously, even in tests.
  auto* factory = device::BluetoothAdapterFactory::Get();
  task_runner->PostTask(
      FROM_HERE,
      base::BindOnce(
          &device::BluetoothAdapterFactory::GetAdapter,
          base::Unretained(factory),
          base::BindOnce(&SecureChannelInitializer::OnBluetoothAdapterReceived,
                         weak_ptr_factory_.GetWeakPtr())));
}

SecureChannelInitializer::~SecureChannelInitializer() = default;

void SecureChannelInitializer::ListenForConnectionFromDevice(
    const multidevice::RemoteDevice& device_to_connect,
    const multidevice::RemoteDevice& local_device,
    const std::string& feature,
    ConnectionMedium connection_medium,
    ConnectionPriority connection_priority,
    mojo::PendingRemote<mojom::ConnectionDelegate> delegate) {
  if (secure_channel_impl_) {
    secure_channel_impl_->ListenForConnectionFromDevice(
        device_to_connect, local_device, feature, connection_medium,
        connection_priority, std::move(delegate));
    return;
  }

  pending_args_.push(std::make_unique<ConnectionRequestArgs>(
      device_to_connect, local_device, feature, connection_medium,
      connection_priority, std::move(delegate), mojo::NullRemote(),
      true /* is_listen_request */));
}

void SecureChannelInitializer::InitiateConnectionToDevice(
    const multidevice::RemoteDevice& device_to_connect,
    const multidevice::RemoteDevice& local_device,
    const std::string& feature,
    ConnectionMedium connection_medium,
    ConnectionPriority connection_priority,
    mojo::PendingRemote<mojom::ConnectionDelegate> delegate,
    mojo::PendingRemote<mojom::SecureChannelStructuredMetricsLogger>
        secure_channel_structured_metrics_logger) {
  if (secure_channel_impl_) {
    secure_channel_impl_->InitiateConnectionToDevice(
        device_to_connect, local_device, feature, connection_medium,
        connection_priority, std::move(delegate),
        std::move(secure_channel_structured_metrics_logger));
    return;
  }

  pending_args_.push(std::make_unique<ConnectionRequestArgs>(
      device_to_connect, local_device, feature, connection_medium,
      connection_priority, std::move(delegate),
      std::move(secure_channel_structured_metrics_logger),
      false /* is_listen_request */));
}
void SecureChannelInitializer::GetLastSeenTimestamp(
    const std::string& remote_device_id,
    base::OnceCallback<void(std::optional<base::Time>)> callback) {
  if (secure_channel_impl_) {
    secure_channel_impl_->GetLastSeenTimestamp(remote_device_id,
                                               std::move(callback));
  }
}

void SecureChannelInitializer::SetNearbyConnector(
    mojo::PendingRemote<mojom::NearbyConnector> nearby_connector) {
  if (secure_channel_impl_) {
    secure_channel_impl_->SetNearbyConnector(std::move(nearby_connector));
    return;
  }

  nearby_connector_ = std::move(nearby_connector);
}

void SecureChannelInitializer::OnBluetoothAdapterReceived(
    scoped_refptr<device::BluetoothAdapter> bluetooth_adapter) {
  PA_LOG(VERBOSE) << "SecureChannelInitializer::OnBluetoothAdapterReceived(): "
                  << "Bluetooth adapter has been fetched. Passing all queued "
                  << "requests to the service.";

  secure_channel_impl_ = SecureChannelImpl::Factory::Create(bluetooth_adapter);

  if (nearby_connector_)
    secure_channel_impl_->SetNearbyConnector(std::move(nearby_connector_));

  while (!pending_args_.empty()) {
    std::unique_ptr<ConnectionRequestArgs> args_to_pass =
        std::move(pending_args_.front());
    pending_args_.pop();

    if (args_to_pass->is_listen_request) {
      secure_channel_impl_->ListenForConnectionFromDevice(
          args_to_pass->device_to_connect, args_to_pass->local_device,
          args_to_pass->feature, args_to_pass->connection_medium,
          args_to_pass->connection_priority, std::move(args_to_pass->delegate));
      continue;
    }

    secure_channel_impl_->InitiateConnectionToDevice(
        args_to_pass->device_to_connect, args_to_pass->local_device,
        args_to_pass->feature, args_to_pass->connection_medium,
        args_to_pass->connection_priority, std::move(args_to_pass->delegate),
        std::move(args_to_pass->secure_channel_structured_metrics_logger));
  }
}

}  // namespace ash::secure_channel
