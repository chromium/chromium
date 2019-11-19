// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_SERVICES_SECURE_CHANNEL_FAKE_CONNECTION_DELEGATE_H_
#define CHROMEOS_SERVICES_SECURE_CHANNEL_FAKE_CONNECTION_DELEGATE_H_

#include "base/macros.h"
#include "chromeos/services/secure_channel/public/mojom/secure_channel.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace chromeos {

namespace secure_channel {

// Test ConnectionDelegate implementation.
class FakeConnectionDelegate : public mojom::ConnectionDelegate {
 public:
  FakeConnectionDelegate();
  ~FakeConnectionDelegate() override;

  mojo::PendingRemote<mojom::ConnectionDelegate> GenerateRemote();
  void DisconnectGeneratedRemotes();

  const base::Optional<mojom::ConnectionAttemptFailureReason>&
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

 private:
  // mojom::ConnectionDelegate:
  void OnConnectionAttemptFailure(
      mojom::ConnectionAttemptFailureReason reason) override;
  void OnConnection(mojo::PendingRemote<mojom::Channel> channel,
                    mojo::PendingReceiver<mojom::MessageReceiver>
                        message_receiver_receiver) override;

  void OnChannelDisconnected(uint32_t disconnection_reason,
                             const std::string& disconnection_description);

  mojo::ReceiverSet<mojom::ConnectionDelegate> receivers_;
  base::OnceClosure closure_for_next_delegate_callback_;

  base::Optional<mojom::ConnectionAttemptFailureReason>
      connection_attempt_failure_reason_;
  mojo::Remote<mojom::Channel> channel_;
  mojo::PendingReceiver<mojom::MessageReceiver> message_receiver_receiver_;

  DISALLOW_COPY_AND_ASSIGN(FakeConnectionDelegate);
};

}  // namespace secure_channel

}  // namespace chromeos

#endif  // CHROMEOS_SERVICES_SECURE_CHANNEL_FAKE_CONNECTION_DELEGATE_H_
