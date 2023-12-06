// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_SYNC_WIFI_PENDING_NETWORK_CONFIGURATION_UPDATE_H_
#define CHROMEOS_ASH_COMPONENTS_SYNC_WIFI_PENDING_NETWORK_CONFIGURATION_UPDATE_H_

#include <optional>
#include <string>

#include "base/unguessable_token.h"
#include "chromeos/ash/components/sync_wifi/network_identifier.h"
#include "components/sync/protocol/wifi_configuration_specifics.pb.h"

namespace ash::sync_wifi {

// Represents a change to the local network stack which hasn't been saved yet,
// including the number of completed attempts to save it.
class PendingNetworkConfigurationUpdate {
 public:
  PendingNetworkConfigurationUpdate(
      const NetworkIdentifier& id,
      const std::string& change_guid,
      const std::optional<sync_pb::WifiConfigurationSpecifics>& specifics,
      int completed_attempts);
  PendingNetworkConfigurationUpdate(
      const PendingNetworkConfigurationUpdate& update);
  PendingNetworkConfigurationUpdate& operator=(
      PendingNetworkConfigurationUpdate& update);
  virtual ~PendingNetworkConfigurationUpdate();

  // The identifier for the network.
  const NetworkIdentifier& id() const { return id_; }

  // A unique ID for each change.
  const std::string& change_guid() const { return change_guid_; }

  // When null, this is a delete operation, if there is a
  // WifiConfigurationSpecifics then it is an add or update.
  const std::optional<sync_pb::WifiConfigurationSpecifics>& specifics() const {
    return specifics_;
  }

  int completed_attempts() { return completed_attempts_; }

  // Returns |true| if the update operation is deleting a network.
  bool IsDeleteOperation() const;

 private:
  friend class FakePendingNetworkConfigurationTracker;

  void SetCompletedAttemptsForTesting(int completed_attempts) {
    completed_attempts_ = completed_attempts;
  }

  NetworkIdentifier id_;
  std::string change_guid_;
  std::optional<sync_pb::WifiConfigurationSpecifics> specifics_;
  int completed_attempts_;
};

}  // namespace ash::sync_wifi

#endif  // CHROMEOS_ASH_COMPONENTS_SYNC_WIFI_PENDING_NETWORK_CONFIGURATION_UPDATE_H_
