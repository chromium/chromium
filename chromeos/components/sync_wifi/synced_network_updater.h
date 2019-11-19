// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_COMPONENTS_SYNC_WIFI_SYNCED_NETWORK_UPDATER_H_
#define CHROMEOS_COMPONENTS_SYNC_WIFI_SYNCED_NETWORK_UPDATER_H_

#include <string>

#include "base/macros.h"

namespace sync_pb {
class WifiConfigurationSpecificsData;
}

namespace chromeos {

namespace sync_wifi {

class NetworkIdentifier;

// Applies updates to synced networks to the local networking stack.
class SyncedNetworkUpdater {
 public:
  virtual ~SyncedNetworkUpdater() = default;

  virtual void AddOrUpdateNetwork(
      const sync_pb::WifiConfigurationSpecificsData& specifics) = 0;
  virtual void RemoveNetwork(const NetworkIdentifier& id) = 0;

 protected:
  SyncedNetworkUpdater() = default;

 private:
  DISALLOW_COPY_AND_ASSIGN(SyncedNetworkUpdater);
};

}  // namespace sync_wifi

}  // namespace chromeos

#endif  // CHROMEOS_COMPONENTS_SYNC_WIFI_SYNCED_NETWORK_UPDATER_H_
