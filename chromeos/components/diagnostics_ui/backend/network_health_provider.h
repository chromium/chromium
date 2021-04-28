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
// Stores network state, managed properties, and an observer for a network.
// TODO(michaelcheco): Use NetworkProperties to construct a mojo::Network
// struct and send it to its corresponding observer.
struct NetworkProperties {
  explicit NetworkProperties(
      chromeos::network_config::mojom::NetworkStatePropertiesPtr network_state);
  ~NetworkProperties();
  chromeos::network_config::mojom::NetworkStatePropertiesPtr network_state;
  chromeos::network_config::mojom::ManagedPropertiesPtr managed_properties;
  // TODO(michaelcheco): Add NetworkStateObserver as a member of this struct.
};

using NetworkPropertiesMap = std::map<std::string, NetworkProperties>;

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

  const NetworkPropertiesMap& GetNetworkPropertiesMapForTesting();

 private:
  // Handler for receiving a list of active networks.
  void OnActiveNetworkStateListReceived(
      std::vector<network_config::mojom::NetworkStatePropertiesPtr> networks);

  // Handler for receiving a list of devices.
  void OnDeviceStateListReceived(
      std::vector<network_config::mojom::DeviceStatePropertiesPtr> devices);

  // Handler for receiving managed properties for a network.
  void OnManagedPropertiesReceived(
      const std::string& guid,
      network_config::mojom::ManagedPropertiesPtr managed_properties);

  // Gets ManagedProperties for a network |guid| from CrosNetworkConfig.
  void GetManagedPropertiesForNetwork(const std::string& guid);

  // Map of networks that are active and of a supported
  // type (Ethernet, WiFi, Cellular).
  NetworkPropertiesMap network_properties_map_;

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
