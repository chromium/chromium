// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_TETHER_NETWORK_LIST_SORTER_H_
#define CHROMEOS_ASH_COMPONENTS_TETHER_NETWORK_LIST_SORTER_H_

#include "chromeos/ash/components/multidevice/remote_device_ref.h"
#include "chromeos/ash/components/network/network_state_handler.h"

namespace ash {

namespace tether {

// Sorts networks in the order that they should be displayed in network
// configuration UI. The sort order is be determined by network properties
// (e.g., connected devices before unconnected; higher signal strength before
// lower signal strength).
class NetworkListSorter : public NetworkStateHandler::TetherSortDelegate {
 public:
  NetworkListSorter();

  NetworkListSorter(const NetworkListSorter&) = delete;
  NetworkListSorter& operator=(const NetworkListSorter&) = delete;

  virtual ~NetworkListSorter();

  // NetworkStateHandler::TetherNetworkListSorter:
  void SortTetherNetworkList(
      NetworkStateHandler::ManagedStateList* tether_networks) const override;
};

}  // namespace tether

}  // namespace ash

#endif  // CHROMEOS_ASH_COMPONENTS_TETHER_NETWORK_LIST_SORTER_H_
