// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_SERVICES_SECURE_CHANNEL_CHANNEL_IMPL_H_
#define CHROMEOS_ASH_SERVICES_SECURE_CHANNEL_CHANNEL_IMPL_H_

#include <string>

#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chromeos/ash/services/secure_channel/file_transfer_update_callback.h"
#include "chromeos/ash/services/secure_channel/public/mojom/secure_channel.mojom.h"
#include "chromeos/ash/services/secure_channel/public/mojom/secure_channel_types.mojom.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote_set.h"

namespace ash::secure_channel {

// Channel which provides clients the ability to send messages to remote
// devices, registers local files to receive file transfers, and to listen for
// disconnections from those devices. To be notified when a channel becomes
// disconnected, clients should use set_connection_error_with_reason_handler()
// and wait for a connection error with reason
// mojom::Channel::kConnectionDroppedReason.
class ChannelImpl : public mojom::Channel {
 public:
  class Delegate {
   public:
    virtual ~Delegate() = default;
    virtual void OnSendMessageRequested(const std::string& message,
                                        base::OnceClosure on_sent_callback) = 0;
    virtual void RegisterPayloadFile(
        int64_t payload_id,
        mojom::PayloadFilesPtr payload_files,
        FileTransferUpdateCallback file_transfer_update_callback,
        base::OnceCallback<void(bool)> registration_result_callback) = 0;
    virtual void GetConnectionMetadata(
        base::OnceCallback<void(mojom::ConnectionMetadataPtr)> callback) = 0;
    virtual void OnClientDisconnected() = 0;
  };

  explicit ChannelImpl(Delegate* delegate);

  ChannelImpl(const ChannelImpl&) = delete;
  ChannelImpl& operator=(const ChannelImpl&) = delete;

  ~ChannelImpl() override;

  // Generates a mojo::PendingRemote<Channel> for this instance; can only be
  // called once.
  mojo::PendingRemote<mojom::Channel> GenerateRemote();

  // Should be called when the underlying connection to the remote device has
  // been disconnected (e.g., because the other device closed the connection or
  // because of instability on the communication channel).
  void HandleRemoteDeviceDisconnection();

 private:
  // mojom::Channel:
  void SendMessage(const std::string& message,
                   SendMessageCallback callback) override;
  void RegisterPayloadFile(
      int64_t payload_id,
      mojom::PayloadFilesPtr payload_files,
      mojo::PendingRemote<mojom::FilePayloadListener> listener,
      RegisterPayloadFileCallback callback) override;
  void GetConnectionMetadata(GetConnectionMetadataCallback callback) override;

  void OnConnectionMetadataFetchedFromDelegate(
      GetConnectionMetadataCallback callback,
      mojom::ConnectionMetadataPtr connection_metadata_from_delegate);

  void OnBindingDisconnected();

  void OnRegisterPayloadFileResult(RegisterPayloadFileCallback callback,
                                   mojo::RemoteSetElementId listener_remote_id,
                                   bool success);
  void NotifyFileTransferUpdate(mojo::RemoteSetElementId listener_remote_id,
                                mojom::FileTransferUpdatePtr update);

  raw_ptr<Delegate> delegate_;
  mojo::Receiver<mojom::Channel> receiver_{this};

  // Set of FilePayloadListener remote endpoints passed from
  // RegisterPayloadFile(). These remotes will be removed when their
  // corresponding file transfer has been completed. They will also be
  // automatically removed from the set when their corresponding pipe
  // is disconnected.
  mojo::RemoteSet<mojom::FilePayloadListener> file_payload_listener_remotes_;

  base::WeakPtrFactory<ChannelImpl> weak_ptr_factory_{this};
};

}  // namespace ash::secure_channel

#endif  // CHROMEOS_ASH_SERVICES_SECURE_CHANNEL_CHANNEL_IMPL_H_
