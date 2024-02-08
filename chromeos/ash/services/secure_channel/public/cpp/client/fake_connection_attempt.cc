// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/services/secure_channel/public/cpp/client/fake_connection_attempt.h"

namespace ash::secure_channel {

FakeConnectionAttempt::FakeConnectionAttempt() = default;

FakeConnectionAttempt::~FakeConnectionAttempt() = default;

void FakeConnectionAttempt::OnConnectionAttemptFailure(
    mojom::ConnectionAttemptFailureReason reason) {
  ConnectionAttemptImpl::OnConnectionAttemptFailure(reason);
  std::move(on_connection_attempt_failure_callback_).Run();
}

void FakeConnectionAttempt::OnConnection(
    mojo::PendingRemote<mojom::Channel> channel,
    mojo::PendingReceiver<mojom::MessageReceiver> message_receiver_receiver,
    mojo::PendingReceiver<mojom::NearbyConnectionStateListener>
        nearby_connection_state_listener) {
  ConnectionAttemptImpl::OnConnection(
      std::move(channel), std::move(message_receiver_receiver),
      std::move(nearby_connection_state_listener));
  std::move(on_connection_callback_).Run();
}

}  // namespace ash::secure_channel
