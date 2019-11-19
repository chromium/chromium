// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_SERVICES_NETWORK_CONFIG_CROS_NETWORK_CONFIG_H_
#define CHROMEOS_SERVICES_NETWORK_CONFIG_CROS_NETWORK_CONFIG_H_

#include "base/containers/flat_map.h"
#include "base/memory/weak_ptr.h"
#include "chromeos/network/network_certificate_handler.h"
#include "chromeos/network/network_state_handler_observer.h"
#include "chromeos/services/network_config/public/mojom/cros_network_config.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "mojo/public/cpp/bindings/remote_set.h"

namespace base {
class DictionaryValue;
}

namespace chromeos {

class ManagedNetworkConfigurationHandler;
class NetworkConnectionHandler;
class NetworkDeviceHandler;
class NetworkStateHandler;

namespace network_config {

class CrosNetworkConfig : public mojom::CrosNetworkConfig,
                          public NetworkStateHandlerObserver,
                          public NetworkCertificateHandler::Observer {
 public:
  // Constructs an instance of CrosNetworkConfig with default network subsystem
  // dependencies appropriate for a production environment.
  CrosNetworkConfig();

  // Constructs an instance of CrosNetworkConfig with specific network subsystem
  // dependencies.
  CrosNetworkConfig(
      NetworkStateHandler* network_state_handler,
      NetworkDeviceHandler* network_device_handler,
      ManagedNetworkConfigurationHandler* network_configuration_handler,
      NetworkConnectionHandler* network_connection_handler,
      NetworkCertificateHandler* network_certificate_handler);
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

 private:
  void GetManagedPropertiesSuccess(int callback_id,
                                   const std::string& service_path,
                                   const base::DictionaryValue& properties);
  void GetManagedPropertiesSuccessEap(
      int callback_id,
      const std::string& service_path,
      const base::DictionaryValue& eap_properties);
  void GetManagedPropertiesSuccessNoEap(
      int callback_id,
      const std::string& error_name,
      std::unique_ptr<base::DictionaryValue> error_data);
  void GetManagedPropertiesFailure(
      std::string guid,
      int callback_id,
      const std::string& error_name,
      std::unique_ptr<base::DictionaryValue> error_data);
  void SetPropertiesSuccess(int callback_id);
  void SetPropertiesConfigureSuccess(int callback_id,
                                     const std::string& service_path,
                                     const std::string& guid);
  void SetPropertiesFailure(const std::string& guid,
                            int callback_id,
                            const std::string& error_name,
                            std::unique_ptr<base::DictionaryValue> error_data);
  void ConfigureNetworkSuccess(int callback_id,
                               const std::string& service_path,
                               const std::string& guid);
  void ConfigureNetworkFailure(
      int callback_id,
      const std::string& error_name,
      std::unique_ptr<base::DictionaryValue> error_data);
  void ForgetNetworkSuccess(int callback_id);
  void ForgetNetworkFailure(const std::string& guid,
                            int callback_id,
                            const std::string& error_name,
                            std::unique_ptr<base::DictionaryValue> error_data);
  void SetCellularSimStateSuccess(int callback_id);
  void SetCellularSimStateFailure(
      int callback_id,
      const std::string& error_name,
      std::unique_ptr<base::DictionaryValue> error_data);
  void SelectCellularMobileNetworkSuccess(int callback_id);
  void SelectCellularMobileNetworkFailure(
      int callback_id,
      const std::string& error_name,
      std::unique_ptr<base::DictionaryValue> error_data);

  void StartConnectSuccess(int callback_id);
  void StartConnectFailure(int callback_id,
                           const std::string& error_name,
                           std::unique_ptr<base::DictionaryValue> error_data);
  void StartDisconnectSuccess(int callback_id);
  void StartDisconnectFailure(
      int callback_id,
      const std::string& error_name,
      std::unique_ptr<base::DictionaryValue> error_data);

  // NetworkStateHandlerObserver
  void NetworkListChanged() override;
  void DeviceListChanged() override;
  void ActiveNetworksChanged(
      const std::vector<const NetworkState*>& active_networks) override;
  void NetworkPropertiesUpdated(const NetworkState* network) override;
  void DevicePropertiesUpdated(const DeviceState* device) override;
  void OnShuttingDown() override;

  // NetworkCertificateHandler::Observer
  void OnCertificatesChanged() override;

  const std::string& GetServicePathFromGuid(const std::string& guid);

  NetworkStateHandler* network_state_handler_;    // Unowned
  NetworkDeviceHandler* network_device_handler_;  // Unowned
  ManagedNetworkConfigurationHandler*
      network_configuration_handler_;                       // Unowned
  NetworkConnectionHandler* network_connection_handler_;    // Unowned
  NetworkCertificateHandler* network_certificate_handler_;  // Unowned

  mojo::RemoteSet<mojom::CrosNetworkConfigObserver> observers_;
  mojo::ReceiverSet<mojom::CrosNetworkConfig> receivers_;

  int callback_id_ = 1;
  base::flat_map<int, GetManagedPropertiesCallback>
      get_managed_properties_callbacks_;
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

  // GetManagedProperties may require multiple async calls so we need to store
  // an owned copy of the mojo properties by callback id.
  base::flat_map<int, mojom::ManagedPropertiesPtr> managed_properties_;

  base::WeakPtrFactory<CrosNetworkConfig> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(CrosNetworkConfig);
};

}  // namespace network_config
}  // namespace chromeos

#endif  // CHROMEOS_SERVICES_NETWORK_CONFIG_CROS_NETWORK_CONFIG_H_
