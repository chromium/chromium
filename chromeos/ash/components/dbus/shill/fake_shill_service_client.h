// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_DBUS_SHILL_FAKE_SHILL_SERVICE_CLIENT_H_
#define CHROMEOS_ASH_COMPONENTS_DBUS_SHILL_FAKE_SHILL_SERVICE_CLIENT_H_

#include <map>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "base/component_export.h"
#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "base/values.h"
#include "chromeos/ash/components/dbus/shill/shill_service_client.h"

namespace ash {

// A fake implementation of ShillServiceClient. This works in close coordination
// with FakeShillManagerClient and is not intended to be used independently.
class COMPONENT_EXPORT(SHILL_CLIENT) FakeShillServiceClient
    : public ShillServiceClient,
      public ShillServiceClient::TestInterface {
 public:
  FakeShillServiceClient();

  FakeShillServiceClient(const FakeShillServiceClient&) = delete;
  FakeShillServiceClient& operator=(const FakeShillServiceClient&) = delete;

  ~FakeShillServiceClient() override;

  // ShillServiceClient overrides
  void AddPropertyChangedObserver(
      const dbus::ObjectPath& service_path,
      ShillPropertyChangedObserver* observer) override;
  void RemovePropertyChangedObserver(
      const dbus::ObjectPath& service_path,
      ShillPropertyChangedObserver* observer) override;
  void GetProperties(
      const dbus::ObjectPath& service_path,
      chromeos::DBusMethodCallback<base::Value::Dict> callback) override;
  void SetProperty(const dbus::ObjectPath& service_path,
                   const std::string& name,
                   const base::Value& value,
                   base::OnceClosure callback,
                   ErrorCallback error_callback) override;
  void SetProperties(const dbus::ObjectPath& service_path,
                     const base::Value::Dict& properties,
                     base::OnceClosure callback,
                     ErrorCallback error_callback) override;
  void ClearProperty(const dbus::ObjectPath& service_path,
                     const std::string& name,
                     base::OnceClosure callback,
                     ErrorCallback error_callback) override;
  void ClearProperties(const dbus::ObjectPath& service_path,
                       const std::vector<std::string>& names,
                       ListValueCallback callback,
                       ErrorCallback error_callback) override;
  void Connect(const dbus::ObjectPath& service_path,
               base::OnceClosure callback,
               ErrorCallback error_callback) override;
  void Disconnect(const dbus::ObjectPath& service_path,
                  base::OnceClosure callback,
                  ErrorCallback error_callback) override;
  void Remove(const dbus::ObjectPath& service_path,
              base::OnceClosure callback,
              ErrorCallback error_callback) override;
  void CompleteCellularActivation(const dbus::ObjectPath& service_path,
                                  base::OnceClosure callback,
                                  ErrorCallback error_callback) override;
  void GetLoadableProfileEntries(
      const dbus::ObjectPath& service_path,
      chromeos::DBusMethodCallback<base::Value::Dict> callback) override;
  void GetWiFiPassphrase(const dbus::ObjectPath& service_path,
                         StringCallback callback,
                         ErrorCallback error_callback) override;
  void GetEapPassphrase(const dbus::ObjectPath& service_path,
                        StringCallback callback,
                        ErrorCallback error_callback) override;
  void RequestPortalDetection(
      const dbus::ObjectPath& service_path,
      chromeos::VoidDBusMethodCallback callback) override;
  void RequestTrafficCounters(
      const dbus::ObjectPath& service_path,
      chromeos::DBusMethodCallback<base::Value> callback) override;
  void ResetTrafficCounters(const dbus::ObjectPath& service_path,
                            base::OnceClosure callback,
                            ErrorCallback error_callback) override;
  ShillServiceClient::TestInterface* GetTestInterface() override;

  // ShillServiceClient::TestInterface overrides.
  base::Value::Dict GetFakeDefaultModbApnDict() override;
  void AddService(const std::string& service_path,
                  const std::string& guid,
                  const std::string& name,
                  const std::string& type,
                  const std::string& state,
                  bool visible) override;
  void AddServiceWithIPConfig(const std::string& service_path,
                              const std::string& guid,
                              const std::string& name,
                              const std::string& type,
                              const std::string& state,
                              const std::string& ipconfig_path,
                              bool visible) override;
  base::Value::Dict* SetServiceProperties(const std::string& service_path,
                                          const std::string& guid,
                                          const std::string& name,
                                          const std::string& type,
                                          const std::string& state,
                                          bool visible) override;
  void RemoveService(const std::string& service_path) override;
  bool SetServiceProperty(const std::string& service_path,
                          const std::string& property,
                          const base::Value& value) override;
  const base::Value::Dict* GetServiceProperties(
      const std::string& service_path) const override;
  bool ClearConfiguredServiceProperties(
      const std::string& service_path) override;
  std::string FindServiceMatchingGUID(const std::string& guid) override;
  std::string FindServiceMatchingName(const std::string& name) override;
  std::string FindSimilarService(
      const base::Value::Dict& template_service_properties) override;
  void ClearServices() override;
  void SetConnectBehavior(const std::string& service_path,
                          const base::RepeatingClosure& behavior) override;
  void SetErrorForNextConnectionAttempt(const std::string& error_name) override;
  void SetErrorForNextSetPropertiesAttempt(
      const std::string& error_name) override;
  void SetRequestPortalState(const std::string& state) override;
  void SetHoldBackServicePropertyUpdates(bool hold_back) override;
  void SetRequireServiceToGetProperties(
      bool require_service_to_get_properties) override;
  void SetFakeTrafficCounters(base::Value::List fake_traffic_counters) override;
  void SetTimeGetterForTest(base::RepeatingCallback<base::Time()>) override;

 private:
  typedef base::ObserverList<ShillPropertyChangedObserver>::Unchecked
      PropertyObserverList;

  void NotifyObserversPropertyChanged(const dbus::ObjectPath& service_path,
                                      const std::string& property);
  base::Value::Dict* GetModifiableServiceProperties(
      const std::string& service_path,
      bool create_if_missing);
  PropertyObserverList& GetObserverList(const dbus::ObjectPath& device_path);
  void SetOtherServicesOffline(const std::string& service_path);
  void SetCellularActivated(const dbus::ObjectPath& service_path,
                            ErrorCallback error_callback);
  void ContinueConnect(const std::string& service_path);
  void SetDefaultFakeTrafficCounters();

  base::Value::Dict stub_services_;

  // Per network service, stores a closure that is executed on each connection
  // attempt. The callback can for example modify the services properties in
  // order to simulate a connection failure.
  std::map<std::string, base::RepeatingClosure> connect_behavior_;

  // If set the next Connect call will fail with this error_name.
  std::optional<std::string> connect_error_name_;

  // If set the next SetProperties call will fail with this error_name.
  std::optional<std::string> set_properties_error_name_;

  // Optional state to set after a call to RequestPortalDetection.
  std::optional<std::string> request_portal_state_;

  // Observer list for each service.
  std::map<dbus::ObjectPath, std::unique_ptr<PropertyObserverList>>
      observer_list_;

  // If this is true, the FakeShillServiceClient is recording service property
  // updates and will only send them when SetHoldBackServicePropertyUpdates is
  // called with false again.
  bool hold_back_service_property_updates_ = false;

  // Property updates that were held back while
  // |hold_back_service_property_updates_| was true.
  std::vector<base::OnceClosure> recorded_property_updates_;

  // Whether or not this class should fail if GetProperties() is called for an
  // unknown service.
  bool require_service_to_get_properties_ = false;

  base::Value::List fake_traffic_counters_;

  // Gets the mocked time in tests.
  base::RepeatingCallback<base::Time()> time_getter_;

  // Note: This should remain the last member so it'll be destroyed and
  // invalidate its weak pointers before any other members are destroyed.
  base::WeakPtrFactory<FakeShillServiceClient> weak_ptr_factory_{this};
};

}  // namespace ash

#endif  // CHROMEOS_ASH_COMPONENTS_DBUS_SHILL_FAKE_SHILL_SERVICE_CLIENT_H_
