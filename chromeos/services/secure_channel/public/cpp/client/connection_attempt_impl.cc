// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/services/secure_channel/public/cpp/client/connection_attempt_impl.h"

#include "base/bind.h"
#include "base/no_destructor.h"
#include "chromeos/services/secure_channel/public/cpp/client/client_channel_impl.h"

namespace chromeos {

namespace secure_channel {

// static
ConnectionAttemptImpl::Factory* ConnectionAttemptImpl::Factory::test_factory_ =
    nullptr;

// static
ConnectionAttemptImpl::Factory* ConnectionAttemptImpl::Factory::Get() {
  if (test_factory_)
    return test_factory_;

  static base::NoDestructor<ConnectionAttemptImpl::Factory> factory;
  return factory.get();
}

// static
void ConnectionAttemptImpl::Factory::SetFactoryForTesting(
    Factory* test_factory) {
  test_factory_ = test_factory;
}

ConnectionAttemptImpl::Factory::~Factory() = default;

std::unique_ptr<ConnectionAttemptImpl>
ConnectionAttemptImpl::Factory::BuildInstance() {
  return base::WrapUnique(new ConnectionAttemptImpl());
}

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
    mojo::PendingReceiver<mojom::MessageReceiver> message_receiver_receiver) {
  NotifyConnection(ClientChannelImpl::Factory::Get()->BuildInstance(
      std::move(channel), std::move(message_receiver_receiver)));
}

}  // namespace secure_channel

}  // namespace chromeos
