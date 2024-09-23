// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_SERVICES_NETWORK_CONFIG_PUBLIC_CPP_FAKE_CROS_NETWORK_CONFIG_H_
#define CHROMEOS_SERVICES_NETWORK_CONFIG_PUBLIC_CPP_FAKE_CROS_NETWORK_CONFIG_H_

#include <queue>
#include <string>
#include <vector>

#include "chromeos/services/network_config/public/mojom/cros_network_config.mojom.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/bindings/remote_set.h"

namespace chromeos::network_config {

class FakeCrosNetworkConfig : public mojom::CrosNetworkConfig {
 public:
  FakeCrosNetworkConfig();
  FakeCrosNetworkConfig(const FakeCrosNetworkConfig&) = delete;
  FakeCrosNetworkConfig& operator=(const FakeCrosNetworkConfig&) = delete;
  ~FakeCrosNetworkConfig() override;

  void AddObserver(
      mojo::PendingRemote<mojom::CrosNetworkConfigObserver> observer) override;
  void GetNetworkState(const std::string& guid,
                       GetNetworkStateCallback callback) override {}

  // Calls `callback` with network states from `visible_networks_`.
  void GetNetworkStateList(mojom::NetworkFilterPtr filter,
                           GetNetworkStateListCallback callback) override;

  // Calls `callback` with `device_properties_`.
  void GetDeviceStateList(GetDeviceStateListCallback callback) override;

  // Looks for managed properties in `guid_to_managed_properties_`. Calls
  // callback with nullptr is no managed properties were added for `guid`.
  void GetManagedProperties(const std::string& guid,
                            GetManagedPropertiesCallback callback) override;
  void SetProperties(const std::string& guid,
                     mojom::ConfigPropertiesPtr properties,
                     SetPropertiesCallback callback) override {}
  void ConfigureNetwork(mojom::ConfigPropertiesPtr properties,
                        bool shared,
                        ConfigureNetworkCallback callback) override {}
  void ForgetNetwork(const std::string& guid,
                     ForgetNetworkCallback callback) override {}
  void SetNetworkTypeEnabledState(
      mojom::NetworkType type,
      bool enabled,
      SetNetworkTypeEnabledStateCallback callback) override {}
  void SetCellularSimState(mojom::CellularSimStatePtr sim_state,
                           SetCellularSimStateCallback callback) override {}
  void SelectCellularMobileNetwork(
      const std::string& guid,
      const std::string& network_id,
      SelectCellularMobileNetworkCallback callback) override {}

  // Increases the counter of `scan_count_` for the specified network type.
  void RequestNetworkScan(mojom::NetworkType type) override;

  // Calls `callback` with `global_policy_`.
  void GetGlobalPolicy(GetGlobalPolicyCallback callback) override;
  void StartConnect(const std::string& guid,
                    StartConnectCallback callback) override {}
  void StartDisconnect(const std::string& guid,
                       StartDisconnectCallback callback) override {}
  void SetVpnProviders(std::vector<mojom::VpnProviderPtr> providers) override {}

  // Calls `callback` with an empty list.
  void GetVpnProviders(GetVpnProvidersCallback callback) override;
  void GetNetworkCertificates(
      GetNetworkCertificatesCallback callback) override {}
  void GetAlwaysOnVpn(GetAlwaysOnVpnCallback callback) override {}
  void SetAlwaysOnVpn(mojom::AlwaysOnVpnPropertiesPtr properties) override {}
  void GetSupportedVpnTypes(GetSupportedVpnTypesCallback callback) override {}
  void RequestTrafficCounters(
      const std::string& guid,
      RequestTrafficCountersCallback callback) override {}
  void ResetTrafficCounters(const std::string& guid) override {}
  void SetTrafficCountersResetDay(
      const std::string& guid,
      mojom::UInt32ValuePtr day,
      SetTrafficCountersResetDayCallback callback) override {}
  void CreateCustomApn(const std::string& network_guid,
                       chromeos::network_config::mojom::ApnPropertiesPtr apn,
                       CreateCustomApnCallback callback) override;
  void CreateExclusivelyEnabledCustomApn(
      const std::string& network_guid,
      chromeos::network_config::mojom::ApnPropertiesPtr apn,
      CreateExclusivelyEnabledCustomApnCallback callback) override;
  void RemoveCustomApn(const std::string& network_guid,
                       const std::string& apn_id) override {}
  void ModifyCustomApn(const std::string& network_guid,
                       mojom::ApnPropertiesPtr apn) override {}

