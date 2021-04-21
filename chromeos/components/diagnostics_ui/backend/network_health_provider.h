// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_COMPONENTS_DIAGNOSTICS_UI_BACKEND_NETWORK_HEALTH_PROVIDER_H_
#define CHROMEOS_COMPONENTS_DIAGNOSTICS_UI_BACKEND_NETWORK_HEALTH_PROVIDER_H_

#include <map>
#include <string>
#include <vector>

#include "chromeos/services/network_config/public/mojom/cros_network_config.mojom.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace chromeos {
namespace diagnostics {

using DeviceMap = std::map<network_config::mojom::NetworkType,
                           network_config::mojom::DeviceStatePropertiesPtr>;

class NetworkHealthProvider
    : public network_config::mojom::CrosNetworkConfigObserver {
 public:
  NetworkHealthProvider();

  NetworkHealthProvider(const NetworkHealthProvider&) = delete;
  NetworkHealthProvider& operator=(const NetworkHealthProvider&) = delete;

  ~NetworkHealthProvider() override;

  // CrosNetworkConfigObserver
  void OnNetworkStateListChanged() override;
  void OnDeviceStateListChanged() override;
  void OnActiveNetworksChanged(
      std::vector<network_config::mojom::NetworkStatePropertiesPtr>
          active_networks) override;
  void OnNetworkStateChanged(
      network_config::mojom::NetworkStatePropertiesPtr network_state) override;
  void OnVpnProvidersChanged() override;
  void OnNetworkCertificatesChanged() override;

  std::vector<std::string> GetNetworkGuidListForTesting();

  const DeviceMap& GetDeviceTypeMapForTesting();

 private:
  // Handler for receiving a list of active networks.
  void OnActiveNetworkStateListReceived(
      std::vector<network_config::mojom::NetworkStatePropertiesPtr> networks);

  // Handler for receiving a list of devices.
  void OnDeviceStateListReceived(
      std::vector<network_config::mojom::DeviceStatePropertiesPtr> devices);

  // Map of networks that are active and of a supported
  // type (Ethernet, WiFi, Cellular).
  std::map<std::string, network_config::mojom::NetworkStatePropertiesPtr>
      guid_to_network_map;

  // Maps device type to device properties, used to find corresponding device
  // for a network.
  DeviceMap device_type_map_;

  // Remote for sending requests to the CrosNetworkConfig service.
  mojo::Remote<network_config::mojom::CrosNetworkConfig>
      remote_cros_network_config_;

  // Receiver for the CrosNetworkConfigObserver events.
  mojo::Receiver<network_config::mojom::CrosNetworkConfigObserver>
      cros_network_config_observer_receiver_{this};
};

}  // namespace diagnostics
}  // namespace chromeos

#endif  // CHROMEOS_COMPONENTS_DIAGNOSTICS_UI_BACKEND_NETWORK_HEALTH_PROVIDER_H_
