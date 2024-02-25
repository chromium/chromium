// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_SERVICES_SECURE_CHANNEL_PUBLIC_CPP_CLIENT_FAKE_CONNECTION_ATTEMPT_H_
#define CHROMEOS_ASH_SERVICES_SECURE_CHANNEL_PUBLIC_CPP_CLIENT_FAKE_CONNECTION_ATTEMPT_H_

#include "base/memory/weak_ptr.h"
#include "chromeos/ash/services/secure_channel/public/cpp/client/connection_attempt_impl.h"
#include "chromeos/ash/services/secure_channel/public/mojom/nearby_connector.mojom-forward.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"

namespace ash::secure_channel {

// Test double implementation of ConnectionAttemptImpl.
class FakeConnectionAttempt : public ConnectionAttemptImpl {
 public:
  FakeConnectionAttempt();

  FakeConnectionAttempt(const FakeConnectionAttempt&) = delete;
  FakeConnectionAttempt& operator=(const FakeConnectionAttempt&) = delete;

  ~FakeConnectionAttempt() override;

  using ConnectionAttempt::NotifyConnection;
  using ConnectionAttempt::NotifyConnectionAttemptFailure;

  // ConnectionAttemptImpl:
  void OnConnectionAttemptFailure(
      mojom::ConnectionAttemptFailureReason reason) override;
  void OnConnection(
      mojo::PendingRemote<mojom::Channel> channel,
      mojo::PendingReceiver<mojom::MessageReceiver> message_receiver_receiver,
      mojo::PendingReceiver<mojom::NearbyConnectionStateListener>
          nearby_connection_state_listener) override;

  void set_on_connection_attempt_failure_callback(base::OnceClosure callback) {
    on_connection_attempt_failure_callback_ = std::move(callback);
  }

  void set_on_connection_callback(base::OnceClosure callback) {
    on_connection_callback_ = std::move(callback);
  }

 private:
  base::OnceClosure on_connection_attempt_failure_callback_;
  base::OnceClosure on_connection_callback_;
};

}  // namespace ash::secure_channel

#endif  // CHROMEOS_ASH_SERVICES_SECURE_CHANNEL_PUBLIC_CPP_CLIENT_FAKE_CONNECTION_ATTEMPT_H_
