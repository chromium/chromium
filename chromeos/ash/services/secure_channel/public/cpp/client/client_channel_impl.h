// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_SERVICES_SECURE_CHANNEL_PUBLIC_CPP_CLIENT_CLIENT_CHANNEL_IMPL_H_
#define CHROMEOS_ASH_SERVICES_SECURE_CHANNEL_PUBLIC_CPP_CLIENT_CLIENT_CHANNEL_IMPL_H_

#include "base/containers/flat_map.h"
#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "chromeos/ash/services/secure_channel/public/cpp/client/client_channel.h"
#include "chromeos/ash/services/secure_channel/public/mojom/nearby_connector.mojom-forward.h"
#include "chromeos/ash/services/secure_channel/public/mojom/nearby_connector.mojom.h"
#include "chromeos/ash/services/secure_channel/public/mojom/secure_channel.mojom.h"
#include "chromeos/ash/services/secure_channel/public/mojom/secure_channel_types.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace ash::secure_channel {

// Concrete implementation of ClientChannel.
class ClientChannelImpl : public ClientChannel,
                          public mojom::MessageReceiver,
                          public mojom::FilePayloadListener,
                          public mojom::NearbyConnectionStateListener {
 public:
  class Factory {
   public:
    static std::unique_ptr<ClientChannel> Create(
        mojo::PendingRemote<mojom::Channel> channel,
        mojo::PendingReceiver<mojom::MessageReceiver> message_receiver_receiver,
        mojo::PendingReceiver<mojom::NearbyConnectionStateListener>
            nearby_connection_state_listener_receiver);
    static void SetFactoryForTesting(Factory* test_factory);

   protected:
    virtual ~Factory();
    virtual std::unique_ptr<ClientChannel> CreateInstance(
        mojo::PendingRemote<mojom::Channel> channel,
        mojo::PendingReceiver<mojom::MessageReceiver>
            message_receiver_receiver) = 0;

   private:
    static Factory* test_factory_;
  };

  ClientChannelImpl(const ClientChannelImpl&) = delete;
  ClientChannelImpl& operator=(const ClientChannelImpl&) = delete;

  ~ClientChannelImpl() override;

 private:
  friend class SecureChannelClientChannelImplTest;

  ClientChannelImpl(
      mojo::PendingRemote<mojom::Channel> channel,
      mojo::PendingReceiver<mojom::MessageReceiver> message_receiver_receiver,
      mojo::PendingReceiver<mojom::NearbyConnectionStateListener>
          nearby_connection_state_listener_receiver);

  // ClientChannel:
  void PerformGetConnectionMetadata(
      base::OnceCallback<void(mojom::ConnectionMetadataPtr)> callback) override;
  void PerformSendMessage(const std::string& payload,
                          base::OnceClosure on_sent_callback) override;
  void PerformRegisterPayloadFile(
      int64_t payload_id,
      mojom::PayloadFilesPtr payload_files,
      base::RepeatingCallback<void(mojom::FileTransferUpdatePtr)>
          file_transfer_update_callback,
      base::OnceCallback<void(bool)> registration_result_callback) override;

  // MessageReceiver:
  void OnMessageReceived(const std::string& message) override;

  // mojom::FilePayloadListener:
  void OnFileTransferUpdate(mojom::FileTransferUpdatePtr update) override;

  // mojom::NearbyConnectionStateListener:
  void OnNearbyConnectionStateChanged(
      mojom::NearbyConnectionStep step,
      mojom::NearbyConnectionStepResult result) override;

  void OnGetConnectionMetadata(
      base::OnceCallback<void(mojom::ConnectionMetadataPtr)> callback,
      mojom::ConnectionMetadataPtr connection_metadata_ptr);

  // Called when this channel is disconnected or destroyed to notify callers
  // about pending file transfers being canceled.
  void CleanUpPendingFileTransfers();
  // Called when a FilePayloadListener remote endpoint is disconnected.
  void OnFilePayloadListenerRemoteDisconnected();

  void OnChannelDisconnected(uint32_t disconnection_reason,
                             const std::string& disconnection_description);

  void FlushForTesting();

  mojo::Remote<mojom::Channel> channel_;
  mojo::Receiver<mojom::MessageReceiver> receiver_;
  mojo::Receiver<mojom::NearbyConnectionStateListener>
      nearby_connection_state_listener_receiver_;
  // Set of receivers created to listen to file payload transfer updates, one
  // for each payload registered via RegisterPayloadFile(). These receivers will
  // be automatically removed from the set when their corresponding Remote
  // endpoints are destroyed upon transfer completion. current_context() will
  // return the corresponding payload ID when a receiver is called or
  // disconnected.
  mojo::ReceiverSet<mojom::FilePayloadListener, int64_t>
      file_payload_listeners_;

  // Callbacks to receive FileTransferUpdates for registered file payloads.
  // Keyed by payload ID. A callback will be emitted from this map when the
  // corresponding mojo::Remote<mojom::FilePayloadListener> is disconnected.
  base::flat_map<int64_t,
                 base::RepeatingCallback<void(mojom::FileTransferUpdatePtr)>>
      file_transfer_update_callbacks_;

  base::WeakPtrFactory<ClientChannelImpl> weak_ptr_factory_{this};
};

}  // namespace ash::secure_channel

#endif  // CHROMEOS_ASH_SERVICES_SECURE_CHANNEL_PUBLIC_CPP_CLIENT_CLIENT_CHANNEL_IMPL_H_
