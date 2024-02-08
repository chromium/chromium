// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_SERVICES_SECURE_CHANNEL_PUBLIC_CPP_CLIENT_CONNECTION_ATTEMPT_IMPL_H_
#define CHROMEOS_ASH_SERVICES_SECURE_CHANNEL_PUBLIC_CPP_CLIENT_CONNECTION_ATTEMPT_IMPL_H_

#include "base/memory/weak_ptr.h"
#include "chromeos/ash/services/secure_channel/public/cpp/client/connection_attempt.h"
#include "chromeos/ash/services/secure_channel/public/mojom/secure_channel.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"

namespace ash::secure_channel {

// Concrete implementation of ConnectionAttempt.
class ConnectionAttemptImpl : public ConnectionAttempt,
                              public mojom::ConnectionDelegate {
 public:
  class Factory {
   public:
    static std::unique_ptr<ConnectionAttemptImpl> Create();
    static void SetFactoryForTesting(Factory* test_factory);

   protected:
    virtual ~Factory();
    virtual std::unique_ptr<ConnectionAttemptImpl> CreateInstance() = 0;

   private:
    static Factory* test_factory_;
  };

  ConnectionAttemptImpl(const ConnectionAttemptImpl&) = delete;
  ConnectionAttemptImpl& operator=(const ConnectionAttemptImpl&) = delete;

  ~ConnectionAttemptImpl() override;

  mojo::PendingRemote<mojom::ConnectionDelegate> GenerateRemote();

 protected:
  ConnectionAttemptImpl();

  // mojom::ConnectionDelegate:
  void OnConnectionAttemptFailure(
      mojom::ConnectionAttemptFailureReason reason) override;
  void OnConnection(
      mojo::PendingRemote<mojom::Channel> channel,
      mojo::PendingReceiver<mojom::MessageReceiver> message_receiver_receiver,
      mojo::PendingReceiver<mojom::NearbyConnectionStateListener>
          nearby_connection_state_listener_receiver) override;

 private:
  mojo::Receiver<mojom::ConnectionDelegate> receiver_{this};

  base::WeakPtrFactory<ConnectionAttemptImpl> weak_ptr_factory_{this};
};

}  // namespace ash::secure_channel

#endif  // CHROMEOS_ASH_SERVICES_SECURE_CHANNEL_PUBLIC_CPP_CLIENT_CONNECTION_ATTEMPT_IMPL_H_
