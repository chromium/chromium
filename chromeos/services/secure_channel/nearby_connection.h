// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_SERVICES_SECURE_CHANNEL_NEARBY_CONNECTION_H_
#define CHROMEOS_SERVICES_SECURE_CHANNEL_NEARBY_CONNECTION_H_

#include "chromeos/services/secure_channel/connection.h"

namespace chromeos {

namespace secure_channel {

// Connection implementation which creates a connection to a remote device via
// the Nearby Connections library.
// TODO(https://crbug.com/1106937): Add real implementation.
class NearbyConnection : public Connection {
 public:
  class Factory {
   public:
    static std::unique_ptr<Connection> Create(
        multidevice::RemoteDeviceRef remote_device);
    static void SetFactoryForTesting(Factory* factory);
    virtual ~Factory() = default;

   protected:
    virtual std::unique_ptr<Connection> CreateInstance(
        multidevice::RemoteDeviceRef remote_device) = 0;

   private:
    static Factory* factory_instance_;
  };

  ~NearbyConnection() override;

 private:
  NearbyConnection(multidevice::RemoteDeviceRef remote_device);

  // Connection:
  void Connect() override;
  void Disconnect() override;
  std::string GetDeviceAddress() override;
  void SendMessageImpl(std::unique_ptr<WireMessage> message) override;
};

}  // namespace secure_channel

}  // namespace chromeos

#endif  // CHROMEOS_SERVICES_SECURE_CHANNEL_NEARBY_CONNECTION_H_
