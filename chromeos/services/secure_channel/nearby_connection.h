// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_SERVICES_SECURE_CHANNEL_NEARBY_CONNECTION_H_
#define CHROMEOS_SERVICES_SECURE_CHANNEL_NEARBY_CONNECTION_H_

#include "base/memory/weak_ptr.h"
#include "chromeos/services/secure_channel/connection.h"
#include "chromeos/services/secure_channel/public/mojom/nearby_connector.mojom.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace chromeos {

namespace secure_channel {

// Connection implementation which creates a connection to a remote device via
// mojom::NearbyConnector. Implements mojom::NearbyMessageReceiver to receive
// messages from the NearbyConnector and uses a
// mojo::Remote<mojom::NearbyMessageSender> to send messages to the
// NearbyConnector.
//
// When requested to send a message, this class queues messages and only sends
// one message after the previous one has been sent successfully. If sending a
// message fails, this is considered a fatal error, and the connection is
// disconnected.
class NearbyConnection : public Connection,
                         public mojom::NearbyMessageReceiver {
 public:
  class Factory {
   public:
    static std::unique_ptr<Connection> Create(
        multidevice::RemoteDeviceRef remote_device,
        mojom::NearbyConnector* nearby_connector);
    static void SetFactoryForTesting(Factory* factory);
    virtual ~Factory() = default;

   protected:
    virtual std::unique_ptr<Connection> CreateInstance(
        multidevice::RemoteDeviceRef remote_device,
        mojom::NearbyConnector* nearby_connector) = 0;

   private:
    static Factory* factory_instance_;
  };

  ~NearbyConnection() override;

 private:
  NearbyConnection(multidevice::RemoteDeviceRef remote_device,
                   mojom::NearbyConnector* nearby_connector);

  // Connection:
  void Connect() override;
  void Disconnect() override;
  std::string GetDeviceAddress() override;
  void SendMessageImpl(std::unique_ptr<WireMessage> message) override;

  // mojom::NearbyMessageReceiver:
  void OnMessageReceived(const std::string& message) override;

  // Returns the the remote device's address as a byte array; note that
  // GetDeviceAddress() returns a colon-separated hex string.
  std::vector<uint8_t> GetRemoteDeviceBluetoothAddressAsVector();

  void OnConnectResult(
      mojo::PendingRemote<mojom::NearbyMessageSender> message_sender);
  void OnSendMessageResult(bool success);
  void ProcessQueuedMessagesToSend();

  mojom::NearbyConnector* nearby_connector_;
  mojo::Receiver<mojom::NearbyMessageReceiver> message_receiver_{this};
  mojo::Remote<mojom::NearbyMessageSender> message_sender_;

  base::queue<std::unique_ptr<WireMessage>> queued_messages_to_send_;

  // Null if no message is being sent.
  std::unique_ptr<WireMessage> message_being_sent_;

  base::WeakPtrFactory<NearbyConnection> weak_ptr_factory_{this};
};

}  // namespace secure_channel

}  // namespace chromeos

#endif  // CHROMEOS_SERVICES_SECURE_CHANNEL_NEARBY_CONNECTION_H_
