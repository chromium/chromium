// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_NETWORK_STUB_CELLULAR_NETWORKS_PROVIDER_H_
#define CHROMEOS_NETWORK_STUB_CELLULAR_NETWORKS_PROVIDER_H_

#include "chromeos/network/network_state_handler.h"

namespace chromeos {

// Provides stub cellular for use by NetworkStateHandler. In this context,
// cellular stub networks correspond to networks shown exposed by
// NetworkStateHandler which are not backed by Shill.
//
// StubCellularNetworksProvider provides stub networks by utilizing
// CellularESimProfileHandler to obtain information about installed eSIM
// profiles and merge this information with networks known to Shill.
class COMPONENT_EXPORT(CHROMEOS_NETWORK) StubCellularNetworksProvider
    : public NetworkStateHandler::StubCellularNetworksProvider {
 public:
  StubCellularNetworksProvider();
  ~StubCellularNetworksProvider() override;

  void Init(NetworkStateHandler* network_state_handler,
            CellularESimProfileHandler* cellular_esim_profile_handler);

 private:
  friend class StubCellularNetworksProviderTest;

  // NetworkStateHandler::StubCellularNetworksProvider:
  bool AddOrRemoveStubCellularNetworks(
      NetworkStateHandler::ManagedStateList& network_list,
      NetworkStateHandler::ManagedStateList& new_stub_networks,
      const DeviceState* device) override;

  NetworkStateHandler* network_state_handler_ = nullptr;
  CellularESimProfileHandler* cellular_esim_profile_handler_ = nullptr;
};

}  // namespace chromeos

#endif  // CHROMEOS_NETWORK_STUB_CELLULAR_NETWORKS_PROVIDER_H_
