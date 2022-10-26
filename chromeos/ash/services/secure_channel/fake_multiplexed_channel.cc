// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/services/secure_channel/fake_multiplexed_channel.h"

namespace ash::secure_channel {

FakeMultiplexedChannel::FakeMultiplexedChannel(
    Delegate* delegate,
    ConnectionDetails connection_details,
    base::OnceCallback<void(const ConnectionDetails&)> destructor_callback)
    : MultiplexedChannel(delegate, connection_details),
      destructor_callback_(std::move(destructor_callback)) {}

FakeMultiplexedChannel::~FakeMultiplexedChannel() {
  if (destructor_callback_)
    std::move(destructor_callback_).Run(connection_details());
}

void FakeMultiplexedChannel::SetDisconnecting() {
  DCHECK(!is_disconnected_);
  DCHECK(!is_disconnecting_);
  is_disconnecting_ = true;
}

void FakeMultiplexedChannel::SetDisconnected() {
  DCHECK(!is_disconnected_);
  is_disconnecting_ = false;
  is_disconnected_ = true;

  NotifyDisconnected();
}

bool FakeMultiplexedChannel::IsDisconnecting() const {
  return is_disconnecting_;
}

bool FakeMultiplexedChannel::IsDisconnected() const {
  return is_disconnected_;
}

void FakeMultiplexedChannel::PerformAddClientToChannel(
    std::unique_ptr<ClientConnectionParameters> client_connection_parameters) {
  added_clients_.push_back(std::move(client_connection_parameters));
}

FakeMultiplexedChannelDelegate::FakeMultiplexedChannelDelegate() = default;

FakeMultiplexedChannelDelegate::~FakeMultiplexedChannelDelegate() = default;

void FakeMultiplexedChannelDelegate::OnDisconnected(
    const ConnectionDetails& connection_details) {
  disconnected_connection_details_ = connection_details;
}

}  // namespace ash::secure_channel
