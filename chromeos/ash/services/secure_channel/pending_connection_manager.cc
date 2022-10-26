// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/services/secure_channel/pending_connection_manager.h"

#include "chromeos/ash/services/secure_channel/authenticated_channel.h"
#include "chromeos/ash/services/secure_channel/client_connection_parameters.h"
#include "chromeos/ash/services/secure_channel/connection_details.h"

namespace ash::secure_channel {

PendingConnectionManager::PendingConnectionManager(Delegate* delegate)
    : delegate_(delegate) {
  DCHECK(delegate);
}

PendingConnectionManager::~PendingConnectionManager() = default;

void PendingConnectionManager::NotifyOnConnection(
    std::unique_ptr<AuthenticatedChannel> authenticated_channel,
    std::vector<std::unique_ptr<ClientConnectionParameters>> clients,
    const ConnectionDetails& connection_details) {
  delegate_->OnConnection(std::move(authenticated_channel), std::move(clients),
                          connection_details);
}

}  // namespace ash::secure_channel
