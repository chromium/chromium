// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/services/secure_channel/public/cpp/client/fake_secure_channel_client.h"

#include "chromeos/components/multidevice/remote_device_ref.h"

namespace chromeos {

namespace secure_channel {

FakeSecureChannelClient::ConnectionRequestArguments::ConnectionRequestArguments(
    multidevice::RemoteDeviceRef device_to_connect,
    multidevice::RemoteDeviceRef local_device,
    const std::string& feature,
    const ConnectionPriority& connection_priority)
    : device_to_connect(device_to_connect),
      local_device(local_device),
      feature(feature),
      connection_priority(connection_priority) {}

FakeSecureChannelClient::ConnectionRequestArguments::
    ~ConnectionRequestArguments() = default;

FakeSecureChannelClient::FakeSecureChannelClient() = default;

FakeSecureChannelClient::~FakeSecureChannelClient() {
  DCHECK(device_pair_to_next_initiate_connection_attempt_.empty());
  DCHECK(device_pair_to_next_listen_connection_attempt_.empty());
}

std::unique_ptr<ConnectionAttempt>
FakeSecureChannelClient::InitiateConnectionToDevice(
    multidevice::RemoteDeviceRef device_to_connect,
    multidevice::RemoteDeviceRef local_device,
    const std::string& feature,
    ConnectionPriority connection_priority) {
  auto remote_local_pair = std::make_pair(device_to_connect, local_device);
  std::unique_ptr<ConnectionAttempt> connection_attempt = std::move(
      device_pair_to_next_initiate_connection_attempt_[remote_local_pair]);
  device_pair_to_next_initiate_connection_attempt_.erase(remote_local_pair);
  return connection_attempt;
}

std::unique_ptr<ConnectionAttempt>
FakeSecureChannelClient::ListenForConnectionFromDevice(
    multidevice::RemoteDeviceRef device_to_connect,
    multidevice::RemoteDeviceRef local_device,
    const std::string& feature,
    ConnectionPriority connection_priority) {
  auto remote_local_pair = std::make_pair(device_to_connect, local_device);
  std::unique_ptr<ConnectionAttempt> connection_attempt = std::move(
      device_pair_to_next_listen_connection_attempt_[remote_local_pair]);
  device_pair_to_next_listen_connection_attempt_.erase(remote_local_pair);
  return connection_attempt;
}

}  // namespace secure_channel

}  // namespace chromeos
