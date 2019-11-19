// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/services/secure_channel/public/cpp/client/secure_channel_client_impl.h"

#include "base/bind.h"
#include "base/no_destructor.h"
#include "base/task_runner.h"
#include "chromeos/services/secure_channel/public/cpp/client/connection_attempt_impl.h"

namespace chromeos {

namespace secure_channel {

// static
SecureChannelClientImpl::Factory*
    SecureChannelClientImpl::Factory::test_factory_ = nullptr;

// static
SecureChannelClientImpl::Factory* SecureChannelClientImpl::Factory::Get() {
  if (test_factory_)
    return test_factory_;

  static base::NoDestructor<Factory> factory;
  return factory.get();
}

// static
void SecureChannelClientImpl::Factory::SetInstanceForTesting(
    Factory* test_factory) {
  test_factory_ = test_factory;
}

SecureChannelClientImpl::Factory::~Factory() = default;

std::unique_ptr<SecureChannelClient>
SecureChannelClientImpl::Factory::BuildInstance(
    mojo::PendingRemote<mojom::SecureChannel> channel,
    scoped_refptr<base::TaskRunner> task_runner) {
  return base::WrapUnique(
      new SecureChannelClientImpl(std::move(channel), task_runner));
}

SecureChannelClientImpl::SecureChannelClientImpl(
    mojo::PendingRemote<mojom::SecureChannel> channel,
    scoped_refptr<base::TaskRunner> task_runner)
    : secure_channel_remote_(std::move(channel)), task_runner_(task_runner) {}

SecureChannelClientImpl::~SecureChannelClientImpl() = default;

std::unique_ptr<ConnectionAttempt>
SecureChannelClientImpl::InitiateConnectionToDevice(
    multidevice::RemoteDeviceRef device_to_connect,
    multidevice::RemoteDeviceRef local_device,
    const std::string& feature,
    ConnectionPriority connection_priority) {
  auto connection_attempt =
      ConnectionAttemptImpl::Factory::Get()->BuildInstance();

  // Delay directly calling mojom::SecureChannel::InitiateConnectionToDevice()
  // until the caller has added itself as a Delegate of the ConnectionAttempt.
  task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(
          &SecureChannelClientImpl::PerformInitiateConnectionToDevice,
          weak_ptr_factory_.GetWeakPtr(), device_to_connect, local_device,
          feature, connection_priority, connection_attempt->GenerateRemote()));

  return connection_attempt;
}

std::unique_ptr<ConnectionAttempt>
SecureChannelClientImpl::ListenForConnectionFromDevice(
    multidevice::RemoteDeviceRef device_to_connect,
    multidevice::RemoteDeviceRef local_device,
    const std::string& feature,
    ConnectionPriority connection_priority) {
  auto connection_attempt =
      ConnectionAttemptImpl::Factory::Get()->BuildInstance();

  // Delay directly calling
  // mojom::SecureChannel::ListenForConnectionFromDevice() until the caller has
  // added itself as a Delegate of the ConnectionAttempt.
  task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(
          &SecureChannelClientImpl::PerformListenForConnectionFromDevice,
          weak_ptr_factory_.GetWeakPtr(), device_to_connect, local_device,
          feature, connection_priority, connection_attempt->GenerateRemote()));

  return connection_attempt;
}

void SecureChannelClientImpl::PerformInitiateConnectionToDevice(
    multidevice::RemoteDeviceRef device_to_connect,
    multidevice::RemoteDeviceRef local_device,
    const std::string& feature,
    ConnectionPriority connection_priority,
    mojo::PendingRemote<mojom::ConnectionDelegate> connection_delegate_remote) {
  secure_channel_remote_->InitiateConnectionToDevice(
      device_to_connect.GetRemoteDevice(), local_device.GetRemoteDevice(),
      feature, connection_priority, std::move(connection_delegate_remote));
}

void SecureChannelClientImpl::PerformListenForConnectionFromDevice(
    multidevice::RemoteDeviceRef device_to_connect,
    multidevice::RemoteDeviceRef local_device,
    const std::string& feature,
    ConnectionPriority connection_priority,
    mojo::PendingRemote<mojom::ConnectionDelegate> connection_delegate_remote) {
  secure_channel_remote_->ListenForConnectionFromDevice(
      device_to_connect.GetRemoteDevice(), local_device.GetRemoteDevice(),
      feature, connection_priority, std::move(connection_delegate_remote));
}

void SecureChannelClientImpl::FlushForTesting() {
  secure_channel_remote_.FlushForTesting();
}

}  // namespace secure_channel

}  // namespace chromeos
