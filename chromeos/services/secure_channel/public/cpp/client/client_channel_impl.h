// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_SERVICES_SECURE_CHANNEL_PUBLIC_CPP_CLIENT_CLIENT_CHANNEL_IMPL_H_
#define CHROMEOS_SERVICES_SECURE_CHANNEL_PUBLIC_CPP_CLIENT_CLIENT_CHANNEL_IMPL_H_

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "chromeos/services/secure_channel/public/cpp/client/client_channel.h"
#include "chromeos/services/secure_channel/public/mojom/secure_channel.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace chromeos {

namespace secure_channel {

// Concrete implementation of ClientChannel.
class ClientChannelImpl : public ClientChannel, public mojom::MessageReceiver {
 public:
  class Factory {
   public:
    static Factory* Get();
    static void SetFactoryForTesting(Factory* test_factory);
    virtual ~Factory();
    virtual std::unique_ptr<ClientChannel> BuildInstance(
        mojo::PendingRemote<mojom::Channel> channel,
        mojo::PendingReceiver<mojom::MessageReceiver>
            message_receiver_receiver);

   private:
    static Factory* test_factory_;
  };

  ~ClientChannelImpl() override;

 private:
  friend class SecureChannelClientChannelImplTest;

  ClientChannelImpl(
      mojo::PendingRemote<mojom::Channel> channel,
      mojo::PendingReceiver<mojom::MessageReceiver> message_receiver_receiver);

  // ClientChannel:
  void PerformGetConnectionMetadata(
      base::OnceCallback<void(mojom::ConnectionMetadataPtr)> callback) override;
  void PerformSendMessage(const std::string& payload,
                          base::OnceClosure on_sent_callback) override;

  // MessageReceiver:
  void OnMessageReceived(const std::string& message) override;

  void OnGetConnectionMetadata(
      base::OnceCallback<void(mojom::ConnectionMetadataPtr)> callback,
      mojom::ConnectionMetadataPtr connection_metadata_ptr);

  void OnChannelDisconnected(uint32_t disconnection_reason,
                             const std::string& disconnection_description);

  void FlushForTesting();

  mojo::Remote<mojom::Channel> channel_;
  mojo::Receiver<mojom::MessageReceiver> receiver_;

  base::WeakPtrFactory<ClientChannelImpl> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(ClientChannelImpl);
};

}  // namespace secure_channel

}  // namespace chromeos

#endif  // CHROMEOS_SERVICES_SECURE_CHANNEL_PUBLIC_CPP_CLIENT_CLIENT_CHANNEL_IMPL_H_
