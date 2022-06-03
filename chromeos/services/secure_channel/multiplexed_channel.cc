// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/services/secure_channel/multiplexed_channel.h"

#include "base/check.h"
#include "base/guid.h"

namespace chromeos {

namespace secure_channel {

MultiplexedChannel::MultiplexedChannel(Delegate* delegate,
                                       ConnectionDetails connection_details)
    : delegate_(delegate), connection_details_(connection_details) {
  DCHECK(delegate);
}

MultiplexedChannel::~MultiplexedChannel() = default;

bool MultiplexedChannel::AddClientToChannel(
    std::unique_ptr<ClientConnectionParameters> client_connection_parameters) {
  if (IsDisconnecting() || IsDisconnected())
    return false;

  PerformAddClientToChannel(std::move(client_connection_parameters));
  return true;
}

void MultiplexedChannel::NotifyDisconnected() {
  delegate_->OnDisconnected(connection_details_);
}

}  // namespace secure_channel

}  // namespace chromeos
