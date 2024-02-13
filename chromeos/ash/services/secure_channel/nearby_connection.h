// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_SERVICES_SECURE_CHANNEL_NEARBY_CONNECTION_H_
#define CHROMEOS_ASH_SERVICES_SECURE_CHANNEL_NEARBY_CONNECTION_H_

#include <optional>

#include "base/containers/flat_map.h"
#include "base/containers/queue.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chromeos/ash/services/secure_channel/connection.h"
#include "chromeos/ash/services/secure_channel/file_transfer_update_callback.h"
#include "chromeos/ash/services/secure_channel/public/mojom/nearby_connector.mojom-shared.h"
#include "chromeos/ash/services/secure_channel/public/mojom/nearby_connector.mojom.h"
#include "chromeos/ash/services/secure_channel/public/mojom/secure_channel_types.mojom.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace ash::secure_channel {

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
//
// Also implements mojom::FilePayloadListener to listen to transfer
// updates for file payloads registered via RegisterPayloadFile.
class NearbyConnection : public Connection,
                         public mojom::NearbyMessageReceiver,
                         public mojom::FilePayloadListener,
                         public mojom::NearbyConnectionStateListener {
 public:
  class Factory {
   public:
    static std::unique_ptr<Connection> Create(
        multidevice::RemoteDeviceRef remote_device,
        const std::vector<uint8_t>& eid,
        mojom::NearbyConnector* nearby_connector);
    static void SetFactoryForTesting(Factory* factory);
    virtual ~Factory() = default;

   protected:
    virtual std::unique_ptr<Connection> CreateInstance(
        multidevice::RemoteDeviceRef remote_device,
        const std::vector<uint8_t>& eid,
        mojom::NearbyConnector* nearby_connector) = 0;

   private:
    static Factory* factory_instance_;
  };

  ~NearbyConnection() override;

 private:
  NearbyConnection(multidevice::RemoteDeviceRef remote_device,
                   const std::vector<uint8_t>& eid,
                   mojom::NearbyConnector* nearby_connector);

  // Connection:
  void Connect() override;
  void Disconnect() override;
  std::string GetDeviceAddress() override;
  void SendMessageImpl(std::unique_ptr<WireMessage> message) override;
  void RegisterPayloadFileImpl(
      int64_t payload_id,
      mojom::PayloadFilesPtr payload_files,
      FileTransferUpdateCallback file_transfer_update_callback,
      base::OnceCallback<void(bool)> registration_result_callback) override;

  // mojom::NearbyMessageReceiver:
  void OnMessageReceived(const std::string& message) override;

  // mojom::NearbyConnectionStateListener:
  void OnNearbyConnectionStateChanged(
      mojom::NearbyConnectionStep step,
      mojom::NearbyConnectionStepResult result) override;

  // mojom::FilePayloadListener:
  void OnFileTransferUpdate(mojom::FileTransferUpdatePtr update) override;

  // Returns the the remote device's address as a byte array; note that
  // GetDeviceAddress() returns a colon-separated hex string.
  std::vector<uint8_t> GetRemoteDeviceBluetoothAddressAsVector();

  void OnConnectResult(
      mojo::PendingRemote<mojom::NearbyMessageSender> message_sender,
      mojo::PendingRemote<mojom::NearbyFilePayloadHandler>
          file_payload_handler);
  void OnSendMessageResult(bool success);
  void ProcessQueuedMessagesToSend();
  // Called when Disconnect() is called by the client. Can also be called when
  // the remote endpoint drops the connection and the other remotes, e.g.
  // MessageSender, triggers Disconnect() before the |disconnect_handler|s of
  // the NearbyPayloadListeners are called.
  void CleanUpPendingFileTransfersOnDisconnect();
  // Called when a FilePayloadListener remote endpoint is disconnected.
  void OnFilePayloadListenerRemoteDisconnected();

  raw_ptr<mojom::NearbyConnector> nearby_connector_;
  mojo::Receiver<mojom::NearbyMessageReceiver> message_receiver_{this};
  mojo::Receiver<mojom::NearbyConnectionStateListener>
      nearby_connection_state_listener_{this};
  mojo::Remote<mojom::NearbyMessageSender> message_sender_;
  mojo::Remote<mojom::NearbyFilePayloadHandler> file_payload_handler_;
  // Set of receivers created to listen to file payload transfer updates, one
  // for each payload registered via RegisterPayloadFile(). These receivers will
  // be automatically removed from the set when their corresponding Remote
  // endpoints are destroyed upon transfer completion. current_context() will
  // return the corresponding payload ID when a receiver is called or
  // disconnected.
  mojo::ReceiverSet<mojom::FilePayloadListener, int64_t>
      file_payload_listener_receivers_;

  std::vector<uint8_t> eid_;

  base::queue<std::unique_ptr<WireMessage>> queued_messages_to_send_;

  // Null if no message is being sent.
  std::unique_ptr<WireMessage> message_being_sent_;

  // Callbacks to receive FileTransferUpdates for registered file payloads.
  // Keyed by payload ID. A callback will be emitted from this map when the
  // corresponding mojo::Remote<mojom::FilePayloadListener> is disconnected.
  base::flat_map<int64_t, FileTransferUpdateCallback>
      file_transfer_update_callbacks_;

  base::WeakPtrFactory<NearbyConnection> weak_ptr_factory_{this};
};

}  // namespace ash::secure_channel

#endif  // CHROMEOS_ASH_SERVICES_SECURE_CHANNEL_NEARBY_CONNECTION_H_
