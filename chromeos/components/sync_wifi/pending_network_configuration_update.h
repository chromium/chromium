// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_COMPONENTS_SYNC_WIFI_PENDING_NETWORK_CONFIGURATION_UPDATE_H_
#define CHROMEOS_COMPONENTS_SYNC_WIFI_PENDING_NETWORK_CONFIGURATION_UPDATE_H_

#include <string>

#include "base/macros.h"
#include "base/optional.h"
#include "base/unguessable_token.h"
#include "chromeos/components/sync_wifi/network_identifier.h"
#include "components/sync/protocol/wifi_configuration_specifics.pb.h"

namespace chromeos {

namespace sync_wifi {

// Represents a change to the local network stack which hasn't been saved yet,
// including the number of completed attempts to save it.
class PendingNetworkConfigurationUpdate {
 public:
  PendingNetworkConfigurationUpdate(
      const NetworkIdentifier& id,
      const std::string& change_guid,
      const base::Optional<sync_pb::WifiConfigurationSpecificsData>& specifics,
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
  // WifiConfigurationSpecificsData then it is an add or update.
  const base::Optional<sync_pb::WifiConfigurationSpecificsData>& specifics()
      const {
    return specifics_;
  }

  int completed_attempts() { return completed_attempts_; }

  // Returns |true| if the update operation is deleting a network.
  bool IsDeleteOperation() const;

 private:
  NetworkIdentifier id_;
  std::string change_guid_;
  base::Optional<sync_pb::WifiConfigurationSpecificsData> specifics_;
  int completed_attempts_;
};

}  // namespace sync_wifi

}  // namespace chromeos

#endif  // CHROMEOS_COMPONENTS_SYNC_WIFI_PENDING_NETWORK_CONFIGURATION_UPDATE_H_
