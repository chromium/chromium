// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_SERVICES_SECURE_CHANNEL_PUBLIC_CPP_CLIENT_SECURE_CHANNEL_CLIENT_H_
#define CHROMEOS_SERVICES_SECURE_CHANNEL_PUBLIC_CPP_CLIENT_SECURE_CHANNEL_CLIENT_H_

#include <string>

#include "base/callback_forward.h"
#include "base/macros.h"
#include "base/observer_list.h"
#include "chromeos/components/multidevice/remote_device_ref.h"
#include "chromeos/services/secure_channel/public/mojom/secure_channel.mojom.h"

namespace chromeos {

namespace secure_channel {

class ConnectionAttempt;

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
// 4) |connection_priority|:
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
  virtual ~SecureChannelClient() = default;

  virtual std::unique_ptr<ConnectionAttempt> InitiateConnectionToDevice(
      multidevice::RemoteDeviceRef device_to_connect,
      multidevice::RemoteDeviceRef local_device,
      const std::string& feature,
      ConnectionPriority connection_priority) = 0;
  virtual std::unique_ptr<ConnectionAttempt> ListenForConnectionFromDevice(
      multidevice::RemoteDeviceRef device_to_connect,
      multidevice::RemoteDeviceRef local_device,
      const std::string& feature,
      ConnectionPriority connection_priority) = 0;

 protected:
  SecureChannelClient() = default;

 private:
  DISALLOW_COPY_AND_ASSIGN(SecureChannelClient);
};

}  // namespace secure_channel

}  // namespace chromeos

#endif  // CHROMEOS_SERVICES_SECURE_CHANNEL_PUBLIC_CPP_CLIENT_SECURE_CHANNEL_CLIENT_H_
