// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_SERVICES_SECURE_CHANNEL_CLIENT_CONNECTION_PARAMETERS_IMPL_H_
#define CHROMEOS_SERVICES_SECURE_CHANNEL_CLIENT_CONNECTION_PARAMETERS_IMPL_H_

#include "base/macros.h"
#include "chromeos/services/secure_channel/client_connection_parameters.h"
#include "chromeos/services/secure_channel/public/mojom/secure_channel.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace chromeos {

namespace secure_channel {

// Concrete ClientConnectionParameters implementation, which utilizes a
// mojo::Remote<ConnectionDelegate>.
class ClientConnectionParametersImpl : public ClientConnectionParameters {
 public:
  class Factory {
   public:
    static Factory* Get();
    static void SetFactoryForTesting(Factory* test_factory);
    virtual ~Factory();
    virtual std::unique_ptr<ClientConnectionParameters> BuildInstance(
        const std::string& feature,
        mojo::PendingRemote<mojom::ConnectionDelegate>
            connection_delegate_remote);

   private:
    static Factory* test_factory_;
  };

  ~ClientConnectionParametersImpl() override;

 private:
  ClientConnectionParametersImpl(const std::string& feature,
                                 mojo::PendingRemote<mojom::ConnectionDelegate>
                                     connection_delegate_remote);

  // ClientConnectionParameters:
  bool HasClientCanceledRequest() override;
  void PerformSetConnectionAttemptFailed(
      mojom::ConnectionAttemptFailureReason reason) override;
  void PerformSetConnectionSucceeded(
      mojo::PendingRemote<mojom::Channel> channel,
      mojo::PendingReceiver<mojom::MessageReceiver> message_receiver_receiver)
      override;

  void OnConnectionDelegateRemoteDisconnected();

  mojo::Remote<mojom::ConnectionDelegate> connection_delegate_remote_;

  DISALLOW_COPY_AND_ASSIGN(ClientConnectionParametersImpl);
};

}  // namespace secure_channel

}  // namespace chromeos

#endif  // CHROMEOS_SERVICES_SECURE_CHANNEL_CLIENT_CONNECTION_PARAMETERS_IMPL_H_
