// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/services/secure_channel/public/cpp/client/connection_attempt_impl.h"

#include "base/functional/bind.h"
#include "base/memory/ptr_util.h"
#include "chromeos/ash/services/secure_channel/public/cpp/client/client_channel_impl.h"

namespace ash::secure_channel {

// static
ConnectionAttemptImpl::Factory* ConnectionAttemptImpl::Factory::test_factory_ =
    nullptr;

// static
std::unique_ptr<ConnectionAttemptImpl>
ConnectionAttemptImpl::Factory::Create() {
  if (test_factory_)
    return test_factory_->CreateInstance();

  return base::WrapUnique(new ConnectionAttemptImpl());
}

// static
void ConnectionAttemptImpl::Factory::SetFactoryForTesting(
    Factory* test_factory) {
  test_factory_ = test_factory;
}

ConnectionAttemptImpl::Factory::~Factory() = default;

ConnectionAttemptImpl::ConnectionAttemptImpl() = default;

ConnectionAttemptImpl::~ConnectionAttemptImpl() = default;

mojo::PendingRemote<mojom::ConnectionDelegate>
ConnectionAttemptImpl::GenerateRemote() {
  return receiver_.BindNewPipeAndPassRemote();
}

void ConnectionAttemptImpl::OnConnectionAttemptFailure(
    mojom::ConnectionAttemptFailureReason reason) {
  NotifyConnectionAttemptFailure(reason);
}

void ConnectionAttemptImpl::OnConnection(
    mojo::PendingRemote<mojom::Channel> channel,
    mojo::PendingReceiver<mojom::MessageReceiver> message_receiver_receiver,
    mojo::PendingReceiver<mojom::NearbyConnectionStateListener>
        nearby_connection_state_listener_receiver) {
  NotifyConnection(ClientChannelImpl::Factory::Create(
      std::move(channel), std::move(message_receiver_receiver),
      std::move(nearby_connection_state_listener_receiver)));
}

}  // namespace ash::secure_channel
