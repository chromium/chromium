// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/tether/host_connection.h"

namespace ash::tether {

HostConnection::HostConnection(const TetherHost& tether_host,
                               raw_ptr<PayloadListener> payload_listener,
                               OnDisconnectionCallback on_disconnection)
    : tether_host_(tether_host),
      payload_listener_(payload_listener),
      on_disconnection_(std::move(on_disconnection)) {}

HostConnection::~HostConnection() = default;

void HostConnection::ParseMessageAndNotifyListener(const std::string& payload) {
  std::unique_ptr<MessageWrapper> incoming_message =
      MessageWrapper::FromRawMessage(payload);
  if (incoming_message) {
    payload_listener_->OnMessageReceived(std::move(incoming_message));
  }
}

}  // namespace ash::tether
