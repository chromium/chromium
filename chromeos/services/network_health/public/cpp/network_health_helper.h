// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_SERVICES_NETWORK_HEALTH_PUBLIC_CPP_NETWORK_HEALTH_HELPER_H_
#define CHROMEOS_SERVICES_NETWORK_HEALTH_PUBLIC_CPP_NETWORK_HEALTH_HELPER_H_

#include <memory>
#include <string>
#include <vector>

#include "base/callback.h"
#include "chromeos/services/network_health/public/mojom/network_health.mojom.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace chromeos::network_health {

class NetworkHealthService;

// Helper class intended to run in either the Ash or Lacros process for
// accessing properties from the chromeos.network_health API.
// TODO(b/247618374): Lacros implementation.
class NetworkHealthHelper : public mojom::NetworkEventsObserver {
 public:
  NetworkHealthHelper();
  NetworkHealthHelper(const NetworkHealthHelper&) = delete;
  NetworkHealthHelper& operator=(const NetworkHealthHelper&) = delete;
  ~NetworkHealthHelper() override;

  // Asynchronous getters. If a cached result is available, |callback| will be
  // invoked immediately. Otherwise the properties will be requested and
  // |callback| will be invoked when they are available.
  void RequestDefaultNetwork(
      base::OnceCallback<void(mojom::NetworkPtr&)> callback);
  void RequestIsPortalState(base::OnceCallback<void(bool)> callback);

  // ash::network_health::mojom::NetworkEventsObserver:
  void OnConnectionStateChanged(const std::string& guid,
                                mojom::NetworkState state) override;
  void OnSignalStrengthChanged(const std::string& guid,
                               mojom::UInt32ValuePtr signal_strength) override;
  void OnNetworkListChanged(std::vector<mojom::NetworkPtr> networks) override;

  static std::unique_ptr<NetworkHealthHelper> CreateForTesting(
      NetworkHealthService* network_health_service);

 private:
  // Private constructor for tests to override the NetworkHealthService.
  explicit NetworkHealthHelper(NetworkHealthService* network_health_service);

  mojom::NetworkHealthService* GetNetworkHealthService();
  void SetupRemote(NetworkHealthService* network_health_service);
  void RequestNetworks();
  void NetworkListReceived(std::vector<mojom::NetworkPtr> networks);
  void OnMojoError();

  mojo::Remote<mojom::NetworkHealthService> remote_;
  mojo::Receiver<mojom::NetworkEventsObserver> observer_receiver_{this};

  mojom::NetworkPtr default_network_;
  bool networks_received_ = false;
  std::vector<base::OnceCallback<void(mojom::NetworkPtr&)>>
      received_networks_callbacks_;
};

}  // namespace chromeos::network_health

#endif  // CHROMEOS_SERVICES_NETWORK_HEALTH_PUBLIC_CPP_NETWORK_HEALTH_HELPER_H_
