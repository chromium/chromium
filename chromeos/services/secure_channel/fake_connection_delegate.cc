// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/services/secure_channel/fake_connection_delegate.h"

#include "base/bind.h"

namespace chromeos {

namespace secure_channel {

FakeConnectionDelegate::FakeConnectionDelegate() = default;

FakeConnectionDelegate::~FakeConnectionDelegate() = default;

mojo::PendingRemote<mojom::ConnectionDelegate>
FakeConnectionDelegate::GenerateRemote() {
  mojo::PendingRemote<mojom::ConnectionDelegate> remote;
  receivers_.Add(this, remote.InitWithNewPipeAndPassReceiver());
  return remote;
}

void FakeConnectionDelegate::DisconnectGeneratedRemotes() {
  receivers_.Clear();
}

void FakeConnectionDelegate::OnConnectionAttemptFailure(
    mojom::ConnectionAttemptFailureReason reason) {
  connection_attempt_failure_reason_ = reason;

  if (closure_for_next_delegate_callback_)
    std::move(closure_for_next_delegate_callback_).Run();
}

void FakeConnectionDelegate::OnConnection(
    mojo::PendingRemote<mojom::Channel> channel,
    mojo::PendingReceiver<mojom::MessageReceiver> message_receiver_receiver) {
  channel_.Bind(std::move(channel));
  message_receiver_receiver_ = std::move(message_receiver_receiver);

  if (closure_for_next_delegate_callback_)
    std::move(closure_for_next_delegate_callback_).Run();
}

}  // namespace secure_channel

}  // namespace chromeos
