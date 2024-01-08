// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_TETHER_NETWORK_CONFIGURATION_REMOVER_H_
#define CHROMEOS_ASH_COMPONENTS_TETHER_NETWORK_CONFIGURATION_REMOVER_H_

#include <string>

#include "base/memory/raw_ptr.h"

namespace ash {

class ManagedNetworkConfigurationHandler;

namespace tether {

// Handles the removal of the configuration of a Wi-Fi network.
class NetworkConfigurationRemover {
 public:
  NetworkConfigurationRemover(ManagedNetworkConfigurationHandler*
                                  managed_network_configuration_handler);

  NetworkConfigurationRemover(const NetworkConfigurationRemover&) = delete;
  NetworkConfigurationRemover& operator=(const NetworkConfigurationRemover&) =
      delete;

  virtual ~NetworkConfigurationRemover();

  // Remove the network configuration of the Wi-Fi hotspot referenced by
  // |wifi_network_path|.
  virtual void RemoveNetworkConfigurationByPath(
      const std::string& wifi_network_path);

 private:
  friend class NetworkConfigurationRemoverTest;

  raw_ptr<ManagedNetworkConfigurationHandler>
      managed_network_configuration_handler_;
};

}  // namespace tether

}  // namespace ash

#endif  // CHROMEOS_ASH_COMPONENTS_TETHER_NETWORK_CONFIGURATION_REMOVER_H_
