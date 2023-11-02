// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/tether/fake_network_configuration_remover.h"

namespace ash {

namespace tether {

FakeNetworkConfigurationRemover::FakeNetworkConfigurationRemover()
    : NetworkConfigurationRemover(
          nullptr /* managed_network_configuration_handler */) {}

FakeNetworkConfigurationRemover::~FakeNetworkConfigurationRemover() = default;

void FakeNetworkConfigurationRemover::RemoveNetworkConfigurationByPath(
    const std::string& wifi_network_path) {
  last_removed_wifi_network_path_ = wifi_network_path;
}

}  // namespace tether

}  // namespace ash
