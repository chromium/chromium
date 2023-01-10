// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_SERVICES_NETWORK_HEALTH_NETWORK_HEALTH_SERVICE_H_
#define CHROMEOS_ASH_SERVICES_NETWORK_HEALTH_NETWORK_HEALTH_SERVICE_H_

#include <memory>
#include <string>
#include <vector>

#include "base/timer/timer.h"
#include "chromeos/ash/components/mojo_service_manager/mojom/mojo_service_manager.mojom.h"
#include "chromeos/ash/services/network_health/signal_strength_tracker.h"
#include "chromeos/services/network_config/public/cpp/cros_network_config_observer.h"
#include "chromeos/services/network_health/public/mojom/network_health.mojom.h"
#include "chromeos/services/network_health/public/mojom/network_health_types.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/bindings/remote_set.h"

namespace ash::network_health {

class NetworkHealthService
    : public chromeos::network_health::mojom::NetworkHealthService,
      public chromeos::network_config::CrosNetworkConfigObserver,
      public chromeos::mojo_service_manager::mojom::ServiceProvider {
 public:
  NetworkHealthService();

  ~NetworkHealthService() override;

  // Binds this instance to |receiver|.
  void BindReceiver(
      mojo::PendingReceiver<
          chromeos::network_health::mojom::NetworkHealthService> receiver);

  // Returns the current NetworkHealthState.
  const chromeos::network_health::mojom::NetworkHealthState&
  GetNetworkHealthState();

  // Returns the tracked network guids.
  const std::map<std::string, base::Time>& GetTrackedGuidsForTest();

  // NetworkHealthService
  void AddObserver(mojo::PendingRemote<
                   chromeos::network_health::mojom::NetworkEventsObserver>
                       observer) override;
  void GetNetworkList(GetNetworkListCallback) override;
  void GetHealthSnapshot(GetHealthSnapshotCallback) override;
  void GetRecentlyActiveNetworks(GetRecentlyActiveNetworksCallback) override;

  // CrosNetworkConfigObserver
  void OnNetworkStateListChanged() override;
  void OnDeviceStateListChanged() override;
  void OnActiveNetworksChanged(
      std::vector<chromeos::network_config::mojom::NetworkStatePropertiesPtr>
          active_networks) override;
  void OnNetworkStateChanged(
      chromeos::network_config::mojom::NetworkStatePropertiesPtr network_state)
      override;

  // Signal strength changes larger than
  // |kMaxSignalStrengthFluctuationTolerance| trigger a signal strength change
  // event.
  static constexpr int kMaxSignalStrengthFluctuationTolerance = 10;

 protected:
  // Used to set the internal timer. Can be called by derived classes for
  // testing.
  void SetTimer(std::unique_ptr<base::RepeatingTimer> timer);

 private:
  // chromeos::mojo_service_manager::mojom::ServiceProvider overrides.
  void Request(
      chromeos::mojo_service_manager::mojom::ProcessIdentityPtr identity,
      mojo::ScopedMessagePipeHandle receiver) override;

  // Handler for receiving the network state list.
  void OnNetworkStateListReceived(
      std::vector<chromeos::network_config::mojom::NetworkStatePropertiesPtr>);

  // Handler for receiving networking devices.
  void OnDeviceStateListReceived(
      std::vector<chromeos::network_config::mojom::DeviceStatePropertiesPtr>);

  // Creates the NetworkHealthState structure from cached network information.
  void CreateNetworkHealthState();

  // Asynchronous call that refreshes the current Network Health State.
  void RefreshNetworkHealthState();
  void RequestNetworkStateList();
  void RequestDeviceStateList();

  // Finds the matching network using |guid|.
  const chromeos::network_health::mojom::NetworkPtr* FindMatchingNetwork(
      const std::string& guid) const;

  // Handles the case when an active network changes. Also handles the case
  // when a network that was not active becomes active.
  void HandleNetworkEventsForActiveNetworks(
      std::vector<chromeos::network_config::mojom::NetworkStatePropertiesPtr>
          active_networks);

  // Handles the case when an active network becomes no longer active.
  void HandleNetworkEventsForInactiveNetworks(
      chromeos::network_config::mojom::NetworkStatePropertiesPtr network);

  void NotifyObserversConnectionStateChanged(
      const std::string& guid,
      chromeos::network_health::mojom::NetworkState state);
  void NotifyObserversSignalStrengthChanged(const std::string& guid,
                                            int signal_strength);
  void NotifyObserversNetworkListChanged();

  // Checks if a connection state changed has occurred.
  bool ConnectionStateChanged(
      const chromeos::network_health::mojom::NetworkPtr& network,
      const chromeos::network_config::mojom::NetworkStatePropertiesPtr&
          network_state);

  // Checks if a signal strength change event has occurred.
  bool SignalStrengthChanged(
      const chromeos::network_health::mojom::NetworkPtr& network,
      const chromeos::network_config::mojom::NetworkStatePropertiesPtr&
          network_state);

  // Function to add a signal strength sample for each network and update the
  // statistics over time for each network.
  void AnalyzeSignalStrength();

  // Checks if the network with a matching |guid| exists in the network health
  // state.
  bool ExistsInNetworkHealthState(const std::string& guid);

  // Checks if the network is active.
  bool IsActive(const chromeos::network_health::mojom::NetworkPtr& network);

  // Checks if |network|'s guid is being tracked in |tracked_guids|.
  bool IsTracked(const chromeos::network_health::mojom::NetworkPtr& network);

  // Updates the timestamp for active networks.
  void UpdateTrackedGuids();

  // Receiver for mojo service manager service provider.
  mojo::Receiver<chromeos::mojo_service_manager::mojom::ServiceProvider>
      provider_receiver_{this};
  // Remotes for tracking observers that will be notified of network events in
  // the `chromeos::network_health::mojom::NetworkEventsObserver` interface.
  mojo::RemoteSet<chromeos::network_health::mojom::NetworkEventsObserver>
      observers_;
  // Remote for sending requests to the CrosNetworkConfig service.
  mojo::Remote<chromeos::network_config::mojom::CrosNetworkConfig>
      remote_cros_network_config_;
  // Receiver for the CrosNetworkConfigObserver events.
  mojo::Receiver<chromeos::network_config::mojom::CrosNetworkConfigObserver>
      cros_network_config_observer_receiver_{this};
  // Receivers for external requests (WebUI, Feedback, CrosHealthdClient).
  mojo::ReceiverSet<chromeos::network_health::mojom::NetworkHealthService>
      receivers_;

  // Container for storing a running tally of the average signal strength per
  // network GUID.
  std::map<std::string, SignalStrengthTracker> signal_strength_trackers_;
  // Timer that triggers the function to analyze the networks' signal strengths.
  std::unique_ptr<base::RepeatingTimer> timer_;
  // Contains guid to time pairs where each guid represents a network that is
  // either currently active or has been active within the past hour.
  std::map<std::string, base::Time> guid_to_active_time_;
  // Timer that triggers the function to update |tracked_guids_|;
  base::RepeatingTimer tracked_guids_timer_;

  // Contains a list of networking devices and any associated connections. Only
  // networking technologies that are present on the device are included.
  // Networks will be sorted with active connections listed first.
  chromeos::network_health::mojom::NetworkHealthState network_health_state_;
  std::vector<chromeos::network_config::mojom::DeviceStatePropertiesPtr>
      device_properties_;
  std::vector<chromeos::network_config::mojom::NetworkStatePropertiesPtr>
      network_properties_;
};

}  // namespace ash::network_health

#endif  // CHROMEOS_ASH_SERVICES_NETWORK_HEALTH_NETWORK_HEALTH_SERVICE_H_
