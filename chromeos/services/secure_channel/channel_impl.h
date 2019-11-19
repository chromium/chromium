// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_SERVICES_SECURE_CHANNEL_CHANNEL_IMPL_H_
#define CHROMEOS_SERVICES_SECURE_CHANNEL_CHANNEL_IMPL_H_

#include <string>

#include "base/callback_forward.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "chromeos/services/secure_channel/public/mojom/secure_channel.mojom.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"

namespace chromeos {

namespace secure_channel {

// Channel which provides clients the ability to send messages to remote devices
// and to listen for disconnections from those devices. To be notified when a
// channel becomes disconnected, clients should use
// set_connection_error_with_reason_handler() and wait for a connection error
// with reason mojom::Channel::kConnectionDroppedReason.
class ChannelImpl : public mojom::Channel {
 public:
  class Delegate {
   public:
    virtual ~Delegate() = default;
    virtual void OnSendMessageRequested(const std::string& message,
                                        base::OnceClosure on_sent_callback) = 0;
    virtual void GetConnectionMetadata(
        base::OnceCallback<void(mojom::ConnectionMetadataPtr)> callback) = 0;
    virtual void OnClientDisconnected() = 0;
  };

  explicit ChannelImpl(Delegate* delegate);
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
  void GetConnectionMetadata(GetConnectionMetadataCallback callback) override;

  void OnConnectionMetadataFetchedFromDelegate(
      GetConnectionMetadataCallback callback,
      mojom::ConnectionMetadataPtr connection_metadata_from_delegate);

  void OnBindingDisconnected();

  Delegate* delegate_;
  mojo::Receiver<mojom::Channel> receiver_{this};

  base::WeakPtrFactory<ChannelImpl> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(ChannelImpl);
};

}  // namespace secure_channel

}  // namespace chromeos

#endif  // CHROMEOS_SERVICES_SECURE_CHANNEL_CHANNEL_IMPL_H_
