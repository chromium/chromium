// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_DBUS_SHILL_FAKE_SHILL_MANAGER_CLIENT_H_
#define CHROMEOS_ASH_COMPONENTS_DBUS_SHILL_FAKE_SHILL_MANAGER_CLIENT_H_

#include <map>
#include <memory>
#include <string>
#include <tuple>

#include "base/component_export.h"
#include "base/functional/callback.h"
#include "base/time/time.h"
#include "base/values.h"
#include "chromeos/ash/components/dbus/shill/shill_manager_client.h"

namespace ash {

// A fake implementation of ShillManagerClient. This works in close coordination
// with FakeShillServiceClient. FakeShillDeviceClient, and
// FakeShillProfileClient, and is not intended to be used independently.
class COMPONENT_EXPORT(SHILL_CLIENT) FakeShillManagerClient
    : public ShillManagerClient,
      public ShillManagerClient::TestInterface {
 public:
  FakeShillManagerClient();

  FakeShillManagerClient(const FakeShillManagerClient&) = delete;
  FakeShillManagerClient& operator=(const FakeShillManagerClient&) = delete;

  ~FakeShillManagerClient() override;

  // ShillManagerClient overrides
  void AddPropertyChangedObserver(
      ShillPropertyChangedObserver* observer) override;
  void RemovePropertyChangedObserver(
      ShillPropertyChangedObserver* observer) override;
  void GetProperties(
      chromeos::DBusMethodCallback<base::Value::Dict> callback) override;
  void GetNetworksForGeolocation(
      chromeos::DBusMethodCallback<base::Value::Dict> callback) override;
  void SetProperty(const std::string& name,
                   const base::Value& value,
                   base::OnceClosure callback,
                   ErrorCallback error_callback) override;
  void RequestScan(const std::string& type,
                   base::OnceClosure callback,
                   ErrorCallback error_callback) override;
  void EnableTechnology(const std::string& type,
                        base::OnceClosure callback,
                        ErrorCallback error_callback) override;
  void DisableTechnology(const std::string& type,
                         base::OnceClosure callback,
                         ErrorCallback error_callback) override;
  void ConfigureService(const base::Value::Dict& properties,
                        chromeos::ObjectPathCallback callback,
                        ErrorCallback error_callback) override;
  void ConfigureServiceForProfile(const dbus::ObjectPath& profile_path,
                                  const base::Value::Dict& properties,
                                  chromeos::ObjectPathCallback callback,
                                  ErrorCallback error_callback) override;
  void GetService(const base::Value::Dict& properties,
                  chromeos::ObjectPathCallback callback,
                  ErrorCallback error_callback) override;
  void ScanAndConnectToBestServices(base::OnceClosure callback,
                                    ErrorCallback error_callback) override;
  void SetNetworkThrottlingStatus(const NetworkThrottlingStatus& status,
                                  base::OnceClosure callback,
                                  ErrorCallback error_callback) override;
  void AddPasspointCredentials(const dbus::ObjectPath& profile_path,
                               const base::Value::Dict& properties,
                               base::OnceClosure callback,
                               ErrorCallback error_callback) override;
  void RemovePasspointCredentials(const dbus::ObjectPath& profile_path,
                                  const base::Value::Dict& properties,
                                  base::OnceClosure callback,
                                  ErrorCallback error_callback) override;
  void SetTetheringEnabled(bool enabled,
                           StringCallback callback,
                           ErrorCallback error_callback) override;
  void EnableTethering(const shill::WiFiInterfacePriority& priority,
                       StringCallback callback,
                       ErrorCallback error_callback) override;
  void DisableTethering(StringCallback callback,
                        ErrorCallback error_callback) override;
  void OnDisableTetheringSuccess(const std::string& result);
  void CheckTetheringReadiness(StringCallback callback,
                               ErrorCallback error_callback) override;
  void SetLOHSEnabled(bool enabled,
                      base::OnceClosure callback,
                      ErrorCallback error_callback) override;

  void CreateP2PGroup(
      const CreateP2PGroupParameter& create_group_argument,
      base::OnceCallback<void(base::Value::Dict result)> callback,
      ErrorCallback error_callback) override;

  void ConnectToP2PGroup(
      const ConnectP2PGroupParameter& connect_group_argument,
      base::OnceCallback<void(base::Value::Dict result)> callback,
      ErrorCallback error_callback) override;

  void DestroyP2PGroup(
      const int shill_id,
      base::OnceCallback<void(base::Value::Dict result)> callback,
      ErrorCallback error_callback) override;

  void DisconnectFromP2PGroup(
      const int shill_id,
      base::OnceCallback<void(base::Value::Dict result)> callback,
      ErrorCallback error_callback) override;

  ShillManagerClient::TestInterface* GetTestInterface() override;

  // ShillManagerClient::TestInterface overrides.
  void AddDevice(const std::string& device_path) override;
  void RemoveDevice(const std::string& device_path) override;
  void ClearDevices() override;
  void AddTechnology(const std::string& type, bool enabled) override;
  void RemoveTechnology(const std::string& type) override;
  void SetTechnologyInitializing(const std::string& type,
                                 bool initializing) override;
  void SetTechnologyProhibited(const std::string& type,
                               bool prohibited) override;
  void SetTechnologyEnabled(const std::string& type,
                            base::OnceClosure callback,
                            bool enabled) override;
  void AddGeoNetwork(const std::string& technology,
                     const base::Value::Dict& network) override;
  void AddProfile(const std::string& profile_path) override;
  void ClearProperties() override;
  void SetManagerProperty(const std::string& key,
                          const base::Value& value) override;
  base::Value::Dict GetStubProperties() override;
  void AddManagerService(const std::string& service_path,
                         bool notify_observers) override;
  void RemoveManagerService(const std::string& service_path) override;
  void RestartTethering() override;
  void ClearManagerServices() override;
  void ServiceStateChanged(const std::string& service_path,
                           const std::string& state) override;
  void SortManagerServices(bool notify) override;
  void SetupDefaultEnvironment() override;
  base::TimeDelta GetInteractiveDelay() const override;
  void SetInteractiveDelay(base::TimeDelta delay) override;
  void SetBestServiceToConnect(const std::string& service_path) override;
  const NetworkThrottlingStatus& GetNetworkThrottlingStatus() override;
  bool GetFastTransitionStatus() override;
  void SetSimulateConfigurationResult(
      FakeShillSimulatedResult configuration_result) override;
  void SetSimulateConfigurationError(std::string_view error_name,
                                     std::string_view error_message) override;
  void SetSimulateTetheringEnableResult(
      FakeShillSimulatedResult tethering_enable_result,
      const std::string& result_string) override;
  void SetSimulateCheckTetheringReadinessResult(
      FakeShillSimulatedResult tethering_readiness_result,
      const std::string& readiness_status) override;
  void SetSimulateCreateP2PGroupResult(
      FakeShillSimulatedResult operation_result,
      const std::string& result_code) override;
  void SetSimulateDestroyP2PGroupResult(
      FakeShillSimulatedResult operation_result,
      const std::string& result_code) override;
  void SetSimulateConnectToP2PGroupResult(
      FakeShillSimulatedResult operation_result,
      const std::string& result_code) override;
  void SetSimulateDisconnectFromP2PGroupResult(
      FakeShillSimulatedResult operation_result,
      const std::string& result_code) override;
  base::Value::List GetEnabledServiceList() const override;
  void ClearProfiles() override;
  void SetShouldReturnNullProperties(bool value) override;
  void SetWifiServicesVisibleByDefault(
      bool wifi_services_visible_by_default) override;
  int GetRecentlyDestroyedP2PGroupId() override;
  int GetRecentlyDisconnectedP2PGroupId() override;

  // Constants used for testing.
  static const char kFakeEthernetNetworkGuid[];

 private:
  // Error message for configure service failure.
  struct ConfigurationError {
    std::string name;
    std::string message;
  };

  using ConnectToBestServicesCallbacks =
      std::tuple<base::OnceClosure, ErrorCallback>;

  void SetDefaultProperties();
  void PassNullopt(
      chromeos::DBusMethodCallback<base::Value::Dict> callback) const;
  void PassStubProperties(
      chromeos::DBusMethodCallback<base::Value::Dict> callback) const;
  void PassStubGeoNetworks(
      chromeos::DBusMethodCallback<base::Value::Dict> callback) const;
  void CallNotifyObserversPropertyChanged(const std::string& property);
  void NotifyObserversPropertyChanged(const std::string& property);
  base::Value::List& GetListProperty(const std::string& property);
  bool TechnologyEnabled(const std::string& type) const;
  void ScanCompleted(const std::string& device_path);

  // Parses the command line for Shill stub switches and sets initial states.
  // Uses comma-separated name-value pairs (see SplitStringIntoKeyValuePairs):
  // interactive={delay} - sets delay in seconds for interactive UI
  // {wifi,cellular,etc}={on,off,disabled,none} - sets initial state for type
  void ParseCommandLineSwitch();
  bool ParseOption(const std::string& arg0, const std::string& arg1);
  bool SetInitialNetworkState(std::string type_arg,
                              const std::string& state_arg);
  std::string GetInitialStateForType(const std::string& type, bool* enabled);

  void ContinueConnectToBestServices(
      ConnectToBestServicesCallbacks connect_to_best_services_callbacks);

  // Dictionary of property name -> property value
  base::Value::Dict stub_properties_;

  // Dictionary of technology -> list of property dictionaries
  base::Value::Dict stub_geo_networks_;

  // Delay for interactive actions
  base::TimeDelta interactive_delay_;

  // Initial state for fake services.
  std::map<std::string, std::string> shill_initial_state_map_;

  // URL used for cellular activation.
  std::string cellular_olp_;

  // Technology type for fake cellular service.
  std::string cellular_technology_;

  // Roaming state for fake cellular service.
  std::string roaming_state_;

  // Current network throttling status.
  NetworkThrottlingStatus network_throttling_status_ = {false, 0, 0};

  using ShillPropertyMap = std::map<std::string, base::Value>;
  using DevicePropertyMap = std::map<std::string, ShillPropertyMap>;
  DevicePropertyMap shill_device_property_map_;

  base::ObserverList<ShillPropertyChangedObserver>::Unchecked observer_list_;

  // Track the default service for signaling Manager.DefaultService.
  std::string default_service_;

  // 'Best' service to connect to on ConnectToBestServices() calls.
  std::string best_service_;

  FakeShillSimulatedResult simulate_configuration_result_ =
      FakeShillSimulatedResult::kSuccess;
  ConfigurationError simulate_configuration_error_ = {"Error",
                                                      "Simulated failure"};
  FakeShillSimulatedResult simulate_tethering_enable_result_ =
      FakeShillSimulatedResult::kSuccess;
  std::string simulate_enable_tethering_result_string_;
  FakeShillSimulatedResult simulate_check_tethering_readiness_result_ =
      FakeShillSimulatedResult::kSuccess;
  std::string simulate_tethering_readiness_status_;
  FakeShillSimulatedResult simulate_create_p2p_group_result_ =
      FakeShillSimulatedResult::kSuccess;
  std::string simulate_create_p2p_group_result_code_;
  FakeShillSimulatedResult simulate_destroy_p2p_group_result_ =
      FakeShillSimulatedResult::kSuccess;
  std::string simulate_destroy_p2p_group_result_code_;
  int recent_destroyed_group_id = -1;
  FakeShillSimulatedResult simulate_connect_p2p_group_result_ =
      FakeShillSimulatedResult::kSuccess;
  std::string simulate_connect_p2p_group_result_code_;
  int recent_disconnected_group_id = -1;
  FakeShillSimulatedResult simulate_disconnect_p2p_group_result_ =
      FakeShillSimulatedResult::kSuccess;
  std::string simulate_disconnect_p2p_group_result_code_;

  bool return_null_properties_ = false;
  bool wifi_services_visible_by_default_ = true;

  // For testing multiple wifi networks.
  int extra_wifi_networks_ = 0;

  // For testing dynamic WEP networks (uses wifi2).
  bool dynamic_wep_ = false;

  // Caches the last-passed callbacks for ScanAndConnectToBestServices.
  std::optional<ConnectToBestServicesCallbacks>
      connect_to_best_services_callbacks_;

  // Note: This should remain the last member so it'll be destroyed and
  // invalidate its weak pointers before any other members are destroyed.
  base::WeakPtrFactory<FakeShillManagerClient> weak_ptr_factory_{this};
};

}  // namespace ash

#endif  // CHROMEOS_ASH_COMPONENTS_DBUS_SHILL_FAKE_SHILL_MANAGER_CLIENT_H_
