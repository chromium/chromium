// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/services/secure_channel/public/cpp/client/fake_connection_attempt.h"

namespace chromeos {

namespace secure_channel {

FakeConnectionAttempt::FakeConnectionAttempt() = default;

FakeConnectionAttempt::~FakeConnectionAttempt() = default;

void FakeConnectionAttempt::OnConnectionAttemptFailure(
    mojom::ConnectionAttemptFailureReason reason) {
  ConnectionAttemptImpl::OnConnectionAttemptFailure(reason);
  std::move(on_connection_attempt_failure_callback_).Run();
}

void FakeConnectionAttempt::OnConnection(
    mojo::PendingRemote<mojom::Channel> channel,
    mojo::PendingReceiver<mojom::MessageReceiver> message_receiver_receiver) {
  ConnectionAttemptImpl::OnConnection(std::move(channel),
                                      std::move(message_receiver_receiver));
  std::move(on_connection_callback_).Run();
}

}  // namespace secure_channel

}  // namespace chromeos