  // Adds `device_properties` to `device_properties_` or replaces the existing
  // properties for the same network type and calls `OnDeviceStateListChanged`
  // and for all observers in `observers_`.
  void SetDeviceProperties(mojom::DeviceStatePropertiesPtr device_properties);

  // Sets the a value for the `allow_only_policy_cellular_networks` field of
  // `global_policy_`and calls `OnPoliciesApplied` with the value of
  // `global_policy_` for all observers in `observers_`.
  void SetGlobalPolicy(bool allow_only_policy_cellular_networks,
                       bool dns_queries_monitored,
                       bool report_xdr_events_enabled);

  // Sets the connection state for the network in `visible_networks_` with the
  // specified guid and calls `OnActiveNetworksChanged` for all observers in
  // `observers_`.
  void SetNetworkState(const std::string& guid,
                       mojom::ConnectionStateType connection_state_type);

  // Adds `network` to `visible_networks_` and an enabled device for the network
  // type to `device_properties_` and then calls `OnDeviceStateListChanged` and
  // `OnActiveNetworksChanged` for all observers in `observers_`.
  void AddNetworkAndDevice(mojom::NetworkStatePropertiesPtr network);

  // Replaces an existing network in `visible_networks_` list with `network`
  // and then calls `OnActiveNetworksChanged` for all observers in `observers_`.
  void UpdateNetworkProperties(mojom::NetworkStatePropertiesPtr network);

  // Sets managed properties for a specific guid to
  // `guid_to_managed_properties_`. These properties can be later retrieved by
  // calling `GetManagedProperties`.
  void AddManagedProperties(const std::string& guid,
                            mojom::ManagedPropertiesPtr managed_properties);

  // Clears all the networks and devices and calls `OnDeviceStateListChanged`
  // and `OnActiveNetworksChanged` for all observers in `observers_`.
  void ClearNetworksAndDevices();

  // Removes the passed in `index` of the current networks from the list and
  // calls `OnDeviceStateListChanged` for all observer in `observers_`.
  void RemoveNthNetworks(size_t index);

  // Returns how many times `RequestNetworkScan` was requested for the specified
  // network type.
  int GetScanCount(mojom::NetworkType type);

  mojo::PendingRemote<mojom::CrosNetworkConfig> GetPendingRemote();

  const std::vector<mojom::ApnPropertiesPtr>& custom_apns() {
    return custom_apns_;
  }

  void InvokePendingCreateCustomApnCallback(bool success);

 private:
  // Adds `device_properties` to `device_properties_` if there are no device
  // properties for the network type `device_properties->type`, otherwise it
  // replaces the existing device properties.
  void AddOrReplaceDevice(mojom::DeviceStatePropertiesPtr device_properties);

  // Returns a filtered list of `visible_networks_` according to `network_type`
  // and `filter_type`.
  std::vector<mojom::NetworkStatePropertiesPtr> GetFilteredNetworkList(
      mojom::NetworkType network_type,
      mojom::FilterType filter_type);

  // Currently, FakeCrosNetworkConfig only represents visible networks, stored
  // here.
  std::vector<mojom::NetworkStatePropertiesPtr> visible_networks_;
  std::vector<mojom::DeviceStatePropertiesPtr> device_properties_;
  std::map<std::string, mojom::ManagedPropertiesPtr>
      guid_to_managed_properties_;
  mojom::GlobalPolicyPtr global_policy_;
  std::map<mojom::NetworkType, int> scan_count_;
  std::vector<mojom::ApnPropertiesPtr> custom_apns_;
  std::queue<std::pair<CreateCustomApnCallback, mojom::ApnPropertiesPtr>>
      pending_create_custom_apn_callbacks_;
  std::queue<std::pair<CreateExclusivelyEnabledCustomApnCallback,
                       mojom::ApnPropertiesPtr>>
      pending_create_exclusively_enabled_custom_apn_callbacks_;
  mojo::RemoteSet<mojom::CrosNetworkConfigObserver> observers_;
  mojo::Receiver<mojom::CrosNetworkConfig> receiver_{this};
};

}  // namespace chromeos::network_config

#endif  // CHROMEOS_SERVICES_NETWORK_CONFIG_PUBLIC_CPP_FAKE_CROS_NETWORK_CONFIG_H_
