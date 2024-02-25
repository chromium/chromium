// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_SERVICES_SECURE_CHANNEL_PUBLIC_CPP_CLIENT_SECURE_CHANNEL_CLIENT_H_
#define CHROMEOS_ASH_SERVICES_SECURE_CHANNEL_PUBLIC_CPP_CLIENT_SECURE_CHANNEL_CLIENT_H_

#include <string>

#include "chromeos/ash/services/secure_channel/public/cpp/client/secure_channel_structured_metrics_logger.h"
#include "chromeos/ash/services/secure_channel/public/mojom/secure_channel.mojom.h"

namespace ash {

namespace multidevice {
class RemoteDeviceRef;
}

namespace secure_channel {

class ConnectionAttempt;
class NearbyConnector;
enum class ConnectionMedium;
enum class ConnectionPriority;

// Provides clients access to the SecureChannel API.
//
// Clients can choose to either initiate a connection to another device, or
// listen for an expected connection from another device. Device details are
// encapsulated in the RemoteDeviceRef; see the DeviceSync API for information
// on how to retrieve this data.
//
// Calls to initiate or listen for a connection take identical arguments:
// 1) |device_to_connect|:
//    The RemoteDeviceRef which refers to the device a connection should be made
//    to.
// 2) |local_device|:
//    The RemoteDeviceRef which refers to the local device. |local_device| and
//    |device_to_connect| must be in the same user account.
// 3) |feature|:
//    A unique string identifier for your feature. If multiple clients make a
//    a connection request between the same |device_to_connect| and
//    |local_device| but different features, those clients will share the same
//    underlying connection, but their messages will be routed to the correct
//    clients based on the |feature| identifier of the message.
// 4) |connection_medium|:
//    The medium (e.g., BLE) to use.
// 5) |connection_priority|:
//    The priority of this connection request. Please make higher priority
//    requests only when necessary.
//
// Calls to initiate or listen for a connection will return a ConnectionAttempt
// object. Please see the documentation on ConnectionAttempt to learn how to
// correctly use it.
//
// Note: Right now, the SecureChannel API only offers connections to other
// devices over BLE. In the future, more connection mediums will be offered.
class SecureChannelClient {
 public:
  SecureChannelClient(const SecureChannelClient&) = delete;
  SecureChannelClient& operator=(const SecureChannelClient&) = delete;

  virtual ~SecureChannelClient() = default;

  virtual std::unique_ptr<ConnectionAttempt> InitiateConnectionToDevice(
      multidevice::RemoteDeviceRef device_to_connect,
      multidevice::RemoteDeviceRef local_device,
      const std::string& feature,
      ConnectionMedium connection_medium,
      ConnectionPriority connection_priority,
      SecureChannelStructuredMetricsLogger*
          secure_channel_structure_metrics_logger) = 0;
  virtual std::unique_ptr<ConnectionAttempt> ListenForConnectionFromDevice(
      multidevice::RemoteDeviceRef device_to_connect,
      multidevice::RemoteDeviceRef local_device,
      const std::string& feature,
      ConnectionMedium connection_medium,
      ConnectionPriority connection_priority) = 0;
  virtual void SetNearbyConnector(NearbyConnector* nearby_connector) = 0;

  // Retrieves the timestamp of the last successful discovery for the given
  // |remote_device_id|, or nullopt if we haven't seen this remote device during
  // the current Chrome session.
  virtual void GetLastSeenTimestamp(
      const std::string& remote_device_id,
      base::OnceCallback<void(std::optional<base::Time>)> callback) = 0;

 protected:
  SecureChannelClient() = default;
};

}  // namespace secure_channel
}  // namespace ash

#endif  // CHROMEOS_ASH_SERVICES_SECURE_CHANNEL_PUBLIC_CPP_CLIENT_SECURE_CHANNEL_CLIENT_H_
