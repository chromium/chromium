// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/services/secure_channel/secure_channel_initializer.h"

#include "base/bind.h"
#include "base/memory/ptr_util.h"
#include "base/no_destructor.h"
#include "chromeos/components/multidevice/logging/logging.h"
#include "chromeos/services/secure_channel/secure_channel_impl.h"
#include "device/bluetooth/bluetooth_adapter_factory.h"

namespace chromeos {

namespace secure_channel {

// static
SecureChannelInitializer::Factory*
    SecureChannelInitializer::Factory::test_factory_ = nullptr;

// static
SecureChannelInitializer::Factory* SecureChannelInitializer::Factory::Get() {
  if (test_factory_)
    return test_factory_;

  static base::NoDestructor<Factory> factory;
  return factory.get();
}

// static
void SecureChannelInitializer::Factory::SetFactoryForTesting(
    Factory* test_factory) {
  test_factory_ = test_factory;
}

SecureChannelInitializer::Factory::~Factory() = default;

std::unique_ptr<SecureChannelBase>
SecureChannelInitializer::Factory::BuildInstance(
    scoped_refptr<base::TaskRunner> task_runner) {
  return base::WrapUnique(new SecureChannelInitializer(task_runner));
}

SecureChannelInitializer::ConnectionRequestArgs::ConnectionRequestArgs(
    const multidevice::RemoteDevice& device_to_connect,
    const multidevice::RemoteDevice& local_device,
    const std::string& feature,
    ConnectionPriority connection_priority,
    mojo::PendingRemote<mojom::ConnectionDelegate> delegate,
    bool is_listen_request)
    : device_to_connect(device_to_connect),
      local_device(local_device),
      feature(feature),
      connection_priority(connection_priority),
      delegate(std::move(delegate)),
      is_listen_request(is_listen_request) {}

SecureChannelInitializer::ConnectionRequestArgs::~ConnectionRequestArgs() =
    default;

SecureChannelInitializer::SecureChannelInitializer(
    scoped_refptr<base::TaskRunner> task_runner) {
  PA_LOG(VERBOSE) << "SecureChannelInitializer::SecureChannelInitializer(): "
                  << "Fetching Bluetooth adapter. All requests received before "
                  << "the adapter is fetched will be queued.";

  // device::BluetoothAdapterFactory::SetAdapterForTesting() causes the
  // GetAdapter() callback to return synchronously. Thus, post the GetAdapter()
  // call as a task to ensure that it is returned asynchronously, even in tests.
  task_runner->PostTask(
      FROM_HERE,
      base::BindOnce(
          device::BluetoothAdapterFactory::GetAdapter,
          base::Bind(&SecureChannelInitializer::OnBluetoothAdapterReceived,
                     weak_ptr_factory_.GetWeakPtr())));
}

SecureChannelInitializer::~SecureChannelInitializer() = default;

void SecureChannelInitializer::ListenForConnectionFromDevice(
    const multidevice::RemoteDevice& device_to_connect,
    const multidevice::RemoteDevice& local_device,
    const std::string& feature,
    ConnectionPriority connection_priority,
    mojo::PendingRemote<mojom::ConnectionDelegate> delegate) {
  if (secure_channel_impl_) {
    secure_channel_impl_->ListenForConnectionFromDevice(
        device_to_connect, local_device, feature, connection_priority,
        std::move(delegate));
    return;
  }

  pending_args_.push(std::make_unique<ConnectionRequestArgs>(
      device_to_connect, local_device, feature, connection_priority,
      std::move(delegate), true /* is_listen_request */));
}

void SecureChannelInitializer::InitiateConnectionToDevice(
    const multidevice::RemoteDevice& device_to_connect,
    const multidevice::RemoteDevice& local_device,
    const std::string& feature,
    ConnectionPriority connection_priority,
    mojo::PendingRemote<mojom::ConnectionDelegate> delegate) {
  if (secure_channel_impl_) {
    secure_channel_impl_->InitiateConnectionToDevice(
        device_to_connect, local_device, feature, connection_priority,
        std::move(delegate));
    return;
  }

  pending_args_.push(std::make_unique<ConnectionRequestArgs>(
      device_to_connect, local_device, feature, connection_priority,
      std::move(delegate), false /* is_listen_request */));
}

void SecureChannelInitializer::OnBluetoothAdapterReceived(
    scoped_refptr<device::BluetoothAdapter> bluetooth_adapter) {
  PA_LOG(VERBOSE) << "SecureChannelInitializer::OnBluetoothAdapterReceived(): "
                  << "Bluetooth adapter has been fetched. Passing all queued "
                  << "requests to the service.";

  secure_channel_impl_ =
      SecureChannelImpl::Factory::Get()->BuildInstance(bluetooth_adapter);

  while (!pending_args_.empty()) {
    std::unique_ptr<ConnectionRequestArgs> args_to_pass =
        std::move(pending_args_.front());
    pending_args_.pop();

    if (args_to_pass->is_listen_request) {
      secure_channel_impl_->ListenForConnectionFromDevice(
          args_to_pass->device_to_connect, args_to_pass->local_device,
          args_to_pass->feature, args_to_pass->connection_priority,
          std::move(args_to_pass->delegate));
      continue;
    }

    secure_channel_impl_->InitiateConnectionToDevice(
        args_to_pass->device_to_connect, args_to_pass->local_device,
        args_to_pass->feature, args_to_pass->connection_priority,
        std::move(args_to_pass->delegate));
  }
}

}  // namespace secure_channel

}  // namespace chromeos
