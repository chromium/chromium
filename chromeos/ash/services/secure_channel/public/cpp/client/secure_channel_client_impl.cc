// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/services/secure_channel/public/cpp/client/secure_channel_client_impl.h"

#include "base/functional/bind.h"
#include "base/memory/ptr_util.h"
#include "base/task/task_runner.h"
#include "chromeos/ash/components/multidevice/remote_device_ref.h"
#include "chromeos/ash/services/secure_channel/public/cpp/client/connection_attempt_impl.h"
#include "chromeos/ash/services/secure_channel/public/cpp/client/nearby_connector.h"
#include "chromeos/ash/services/secure_channel/public/cpp/client/secure_channel_client.h"
#include "chromeos/ash/services/secure_channel/public/mojom/secure_channel.mojom.h"
#include "mojo/public/cpp/bindings/pending_remote.h"

namespace ash::secure_channel {

// static
SecureChannelClientImpl::Factory*
    SecureChannelClientImpl::Factory::test_factory_ = nullptr;

// static
std::unique_ptr<SecureChannelClient> SecureChannelClientImpl::Factory::Create(
    mojo::PendingRemote<mojom::SecureChannel> channel,
    scoped_refptr<base::TaskRunner> task_runner) {
  if (test_factory_) {
    return test_factory_->CreateInstance(std::move(channel),
                                         std::move(task_runner));
  }

  return base::WrapUnique(
      new SecureChannelClientImpl(std::move(channel), std::move(task_runner)));
}

// static
void SecureChannelClientImpl::Factory::SetFactoryForTesting(
    Factory* test_factory) {
  test_factory_ = test_factory;
}

SecureChannelClientImpl::Factory::~Factory() = default;

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
    ConnectionMedium connection_medium,
    ConnectionPriority connection_priority,
    SecureChannelStructuredMetricsLogger*
        secure_channel_structured_metrics_logger) {
  auto connection_attempt = ConnectionAttemptImpl::Factory::Create();

  // Delay directly calling mojom::SecureChannel::InitiateConnectionToDevice()
  // until the caller has added itself as a Delegate of the ConnectionAttempt.
  task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(
          &SecureChannelClientImpl::PerformInitiateConnectionToDevice,
          weak_ptr_factory_.GetWeakPtr(), device_to_connect, local_device,
          feature, connection_medium, connection_priority,
          connection_attempt->GenerateRemote(),
          secure_channel_structured_metrics_logger
              ? secure_channel_structured_metrics_logger->GenerateRemote()
              : mojo::NullRemote()));

  return connection_attempt;
}

std::unique_ptr<ConnectionAttempt>
SecureChannelClientImpl::ListenForConnectionFromDevice(
    multidevice::RemoteDeviceRef device_to_connect,
    multidevice::RemoteDeviceRef local_device,
    const std::string& feature,
    ConnectionMedium connection_medium,
    ConnectionPriority connection_priority) {
  auto connection_attempt = ConnectionAttemptImpl::Factory::Create();

  // Delay directly calling
  // mojom::SecureChannel::ListenForConnectionFromDevice() until the caller has
  // added itself as a Delegate of the ConnectionAttempt.
  task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(
          &SecureChannelClientImpl::PerformListenForConnectionFromDevice,
          weak_ptr_factory_.GetWeakPtr(), device_to_connect, local_device,
          feature, connection_medium, connection_priority,
          connection_attempt->GenerateRemote()));

  return connection_attempt;
}

void SecureChannelClientImpl::SetNearbyConnector(
    NearbyConnector* nearby_connector) {
  secure_channel_remote_->SetNearbyConnector(
      nearby_connector->GeneratePendingRemote());
}

void SecureChannelClientImpl::PerformInitiateConnectionToDevice(
    multidevice::RemoteDeviceRef device_to_connect,
    multidevice::RemoteDeviceRef local_device,
    const std::string& feature,
    ConnectionMedium connection_medium,
    ConnectionPriority connection_priority,
    mojo::PendingRemote<mojom::ConnectionDelegate> connection_delegate_remote,
    mojo::PendingRemote<mojom::SecureChannelStructuredMetricsLogger>
        secure_channel_structured_metrics_logger_remote) {
  secure_channel_remote_->InitiateConnectionToDevice(
      device_to_connect.GetRemoteDevice(), local_device.GetRemoteDevice(),
      feature, connection_medium, connection_priority,
      std::move(connection_delegate_remote),
      std::move(secure_channel_structured_metrics_logger_remote));
}

void SecureChannelClientImpl::PerformListenForConnectionFromDevice(
    multidevice::RemoteDeviceRef device_to_connect,
    multidevice::RemoteDeviceRef local_device,
    const std::string& feature,
    ConnectionMedium connection_medium,
    ConnectionPriority connection_priority,
    mojo::PendingRemote<mojom::ConnectionDelegate> connection_delegate_remote) {
  secure_channel_remote_->ListenForConnectionFromDevice(
      device_to_connect.GetRemoteDevice(), local_device.GetRemoteDevice(),
      feature, connection_medium, connection_priority,
      std::move(connection_delegate_remote));
}

void SecureChannelClientImpl::FlushForTesting() {
  secure_channel_remote_.FlushForTesting();
}

void SecureChannelClientImpl::GetLastSeenTimestamp(
    const std::string& remote_device_id,
    base::OnceCallback<void(std::optional<base::Time>)> callback) {
  secure_channel_remote_->GetLastSeenTimestamp(remote_device_id,
                                               std::move(callback));
}

}  // namespace ash::secure_channel
