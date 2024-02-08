// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_SERVICES_SECURE_CHANNEL_SINGLE_CLIENT_PROXY_IMPL_H_
#define CHROMEOS_ASH_SERVICES_SECURE_CHANNEL_SINGLE_CLIENT_PROXY_IMPL_H_

#include <string>

#include "base/functional/callback.h"
#include "chromeos/ash/services/secure_channel/channel_impl.h"
#include "chromeos/ash/services/secure_channel/client_connection_parameters.h"
#include "chromeos/ash/services/secure_channel/file_transfer_update_callback.h"
#include "chromeos/ash/services/secure_channel/public/mojom/nearby_connector.mojom.h"
#include "chromeos/ash/services/secure_channel/public/mojom/secure_channel.mojom.h"
#include "chromeos/ash/services/secure_channel/single_client_proxy.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace ash::secure_channel {

// Concrete SingleClientProxy implementation, which forwards client requests to
// its delegate, and utilizes a mojo::Remote<MessageReceiver> to receive
// incoming messages.
class SingleClientProxyImpl : public SingleClientProxy,
                              public ChannelImpl::Delegate {
 public:
  class Factory {
   public:
    static std::unique_ptr<SingleClientProxy> Create(
        SingleClientProxy::Delegate* delegate,
        std::unique_ptr<ClientConnectionParameters>
            client_connection_parameters);
    static void SetFactoryForTesting(Factory* factory);

   protected:
    virtual ~Factory();
    virtual std::unique_ptr<SingleClientProxy> CreateInstance(
        SingleClientProxy::Delegate* delegate,
        std::unique_ptr<ClientConnectionParameters>
            client_connection_parameters) = 0;

   private:
    static Factory* test_factory_;
  };

  SingleClientProxyImpl(const SingleClientProxyImpl&) = delete;
  SingleClientProxyImpl& operator=(const SingleClientProxyImpl&) = delete;
  ~SingleClientProxyImpl() override;

  // SingleClientProxy:
  const base::UnguessableToken& GetProxyId() override;

 private:
  friend class SecureChannelSingleClientProxyImplTest;

  SingleClientProxyImpl(
      SingleClientProxy::Delegate* delegate,
      std::unique_ptr<ClientConnectionParameters> client_connection_parameters);

  // SingleClientProxy:
  void HandleReceivedMessage(const std::string& feature,
                             const std::string& payload) override;
  void HandleRemoteDeviceDisconnection() override;
  void HandleNearbyConnectionStateChanged(
      mojom::NearbyConnectionStep step,
      mojom::NearbyConnectionStepResult result) override;

  // ChannelImpl::Delegate:
  void OnSendMessageRequested(const std::string& message,
                              base::OnceClosure on_sent_callback) override;
  void RegisterPayloadFile(
      int64_t payload_id,
      mojom::PayloadFilesPtr payload_files,
      FileTransferUpdateCallback file_transfer_update_callback,
      base::OnceCallback<void(bool)> registration_result_callback) override;

  void GetConnectionMetadata(
      base::OnceCallback<void(mojom::ConnectionMetadataPtr)> callback) override;
  void OnClientDisconnected() override;

  void FlushForTesting();

  std::unique_ptr<ClientConnectionParameters> client_connection_parameters_;
  std::unique_ptr<ChannelImpl> channel_;
  mojo::Remote<mojom::MessageReceiver> message_receiver_remote_;
  mojo::Remote<mojom::NearbyConnectionStateListener>
      nearby_connection_state_listener_remote_;
};

}  // namespace ash::secure_channel

#endif  // CHROMEOS_ASH_SERVICES_SECURE_CHANNEL_SINGLE_CLIENT_PROXY_IMPL_H_
