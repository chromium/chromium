// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_SERVICES_SECURE_CHANNEL_PUBLIC_CPP_CLIENT_FAKE_NEARBY_CONNECTOR_H_
#define CHROMEOS_ASH_SERVICES_SECURE_CHANNEL_PUBLIC_CPP_CLIENT_FAKE_NEARBY_CONNECTOR_H_

#include <memory>
#include <string>
#include <vector>

#include "base/containers/flat_map.h"
#include "base/containers/queue.h"
#include "base/functional/callback.h"
#include "chromeos/ash/services/secure_channel/public/cpp/client/nearby_connector.h"
#include "chromeos/ash/services/secure_channel/public/mojom/nearby_connector.mojom.h"
#include "chromeos/ash/services/secure_channel/public/mojom/secure_channel_types.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace ash::secure_channel {

// Fake NearbyConnector implementation. When Connect() is called, parameters are
// queued up and can be completed using either FailQueuedCallback() or
// ConnectQueuedCallback(). Both of these functions take the parameters at the
// front of the queue and either cause the connection to fail or succeed. In the
// success case, a FakeConnection is returned which allows the client to
// interact with the connection.
class FakeNearbyConnector : public NearbyConnector {
 public:
  FakeNearbyConnector();
  ~FakeNearbyConnector() override;

  class FakeConnection : public mojom::NearbyMessageSender,
                         public mojom::NearbyFilePayloadHandler {
   public:
    FakeConnection(const std::vector<uint8_t>& bluetooth_public_address,
                   mojo::PendingReceiver<mojom::NearbyMessageSender>
                       message_sender_pending_receiver,
                   mojo::PendingReceiver<mojom::NearbyFilePayloadHandler>
                       nearby_file_payload_handler_receiver,
                   mojo::PendingRemote<mojom::NearbyMessageReceiver>
                       message_receiver_pending_remote);
    ~FakeConnection() override;

    struct RegisterPayloadFileRequest {
      RegisterPayloadFileRequest(int64_t payload_id,
                                 mojo::PendingRemote<mojom::FilePayloadListener>
                                     file_payload_listener);
      RegisterPayloadFileRequest(RegisterPayloadFileRequest&&);
      RegisterPayloadFileRequest& operator=(RegisterPayloadFileRequest&&);
      ~RegisterPayloadFileRequest();

      int64_t payload_id;
      mojo::Remote<mojom::FilePayloadListener> file_payload_listener;
    };

    void Disconnect();
    void ReceiveMessage(const std::string& message);

    void SendFileTransferUpdate(int64_t payload_id,
                                mojom::FileTransferStatus status,
                                uint64_t total_bytes,
                                uint64_t bytes_transferred);
    void SendUnexpectedFileTransferUpdate(int64_t unexpected_payload_id);
    void DisconnectPendingFileTransfers();

    void set_should_send_succeed(bool should_send_succeed) {
      should_send_succeed_ = should_send_succeed;
    }

    const std::vector<uint8_t>& bluetooth_public_address() const {
      return bluetooth_public_address_;
    }
    const std::vector<std::string>& sent_messages() { return sent_messages_; }

    const base::flat_map<int64_t, RegisterPayloadFileRequest>&
    register_payload_file_requests() const {
      return register_payload_file_requests_;
    }

   private:
    // mojom::NearbyMessageSender:
    void SendMessage(const std::string& message,
                     SendMessageCallback callback) override;

    // mojom::NearbyFilePayloadHandler:
    void RegisterPayloadFile(
        int64_t payload_id,
        mojom::PayloadFilesPtr payload_files,
        mojo::PendingRemote<mojom::FilePayloadListener> listener,
        RegisterPayloadFileCallback callback) override;

    std::vector<uint8_t> bluetooth_public_address_;
    mojo::Receiver<mojom::NearbyMessageSender> message_sender_receiver_;
    mojo::Receiver<mojom::NearbyFilePayloadHandler>
        file_payload_handler_receiver_;
    mojo::Remote<mojom::NearbyMessageReceiver> message_receiver_remote_;

    std::vector<std::string> sent_messages_;
    base::flat_map<int64_t, RegisterPayloadFileRequest>
        register_payload_file_requests_;
    bool should_send_succeed_ = true;
  };

  void FailQueuedCallback();
  FakeConnection* ConnectQueuedCallback();

  // Invoked when Connect() is called.
  base::OnceClosure on_connect_closure;

 private:
  struct ConnectArgs {
    ConnectArgs(
        const std::vector<uint8_t>& bluetooth_public_address,
        mojo::PendingRemote<mojom::NearbyMessageReceiver> message_receiver,
        mojo::PendingRemote<mojom::NearbyConnectionStateListener>
            nearby_connection_state_listener,
        ConnectCallback callback);
    ~ConnectArgs();

    std::vector<uint8_t> bluetooth_public_address;
    mojo::PendingRemote<mojom::NearbyMessageReceiver> message_receiver;
    mojo::PendingRemote<mojom::NearbyConnectionStateListener>
        nearby_connection_state_listener;
    ConnectCallback callback;
  };

  /// mojom::NearbyConnector:
  void Connect(
      const std::vector<uint8_t>& bluetooth_public_address,
      const std::vector<uint8_t>& service_data,
      mojo::PendingRemote<mojom::NearbyMessageReceiver> message_receiver,
      mojo::PendingRemote<mojom::NearbyConnectionStateListener>
          nearby_connection_state_listener,
      ConnectCallback callback) override;

  base::queue<std::unique_ptr<ConnectArgs>> queued_connect_args_;
  std::vector<std::unique_ptr<FakeConnection>> fake_connections_;
};

}  // namespace ash::secure_channel

#endif  // CHROMEOS_ASH_SERVICES_SECURE_CHANNEL_PUBLIC_CPP_CLIENT_FAKE_NEARBY_CONNECTOR_H_
