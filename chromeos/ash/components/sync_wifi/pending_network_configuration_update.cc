// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/sync_wifi/pending_network_configuration_update.h"

namespace ash::sync_wifi {

PendingNetworkConfigurationUpdate::PendingNetworkConfigurationUpdate(
    const NetworkIdentifier& id,
    const std::string& change_guid,
    const std::optional<sync_pb::WifiConfigurationSpecifics>& specifics,
    int completed_attempts)
    : id_(id),
      change_guid_(change_guid),
      specifics_(specifics),
      completed_attempts_(completed_attempts) {}

PendingNetworkConfigurationUpdate::PendingNetworkConfigurationUpdate(
    const PendingNetworkConfigurationUpdate& update) = default;

PendingNetworkConfigurationUpdate::~PendingNetworkConfigurationUpdate() =
    default;

PendingNetworkConfigurationUpdate& PendingNetworkConfigurationUpdate::operator=(
    PendingNetworkConfigurationUpdate& update) = default;

bool PendingNetworkConfigurationUpdate::IsDeleteOperation() const {
  return !specifics_.has_value();
}

}  // namespace ash::sync_wifi
