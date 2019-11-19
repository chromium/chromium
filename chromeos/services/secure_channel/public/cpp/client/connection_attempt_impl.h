// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_SERVICES_SECURE_CHANNEL_PUBLIC_CPP_CLIENT_CONNECTION_ATTEMPT_IMPL_H_
#define CHROMEOS_SERVICES_SECURE_CHANNEL_PUBLIC_CPP_CLIENT_CONNECTION_ATTEMPT_IMPL_H_

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "chromeos/services/secure_channel/public/cpp/client/connection_attempt.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"

namespace chromeos {

namespace secure_channel {

// Concrete implementation of ConnectionAttempt.
class ConnectionAttemptImpl : public ConnectionAttempt,
                              public mojom::ConnectionDelegate {
 public:
  class Factory {
   public:
    static Factory* Get();
    static void SetFactoryForTesting(Factory* test_factory);
    virtual ~Factory();
    virtual std::unique_ptr<ConnectionAttemptImpl> BuildInstance();

   private:
    static Factory* test_factory_;
  };

  ~ConnectionAttemptImpl() override;

  mojo::PendingRemote<mojom::ConnectionDelegate> GenerateRemote();

 protected:
  ConnectionAttemptImpl();

  // mojom::ConnectionDelegate:
  void OnConnectionAttemptFailure(
      mojom::ConnectionAttemptFailureReason reason) override;
  void OnConnection(mojo::PendingRemote<mojom::Channel> channel,
                    mojo::PendingReceiver<mojom::MessageReceiver>
                        message_receiver_receiver) override;

 private:
  mojo::Receiver<mojom::ConnectionDelegate> receiver_{this};

  base::WeakPtrFactory<ConnectionAttemptImpl> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(ConnectionAttemptImpl);
};

}  // namespace secure_channel

}  // namespace chromeos

#endif  // CHROMEOS_SERVICES_SECURE_CHANNEL_PUBLIC_CPP_CLIENT_CONNECTION_ATTEMPT_IMPL_H_
