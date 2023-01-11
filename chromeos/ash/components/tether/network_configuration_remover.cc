// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/tether/network_configuration_remover.h"

#include "base/functional/bind.h"
#include "chromeos/ash/components/multidevice/logging/logging.h"
#include "chromeos/ash/components/network/managed_network_configuration_handler.h"

namespace {

void RemoveConfigurationSuccessCallback(const std::string& path) {
  PA_LOG(VERBOSE) << "Successfully removed Wi-Fi network with path " << path
                  << ".";
}

void RemoveConfigurationFailureCallback(const std::string& path,
                                        const std::string& error_name) {
  PA_LOG(WARNING) << "Failed to remove Wi-Fi network with path " << path
                  << ". Error:" << error_name << ".";
}

}  // namespace

namespace ash {

namespace tether {

NetworkConfigurationRemover::NetworkConfigurationRemover(
    ManagedNetworkConfigurationHandler* managed_network_configuration_handler)
    : managed_network_configuration_handler_(
          managed_network_configuration_handler) {}

NetworkConfigurationRemover::~NetworkConfigurationRemover() = default;

void NetworkConfigurationRemover::RemoveNetworkConfigurationByPath(
    const std::string& wifi_network_path) {
  managed_network_configuration_handler_->RemoveConfiguration(
      wifi_network_path,
      base::BindOnce(&RemoveConfigurationSuccessCallback, wifi_network_path),
      base::BindOnce(&RemoveConfigurationFailureCallback, wifi_network_path));
}

}  // namespace tether

}  // namespace ash
