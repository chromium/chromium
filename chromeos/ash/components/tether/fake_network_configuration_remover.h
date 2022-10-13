// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_TETHER_FAKE_NETWORK_CONFIGURATION_REMOVER_H_
#define CHROMEOS_ASH_COMPONENTS_TETHER_FAKE_NETWORK_CONFIGURATION_REMOVER_H_

#include <string>

#include "chromeos/ash/components/tether/network_configuration_remover.h"

namespace ash {

namespace tether {

// Test double for NetworkConfigurationRemover.
class FakeNetworkConfigurationRemover : public NetworkConfigurationRemover {
 public:
  FakeNetworkConfigurationRemover();

  FakeNetworkConfigurationRemover(const FakeNetworkConfigurationRemover&) =
      delete;
  FakeNetworkConfigurationRemover& operator=(
      const FakeNetworkConfigurationRemover&) = delete;

  ~FakeNetworkConfigurationRemover() override;

  std::string last_removed_wifi_network_path() {
    return last_removed_wifi_network_path_;
  }

  // NetworkConfigurationRemover:
  void RemoveNetworkConfigurationByPath(
      const std::string& wifi_network_path) override;

 private:
  std::string last_removed_wifi_network_path_;
};

}  // namespace tether

}  // namespace ash

#endif  // CHROMEOS_ASH_COMPONENTS_TETHER_FAKE_NETWORK_CONFIGURATION_REMOVER_H_
