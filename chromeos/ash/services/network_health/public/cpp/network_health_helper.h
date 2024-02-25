// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_SERVICES_NETWORK_HEALTH_PUBLIC_CPP_NETWORK_HEALTH_HELPER_H_
#define CHROMEOS_ASH_SERVICES_NETWORK_HEALTH_PUBLIC_CPP_NETWORK_HEALTH_HELPER_H_

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "chromeos/services/network_health/public/mojom/network_health.mojom.h"
#include "chromeos/services/network_health/public/mojom/network_health_types.mojom.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace ash::network_health {

class NetworkHealthService;

// Helper class intended to run in the Ash process for accessing properties
// from the chromeos.network_health API.
class NetworkHealthHelper
    : public chromeos::network_health::mojom::NetworkEventsObserver {
 public:
  NetworkHealthHelper();
  NetworkHealthHelper(const NetworkHealthHelper&) = delete;
  NetworkHealthHelper& operator=(const NetworkHealthHelper&) = delete;
  ~NetworkHealthHelper() override;

  // If the default network is set and is a WiFi network, returns the portal
  // state of the default network, otherwise returns kUnknown.
  chromeos::network_config::mojom::PortalState WiFiPortalState();

  // chromeos::network_health::mojom::NetworkEventsObserver:
  void OnConnectionStateChanged(
      const std::string& guid,
      chromeos::network_health::mojom::NetworkState state) override;
  void OnSignalStrengthChanged(
      const std::string& guid,
      chromeos::network_health::mojom::UInt32ValuePtr signal_strength) override;
  void OnNetworkListChanged(
      std::vector<chromeos::network_health::mojom::NetworkPtr> networks)
      override;

  chromeos::network_health::mojom::Network* default_network() {
    return default_network_.get();
  }

  static std::unique_ptr<NetworkHealthHelper> CreateForTesting(
      NetworkHealthService* network_health_service);

 private:
  // Private constructor for tests to override the NetworkHealthService.
  explicit NetworkHealthHelper(NetworkHealthService* network_health_service);

  chromeos::network_health::mojom::NetworkHealthService*
  GetNetworkHealthService();
  void SetupRemote(NetworkHealthService* network_health_service);
  void RequestNetworks();
  void NetworkListReceived(
      std::vector<chromeos::network_health::mojom::NetworkPtr> networks);
  void OnMojoError();

  mojo::Remote<chromeos::network_health::mojom::NetworkHealthService> remote_;
  mojo::Receiver<chromeos::network_health::mojom::NetworkEventsObserver>
      observer_receiver_{this};

  chromeos::network_health::mojom::NetworkPtr default_network_;
};

}  // namespace ash::network_health

#endif  // CHROMEOS_ASH_SERVICES_NETWORK_HEALTH_PUBLIC_CPP_NETWORK_HEALTH_HELPER_H_
