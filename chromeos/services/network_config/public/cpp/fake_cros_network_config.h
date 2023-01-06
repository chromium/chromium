// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_SERVICES_NETWORK_CONFIG_PUBLIC_CPP_FAKE_CROS_NETWORK_CONFIG_H_
#define CHROMEOS_SERVICES_NETWORK_CONFIG_PUBLIC_CPP_FAKE_CROS_NETWORK_CONFIG_H_

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

  void AddObserver(mojo::PendingRemote<
                   chromeos::network_config::mojom::CrosNetworkConfigObserver>
                       observer) override;
  void GetNetworkState(const std::string& guid,
                       GetNetworkStateCallback callback) override {}

  // Calls `callback` with network states from `visible_networks_`.
  void GetNetworkStateList(
      chromeos::network_config::mojom::NetworkFilterPtr filter,
      GetNetworkStateListCallback callback) override;

  // Calls `callback` with `device_properties_`.
  void GetDeviceStateList(GetDeviceStateListCallback callback) override;

  // Looks for managed properties in `guid_to_managed_properties_`. Calls
  // callback with nullptr is no managed properties were added for `guid`.
  void GetManagedProperties(const std::string& guid,
                            GetManagedPropertiesCallback callback) override;
  void SetProperties(
      const std::string& guid,
      chromeos::network_config::mojom::ConfigPropertiesPtr properties,
      SetPropertiesCallback callback) override {}
  void ConfigureNetwork(
      chromeos::network_config::mojom::ConfigPropertiesPtr properties,
      bool shared,
      ConfigureNetworkCallback callback) override {}
  void ForgetNetwork(const std::string& guid,
                     ForgetNetworkCallback callback) override {}
  void SetNetworkTypeEnabledState(
      chromeos::network_config::mojom::NetworkType type,
      bool enabled,
      SetNetworkTypeEnabledStateCallback callback) override {}
  void SetCellularSimState(
      chromeos::network_config::mojom::CellularSimStatePtr sim_state,
      SetCellularSimStateCallback callback) override {}
  void SelectCellularMobileNetwork(
      const std::string& guid,
      const std::string& network_id,
      SelectCellularMobileNetworkCallback callback) override {}

  // Increases the counter of `scan_count_` for the specified network type.
  void RequestNetworkScan(
      chromeos::network_config::mojom::NetworkType type) override;

  // Calls `callback` with `global_policy_`.
  void GetGlobalPolicy(GetGlobalPolicyCallback callback) override;
  void StartConnect(const std::string& guid,
                    StartConnectCallback callback) override {}
  void StartDisconnect(const std::string& guid,
                       StartDisconnectCallback callback) override {}
  void SetVpnProviders(
      std::vector<chromeos::network_config::mojom::VpnProviderPtr> providers)
      override {}

  // Calls `callback` with an empty list.
  void GetVpnProviders(GetVpnProvidersCallback callback) override;
  void GetNetworkCertificates(
      GetNetworkCertificatesCallback callback) override {}
  void GetAlwaysOnVpn(GetAlwaysOnVpnCallback callback) override {}
  void SetAlwaysOnVpn(chromeos::network_config::mojom::AlwaysOnVpnPropertiesPtr
                          properties) override {}
  void GetSupportedVpnTypes(GetSupportedVpnTypesCallback callback) override {}
  void RequestTrafficCounters(
      const std::string& guid,
      RequestTrafficCountersCallback callback) override {}
  void ResetTrafficCounters(const std::string& guid) override {}
  void SetTrafficCountersAutoReset(
      const std::string& guid,
      bool auto_reset,
      chromeos::network_config::mojom::UInt32ValuePtr day,
      SetTrafficCountersAutoResetCallback callback) override {}
  void CreateCustomApn(
      const std::string& network_guid,
      chromeos::network_config::mojom::ApnPropertiesPtr apn) override {}
  void RemoveCustomApn(const std::string& network_guid,
                       const std::string& apn_id) override {}
  void ModifyCustomApn(
      const std::string& network_guid,
      chromeos::network_config::mojom::ApnPropertiesPtr apn) override {}

  // Adds `device_properties` to `device_properties_` or replaces the existing
  // properties for the same network type and calls `OnDeviceStateListChanged`
  // and for all observers in `observers_`.
  void SetDeviceProperties(
      chromeos::network_config::mojom::DeviceStatePropertiesPtr
          device_properties);

  // Sets the a value for the `allow_only_policy_cellular_networks` field of
  // `global_policy_`and calls `OnPoliciesApplied` with the value of
  // `global_policy_` for all observers in `observers_`.
  void SetGlobalPolicy(bool allow_only_policy_cellular_networks);

  // Sets the connection state for the network in `visible_networks_` with the
  // specified guid and calls `OnActiveNetworksChanged` for all observers in
  // `observers_`.
  void SetNetworkState(const std::string& guid,
                       chromeos::network_config::mojom::ConnectionStateType
                           connection_state_type);

  // Adds `network` to `visible_networks_` and an enabled device for the network
  // type to `device_properties_` and then calls `OnDeviceStateListChanged` and
  // `OnActiveNetworksChanged` for all observers in `observers_`.
  void AddNetworkAndDevice(
      chromeos::network_config::mojom::NetworkStatePropertiesPtr network);

  // Sets managed properties for a specific guid to
  // `guid_to_managed_properties_`. These properties can be later retrieved by
  // calling `GetManagedProperties`.
  void AddManagedProperties(
      const std::string& guid,
      chromeos::network_config::mojom::ManagedPropertiesPtr managed_properties);

  // Clears all the networks and devices and calls `OnDeviceStateListChanged`
  // and `OnActiveNetworksChanged` for all observers in `observers_`.
  void ClearNetworksAndDevices();

  // Returns how many times `RequestNetworkScan` was requested for the specified
  // network type.
  int GetScanCount(chromeos::network_config::mojom::NetworkType type);

  mojo::PendingRemote<chromeos::network_config::mojom::CrosNetworkConfig>
  GetPendingRemote();

 private:
  // Adds `device_properties` to `device_properties_` if there are no device
  // properties for the network type `device_properties->type`, otherwise it
  // replaces the existing device properties.
  void AddOrReplaceDevice(
      chromeos::network_config::mojom::DeviceStatePropertiesPtr
          device_properties);

  // Returns a filtered list of `visible_networks_` according to `network_type`
  // and `filter_type`.
  std::vector<chromeos::network_config::mojom::NetworkStatePropertiesPtr>
  GetFilteredNetworkList(
      chromeos::network_config::mojom::NetworkType network_type,
      chromeos::network_config::mojom::FilterType filter_type);

  // Currently, FakeCrosNetworkConfig only represents visible networks, stored
  // here.
  std::vector<chromeos::network_config::mojom::NetworkStatePropertiesPtr>
      visible_networks_;
  std::vector<chromeos::network_config::mojom::DeviceStatePropertiesPtr>
      device_properties_;
  std::map<std::string, chromeos::network_config::mojom::ManagedPropertiesPtr>
      guid_to_managed_properties_;
  chromeos::network_config::mojom::GlobalPolicyPtr global_policy_;
  std::map<chromeos::network_config::mojom::NetworkType, int> scan_count_;
  mojo::RemoteSet<chromeos::network_config::mojom::CrosNetworkConfigObserver>
      observers_;
  mojo::Receiver<chromeos::network_config::mojom::CrosNetworkConfig> receiver_{
      this};
};
}  // namespace chromeos::network_config

#endif  // CHROMEOS_SERVICES_NETWORK_CONFIG_PUBLIC_CPP_CROS_NETWORK_CONFIG_H_
