// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_SERVICES_SECURE_CHANNEL_FAKE_CONNECTION_DELEGATE_H_
#define CHROMEOS_ASH_SERVICES_SECURE_CHANNEL_FAKE_CONNECTION_DELEGATE_H_

#include "chromeos/ash/services/secure_channel/public/mojom/nearby_connector.mojom.h"
#include "chromeos/ash/services/secure_channel/public/mojom/secure_channel.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace ash::secure_channel {

// Test ConnectionDelegate implementation.
class FakeConnectionDelegate : public mojom::ConnectionDelegate {
 public:
  FakeConnectionDelegate();

  FakeConnectionDelegate(const FakeConnectionDelegate&) = delete;
  FakeConnectionDelegate& operator=(const FakeConnectionDelegate&) = delete;

  ~FakeConnectionDelegate() override;

  mojo::PendingRemote<mojom::ConnectionDelegate> GenerateRemote();
  void DisconnectGeneratedRemotes();

  const std::optional<mojom::ConnectionAttemptFailureReason>&
  connection_attempt_failure_reason() const {
    return connection_attempt_failure_reason_;
  }

  void set_closure_for_next_delegate_callback(base::OnceClosure closure) {
    closure_for_next_delegate_callback_ = std::move(closure);
  }

  const mojo::Remote<mojom::Channel>& channel() const { return channel_; }

  const mojo::PendingReceiver<mojom::MessageReceiver>&
  message_receiver_receiver() const {
    return message_receiver_receiver_;
  }
  const mojo::PendingReceiver<mojom::NearbyConnectionStateListener>&
  nearby_connection_state_listener_receiver() const {
    return nearby_connection_state_listener_receiver_;
  }

 private:
  // mojom::ConnectionDelegate:
  void OnConnectionAttemptFailure(
      mojom::ConnectionAttemptFailureReason reason) override;
  void OnConnection(
      mojo::PendingRemote<mojom::Channel> channel,
      mojo::PendingReceiver<mojom::MessageReceiver> message_receiver_receiver,
      mojo::PendingReceiver<mojom::NearbyConnectionStateListener>
          nearby_connection_state_listener_receiver) override;

  void OnChannelDisconnected(uint32_t disconnection_reason,
                             const std::string& disconnection_description);

  mojo::ReceiverSet<mojom::ConnectionDelegate> receivers_;
  base::OnceClosure closure_for_next_delegate_callback_;

  std::optional<mojom::ConnectionAttemptFailureReason>
      connection_attempt_failure_reason_;
  mojo::Remote<mojom::Channel> channel_;
  mojo::PendingReceiver<mojom::MessageReceiver> message_receiver_receiver_;
  mojo::PendingReceiver<mojom::NearbyConnectionStateListener>
      nearby_connection_state_listener_receiver_;
};

}  // namespace ash::secure_channel

#endif  // CHROMEOS_ASH_SERVICES_SECURE_CHANNEL_FAKE_CONNECTION_DELEGATE_H_
