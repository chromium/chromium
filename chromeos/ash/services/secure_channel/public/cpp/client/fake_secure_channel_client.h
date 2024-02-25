// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_SERVICES_SECURE_CHANNEL_PUBLIC_CPP_CLIENT_FAKE_SECURE_CHANNEL_CLIENT_H_
#define CHROMEOS_ASH_SERVICES_SECURE_CHANNEL_PUBLIC_CPP_CLIENT_FAKE_SECURE_CHANNEL_CLIENT_H_

#include <memory>
#include <string>
#include <vector>

#include "base/containers/contains.h"
#include "base/functional/callback.h"
#include "base/ranges/algorithm.h"
#include "chromeos/ash/components/multidevice/remote_device_ref.h"
#include "chromeos/ash/services/secure_channel/public/cpp/client/connection_attempt.h"
#include "chromeos/ash/services/secure_channel/public/cpp/client/secure_channel_client.h"

namespace ash::secure_channel {

// Test SecureChannelClient implementation.
class FakeSecureChannelClient : public SecureChannelClient {
 public:
  struct ConnectionRequestArguments {
   public:
    ConnectionRequestArguments(multidevice::RemoteDeviceRef device_to_connect,
                               multidevice::RemoteDeviceRef local_device,
                               const std::string& feature,
                               ConnectionMedium connection_medium,
                               ConnectionPriority connection_priority);

    ConnectionRequestArguments(const ConnectionRequestArguments&) = delete;
    ConnectionRequestArguments& operator=(const ConnectionRequestArguments&) =
        delete;

    ~ConnectionRequestArguments();

    multidevice::RemoteDeviceRef device_to_connect;
    multidevice::RemoteDeviceRef local_device;
    std::string feature;
    ConnectionMedium connection_medium;
    ConnectionPriority connection_priority;
  };

  FakeSecureChannelClient();

  FakeSecureChannelClient(const FakeSecureChannelClient&) = delete;
  FakeSecureChannelClient& operator=(const FakeSecureChannelClient&) = delete;

  ~FakeSecureChannelClient() override;

  void set_next_initiate_connection_attempt(
      multidevice::RemoteDeviceRef device_to_connect,
      multidevice::RemoteDeviceRef local_device,
      std::unique_ptr<ConnectionAttempt> attempt) {
    device_pair_to_next_initiate_connection_attempt_[std::make_pair(
        device_to_connect, local_device)] = std::move(attempt);
  }

  void set_next_listen_connection_attempt(
      multidevice::RemoteDeviceRef device_to_connect,
      multidevice::RemoteDeviceRef local_device,
      std::unique_ptr<ConnectionAttempt> attempt) {
    device_pair_to_next_listen_connection_attempt_[std::make_pair(
        device_to_connect, local_device)] = std::move(attempt);
  }

  ConnectionAttempt* peek_next_initiate_connection_attempt(
      multidevice::RemoteDeviceRef device_to_connect,
      multidevice::RemoteDeviceRef local_device) {
    auto device_id_pair = std::make_pair(device_to_connect, local_device);
    if (!base::Contains(device_pair_to_next_initiate_connection_attempt_,
                        device_id_pair)) {
      return nullptr;
    }

    return device_pair_to_next_initiate_connection_attempt_[device_id_pair]
        .get();
  }

  ConnectionAttempt* peek_next_listen_connection_attempt(
      multidevice::RemoteDeviceRef device_to_connect,
      multidevice::RemoteDeviceRef local_device) {
    auto device_id_pair = std::make_pair(device_to_connect, local_device);
    if (!base::Contains(device_pair_to_next_listen_connection_attempt_,
                        device_id_pair)) {
      return nullptr;
    }

    return device_pair_to_next_listen_connection_attempt_[device_id_pair].get();
  }

  void clear_next_initiate_connection_attempt(
      multidevice::RemoteDeviceRef device_to_connect,
      multidevice::RemoteDeviceRef local_device) {
    device_pair_to_next_initiate_connection_attempt_.erase(
        std::make_pair(device_to_connect, local_device));
  }

  void clear_next_listen_connection_attempt(
      multidevice::RemoteDeviceRef device_to_connect,
      multidevice::RemoteDeviceRef local_device) {
    device_pair_to_next_listen_connection_attempt_.erase(
        std::make_pair(device_to_connect, local_device));
  }

  std::vector<ConnectionRequestArguments*>
  last_initiate_connection_request_arguments_list() {
    std::vector<ConnectionRequestArguments*> arguments_list_raw_;
    base::ranges::transform(last_initiate_connection_request_arguments_list_,
                            std::back_inserter(arguments_list_raw_),
                            &std::unique_ptr<ConnectionRequestArguments>::get);
    return arguments_list_raw_;
  }

  std::vector<ConnectionRequestArguments*>
  last_listen_for_connection_request_arguments_list() {
    std::vector<ConnectionRequestArguments*> arguments_list_raw_;
    base::ranges::transform(last_listen_for_connection_request_arguments_list_,
                            std::back_inserter(arguments_list_raw_),
                            &std::unique_ptr<ConnectionRequestArguments>::get);
    return arguments_list_raw_;
  }

  // SecureChannelClient:
  std::unique_ptr<ConnectionAttempt> InitiateConnectionToDevice(
      multidevice::RemoteDeviceRef device_to_connect,
      multidevice::RemoteDeviceRef local_device,
      const std::string& feature,
      ConnectionMedium connection_medium,
      ConnectionPriority connection_priority,
      SecureChannelStructuredMetricsLogger*
          secure_channel_structured_metrics_logger) override;
  std::unique_ptr<ConnectionAttempt> ListenForConnectionFromDevice(
      multidevice::RemoteDeviceRef device_to_connect,
      multidevice::RemoteDeviceRef local_device,
      const std::string& feature,
      ConnectionMedium connection_medium,
      ConnectionPriority connection_priority) override;
  void SetNearbyConnector(NearbyConnector* nearby_connector) override {}
  void GetLastSeenTimestamp(
      const std::string& remote_device_id,
      base::OnceCallback<void(std::optional<base::Time>)> callback) override;

 private:
  // First element of pair is remote device, second is local device.
  base::flat_map<
      std::pair<multidevice::RemoteDeviceRef, multidevice::RemoteDeviceRef>,
      std::unique_ptr<ConnectionAttempt>>
      device_pair_to_next_initiate_connection_attempt_;
  base::flat_map<
      std::pair<multidevice::RemoteDeviceRef, multidevice::RemoteDeviceRef>,
      std::unique_ptr<ConnectionAttempt>>
      device_pair_to_next_listen_connection_attempt_;

  std::vector<std::unique_ptr<ConnectionRequestArguments>>
      last_initiate_connection_request_arguments_list_;
  std::vector<std::unique_ptr<ConnectionRequestArguments>>
      last_listen_for_connection_request_arguments_list_;
};

}  // namespace ash::secure_channel

#endif  // CHROMEOS_ASH_SERVICES_SECURE_CHANNEL_PUBLIC_CPP_CLIENT_FAKE_SECURE_CHANNEL_CLIENT_H_
