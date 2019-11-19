// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_SERVICES_SECURE_CHANNEL_SINGLE_CLIENT_MESSAGE_PROXY_IMPL_H_
#define CHROMEOS_SERVICES_SECURE_CHANNEL_SINGLE_CLIENT_MESSAGE_PROXY_IMPL_H_

#include <string>

#include "base/macros.h"
#include "chromeos/services/secure_channel/channel_impl.h"
#include "chromeos/services/secure_channel/client_connection_parameters.h"
#include "chromeos/services/secure_channel/public/mojom/secure_channel.mojom.h"
#include "chromeos/services/secure_channel/single_client_message_proxy.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace chromeos {

namespace secure_channel {

// Concrete SingleClientMessageProxy implementation, which utilizes a
// ChannelImpl and mojo::Remote<MessageReceiver> to send/receive messages.
class SingleClientMessageProxyImpl : public SingleClientMessageProxy,
                                     public ChannelImpl::Delegate {
 public:
  class Factory {
   public:
    static Factory* Get();
    static void SetInstanceForTesting(Factory* factory);
    virtual ~Factory();
    virtual std::unique_ptr<SingleClientMessageProxy> BuildInstance(
        SingleClientMessageProxy::Delegate* delegate,
        std::unique_ptr<ClientConnectionParameters>
            client_connection_parameters);

   private:
    static Factory* test_factory_;
  };

  ~SingleClientMessageProxyImpl() override;

  // SingleClientMessageProxy:
  const base::UnguessableToken& GetProxyId() override;

 private:
  friend class SecureChannelSingleClientMessageProxyImplTest;

  SingleClientMessageProxyImpl(
      SingleClientMessageProxy::Delegate* delegate,
      std::unique_ptr<ClientConnectionParameters> client_connection_parameters);

  // SingleClientMessageProxy:
  void HandleReceivedMessage(const std::string& feature,
                             const std::string& payload) override;
  void HandleRemoteDeviceDisconnection() override;

  // ChannelImpl::Delegate:
  void OnSendMessageRequested(const std::string& message,
                              base::OnceClosure on_sent_callback) override;
  void GetConnectionMetadata(
      base::OnceCallback<void(mojom::ConnectionMetadataPtr)> callback) override;
  void OnClientDisconnected() override;

  void FlushForTesting();

  std::unique_ptr<ClientConnectionParameters> client_connection_parameters_;
  std::unique_ptr<ChannelImpl> channel_;
  mojo::Remote<mojom::MessageReceiver> message_receiver_remote_;

  DISALLOW_COPY_AND_ASSIGN(SingleClientMessageProxyImpl);
};

}  // namespace secure_channel

}  // namespace chromeos

#endif  // CHROMEOS_SERVICES_SECURE_CHANNEL_SINGLE_CLIENT_MESSAGE_PROXY_IMPL_H_
