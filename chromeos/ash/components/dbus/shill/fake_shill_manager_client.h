// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_DBUS_SHILL_FAKE_SHILL_MANAGER_CLIENT_H_
#define CHROMEOS_ASH_COMPONENTS_DBUS_SHILL_FAKE_SHILL_MANAGER_CLIENT_H_

#include <map>
#include <memory>
#include <string>

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
      chromeos::DBusMethodCallback<base::Value> callback) override;
  void GetNetworksForGeolocation(
      chromeos::DBusMethodCallback<base::Value> callback) override;
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
  void ConfigureService(const base::Value& properties,
                        chromeos::ObjectPathCallback callback,
                        ErrorCallback error_callback) override;
  void ConfigureServiceForProfile(const dbus::ObjectPath& profile_path,
                                  const base::Value& properties,
                                  chromeos::ObjectPathCallback callback,
                                  ErrorCallback error_callback) override;
  void GetService(const base::Value& properties,
                  chromeos::ObjectPathCallback callback,
                  ErrorCallback error_callback) override;
  void ScanAndConnectToBestServices(base::OnceClosure callback,
                                    ErrorCallback error_callback) override;
  void SetNetworkThrottlingStatus(const NetworkThrottlingStatus& status,
                                  base::OnceClosure callback,
                                  ErrorCallback error_callback) override;
  void AddPasspointCredentials(const dbus::ObjectPath& profile_path,
                               const base::Value& properties,
                               base::OnceClosure callback,
                               ErrorCallback error_callback) override;
  void RemovePasspointCredentials(const dbus::ObjectPath& profile_path,
                                  const base::Value& properties,
                                  base::OnceClosure callback,
                                  ErrorCallback error_callback) override;
  void SetTetheringEnabled(bool enabled,
                           StringCallback callback,
                           ErrorCallback error_callback) override;
  void CheckTetheringReadiness(StringCallback callback,
                               ErrorCallback error_callback) override;
  void SetLOHSEnabled(bool enabled,
                      base::OnceClosure callback,
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
                     const base::Value& network) override;
  void AddProfile(const std::string& profile_path) override;
  void ClearProperties() override;
  void SetManagerProperty(const std::string& key,
                          const base::Value& value) override;
  void AddManagerService(const std::string& service_path,
                         bool notify_observers) override;
  void RemoveManagerService(const std::string& service_path) override;
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
  void SetSimulateTetheringEnableResult(
      FakeShillSimulatedResult tethering_enable_result,
      const std::string& result_string) override;
  void SetSimulateCheckTetheringReadinessResult(
      FakeShillSimulatedResult tethering_readiness_result,
      const std::string& readiness_status) override;
  base::Value GetEnabledServiceList() const override;
  void ClearProfiles() override;
  void SetShouldReturnNullProperties(bool value) override;

  // Constants used for testing.
  static const char kFakeEthernetNetworkGuid[];

 private:
  void SetDefaultProperties();
  void PassNullopt(chromeos::DBusMethodCallback<base::Value> callback) const;
  void PassStubProperties(
      chromeos::DBusMethodCallback<base::Value> callback) const;
  void PassStubGeoNetworks(
      chromeos::DBusMethodCallback<base::Value> callback) const;
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

  typedef std::map<std::string, base::Value> ShillPropertyMap;
  typedef std::map<std::string, ShillPropertyMap> DevicePropertyMap;
  DevicePropertyMap shill_device_property_map_;

  base::ObserverList<ShillPropertyChangedObserver>::Unchecked observer_list_;

  // Track the default service for signaling Manager.DefaultService.
  std::string default_service_;

  // 'Best' service to connect to on ConnectToBestServices() calls.
  std::string best_service_;

  FakeShillSimulatedResult simulate_configuration_result_ =
      FakeShillSimulatedResult::kSuccess;
  FakeShillSimulatedResult simulate_tethering_enable_result_ =
      FakeShillSimulatedResult::kSuccess;
  std::string simulate_enable_tethering_result_string_;
  FakeShillSimulatedResult simulate_check_tethering_readiness_result_ =
      FakeShillSimulatedResult::kSuccess;
  std::string simulate_tethering_readiness_status_;

  bool return_null_properties_;

  // For testing multiple wifi networks.
  int extra_wifi_networks_ = 0;

  // For testing dynamic WEP networks (uses wifi2).
  bool dynamic_wep_ = false;

  // For testing proxy-auth case for shill service state.
  bool proxy_auth_ = false;

  // Note: This should remain the last member so it'll be destroyed and
  // invalidate its weak pointers before any other members are destroyed.
  base::WeakPtrFactory<FakeShillManagerClient> weak_ptr_factory_{this};
};

}  // namespace ash

#endif  // CHROMEOS_ASH_COMPONENTS_DBUS_SHILL_FAKE_SHILL_MANAGER_CLIENT_H_
