// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_SERVICES_NETWORK_CONFIG_CROS_NETWORK_CONFIG_H_
#define CHROMEOS_SERVICES_NETWORK_CONFIG_CROS_NETWORK_CONFIG_H_

#include "base/containers/flat_map.h"
#include "base/memory/weak_ptr.h"
#include "chromeos/ash/components/network/cellular_inhibitor.h"
// TODO(https://crbug.com/1164001): move to forward declaration
#include "chromeos/ash/components/network/managed_network_configuration_handler.h"
#include "chromeos/ash/components/network/network_certificate_handler.h"
// TODO(https://crbug.com/1164001): move to forward declaration
#include "chromeos/ash/components/network/network_connection_handler.h"
// TODO(https://crbug.com/1164001): move to forward declaration
#include "chromeos/ash/components/network/network_device_handler.h"
#include "chromeos/ash/components/network/network_policy_observer.h"
#include "chromeos/ash/components/network/network_profile_handler.h"
#include "chromeos/ash/components/network/network_state_handler_observer.h"
#include "chromeos/services/network_config/public/mojom/cros_network_config.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "mojo/public/cpp/bindings/remote_set.h"

namespace ash {
class CellularESimProfileHandler;
class NetworkStateHandler;
}  // namespace ash

namespace chromeos::network_config {

class CrosNetworkConfig : public mojom::CrosNetworkConfig,
                          public NetworkStateHandlerObserver,
                          public NetworkCertificateHandler::Observer,
                          public CellularInhibitor::Observer,
                          public NetworkPolicyObserver {
 public:
  // Constructs an instance of CrosNetworkConfig with default network subsystem
  // dependencies appropriate for a production environment.
  CrosNetworkConfig();

  // Constructs an instance of CrosNetworkConfig with specific network subsystem
  // dependencies.
  CrosNetworkConfig(
      NetworkStateHandler* network_state_handler,
      NetworkDeviceHandler* network_device_handler,
      CellularInhibitor* cellular_inhibitor,
      ash::CellularESimProfileHandler* cellular_esim_profile_handler,
      ManagedNetworkConfigurationHandler* network_configuration_handler,
      NetworkConnectionHandler* network_connection_handler,
      NetworkCertificateHandler* network_certificate_handler,
      NetworkProfileHandler* network_profile_handler);

  CrosNetworkConfig(const CrosNetworkConfig&) = delete;
  CrosNetworkConfig& operator=(const CrosNetworkConfig&) = delete;

  ~CrosNetworkConfig() override;

  void BindReceiver(mojo::PendingReceiver<mojom::CrosNetworkConfig> receiver);

  // mojom::CrosNetworkConfig
  void AddObserver(
      mojo::PendingRemote<mojom::CrosNetworkConfigObserver> observer) override;
  void GetNetworkState(const std::string& guid,
                       GetNetworkStateCallback callback) override;
  void GetNetworkStateList(mojom::NetworkFilterPtr filter,
                           GetNetworkStateListCallback callback) override;
  void GetDeviceStateList(GetDeviceStateListCallback callback) override;
  void GetManagedProperties(const std::string& guid,
                            GetManagedPropertiesCallback callback) override;
  void SetProperties(const std::string& guid,
                     mojom::ConfigPropertiesPtr properties,
                     SetPropertiesCallback callback) override;
  void ConfigureNetwork(mojom::ConfigPropertiesPtr properties,
                        bool shared,
                        ConfigureNetworkCallback callback) override;
  void ForgetNetwork(const std::string& guid,
                     ForgetNetworkCallback callback) override;
  void SetNetworkTypeEnabledState(
      mojom::NetworkType type,
      bool enabled,
      SetNetworkTypeEnabledStateCallback callback) override;
  void SetCellularSimState(mojom::CellularSimStatePtr sim_state,
                           SetCellularSimStateCallback callback) override;
  void SelectCellularMobileNetwork(
      const std::string& guid,
      const std::string& network_id,
      SelectCellularMobileNetworkCallback callback) override;
  void RequestNetworkScan(mojom::NetworkType type) override;
  void GetGlobalPolicy(GetGlobalPolicyCallback callback) override;
  void StartConnect(const std::string& guid,
                    StartConnectCallback callback) override;
  void StartDisconnect(const std::string& guid,
                       StartDisconnectCallback callback) override;
  void SetVpnProviders(std::vector<mojom::VpnProviderPtr> providers) override;
  void GetVpnProviders(GetVpnProvidersCallback callback) override;
  void GetNetworkCertificates(GetNetworkCertificatesCallback callback) override;
  void GetAlwaysOnVpn(GetAlwaysOnVpnCallback callback) override;
  void SetAlwaysOnVpn(mojom::AlwaysOnVpnPropertiesPtr properties) override;
  void GetSupportedVpnTypes(GetSupportedVpnTypesCallback callback) override;
  void RequestTrafficCounters(const std::string& guid,
                              RequestTrafficCountersCallback callback) override;
  void ResetTrafficCounters(const std::string& guid) override;
  void SetTrafficCountersAutoReset(
      const std::string& guid,
      bool auto_reset,
      mojom::UInt32ValuePtr day,
      SetTrafficCountersAutoResetCallback callback) override;
  void CreateCustomApn(const std::string& network_guid,
                       mojom::ApnPropertiesPtr apn) override;
  void RemoveCustomApn(const std::string& network_guid,
                       const std::string& apn_id) override;
  void ModifyCustomApn(const std::string& network_guid,
                       mojom::ApnPropertiesPtr apn) override;

  // static
  static mojom::TrafficCounterSource GetTrafficCounterEnumForTesting(
      const std::string& source);

 private:
  void OnGetManagedProperties(GetManagedPropertiesCallback callback,
                              std::string guid,
                              const std::string& service_path,
                              absl::optional<base::Value> properties,
                              absl::optional<std::string> error);
  void OnGetManagedPropertiesEap(GetManagedPropertiesCallback callback,
                                 mojom::ManagedPropertiesPtr managed_properties,
                                 const std::string& service_path,
                                 absl::optional<base::Value> properties,
                                 absl::optional<std::string> error);
  void SetPropertiesInternal(const std::string& guid,
                             const NetworkState& network,
                             base::Value::Dict onc,
                             SetPropertiesCallback callback);
  void SetPropertiesSuccess(int callback_id);
  void SetPropertiesConfigureSuccess(int callback_id,
                                     const std::string& service_path,
                                     const std::string& guid);
  void SetPropertiesFailure(const std::string& guid,
                            int callback_id,
                            const std::string& error_name);
  void ConfigureNetworkSuccess(int callback_id,
                               const std::string& service_path,
                               const std::string& guid);
  void ConfigureNetworkFailure(int callback_id, const std::string& error_name);
  void ForgetNetworkSuccess(int callback_id);
  void ForgetNetworkFailure(const std::string& guid,
                            int callback_id,
                            const std::string& error_name);
  void SetCellularSimStateSuccess(int callback_id);
  void SetCellularSimStateFailure(int callback_id,
                                  const std::string& error_name);
  void SelectCellularMobileNetworkSuccess(int callback_id);
  void SelectCellularMobileNetworkFailure(int callback_id,
                                          const std::string& error_name);
  void UpdateCustomApnList(const NetworkState* network,
                           const mojom::ConfigProperties* properties);
  std::vector<mojom::ApnPropertiesPtr> GetCustomApnList(
      const std::string& guid);

  void StartConnectSuccess(int callback_id);
  void StartConnectFailure(int callback_id, const std::string& error_name);
  void StartDisconnectSuccess(int callback_id);
  void StartDisconnectFailure(int callback_id, const std::string& error_name);
  void OnGetAlwaysOnVpn(GetAlwaysOnVpnCallback callback,
                        std::string mode,
                        std::string service_path);
  void OnGetSupportedVpnTypes(GetSupportedVpnTypesCallback callback,
                              absl::optional<base::Value> manager_properties);
  void PopulateTrafficCounters(RequestTrafficCountersCallback callback,
                               absl::optional<base::Value> traffic_counters);

  // NetworkStateHandlerObserver:
  void NetworkListChanged() override;
  void DeviceListChanged() override;
  void ActiveNetworksChanged(
      const std::vector<const NetworkState*>& active_networks) override;
  void NetworkPropertiesUpdated(const NetworkState* network) override;
  void DevicePropertiesUpdated(const DeviceState* device) override;
  void OnShuttingDown() override;
  void ScanStarted(const DeviceState* device) override;
  void ScanCompleted(const DeviceState* device) override;
  void NetworkConnectionStateChanged(const NetworkState* network) override;

  // NetworkCertificateHandler::Observer:
  void OnCertificatesChanged() override;

  // CellularInhibitor::Observer:
  void OnInhibitStateChanged() override;

  // NetworkPolicyObserver:
  void PoliciesApplied(const std::string& userhash) override;
  void OnManagedNetworkConfigurationHandlerShuttingDown() override;

  const std::string& GetServicePathFromGuid(const std::string& guid);

  NetworkStateHandler* network_state_handler_;  // Unowned

  NetworkStateHandlerScopedObservation network_state_handler_observer_{this};

  NetworkDeviceHandler* network_device_handler_;                    // Unowned
  CellularInhibitor* cellular_inhibitor_;                           // Unowned
  ash::CellularESimProfileHandler* cellular_esim_profile_handler_;  // Unowned
  ManagedNetworkConfigurationHandler*
      network_configuration_handler_;                       // Unowned
  NetworkConnectionHandler* network_connection_handler_;    // Unowned
  NetworkCertificateHandler* network_certificate_handler_;  // Unowned
  NetworkProfileHandler* network_profile_handler_;          // Unowned

  mojo::RemoteSet<mojom::CrosNetworkConfigObserver> observers_;
  mojo::ReceiverSet<mojom::CrosNetworkConfig> receivers_;

  int callback_id_ = 1;
  base::flat_map<int, SetPropertiesCallback> set_properties_callbacks_;
  base::flat_map<int, ConfigureNetworkCallback> configure_network_callbacks_;
  base::flat_map<int, ForgetNetworkCallback> forget_network_callbacks_;
  base::flat_map<int, SetCellularSimStateCallback>
      set_cellular_sim_state_callbacks_;
  base::flat_map<int, SelectCellularMobileNetworkCallback>
      select_cellular_mobile_network_callbacks_;
  base::flat_map<int, StartConnectCallback> start_connect_callbacks_;
  base::flat_map<int, StartDisconnectCallback> start_disconnect_callbacks_;

  std::vector<mojom::VpnProviderPtr> vpn_providers_;

  base::WeakPtrFactory<CrosNetworkConfig> weak_factory_{this};
};

}  // namespace chromeos::network_config

#endif  // CHROMEOS_SERVICES_NETWORK_CONFIG_CROS_NETWORK_CONFIG_H_
