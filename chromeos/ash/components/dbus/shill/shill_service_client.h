// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_DBUS_SHILL_SHILL_SERVICE_CLIENT_H_
#define CHROMEOS_ASH_COMPONENTS_DBUS_SHILL_SHILL_SERVICE_CLIENT_H_

#include <string>
#include <vector>

#include "base/component_export.h"
#include "base/functional/callback.h"
#include "base/values.h"
#include "chromeos/ash/components/dbus/shill/shill_client_helper.h"

namespace base {
class Time;
}

namespace dbus {
class Bus;
class ObjectPath;
}  // namespace dbus

namespace ash {

// ShillServiceClient is used to communicate with the Shill Service
// service.
// All methods should be called from the origin thread which initializes the
// DBusThreadManager instance.
class COMPONENT_EXPORT(SHILL_CLIENT) ShillServiceClient {
 public:
  typedef ShillClientHelper::ListValueCallback ListValueCallback;
  typedef ShillClientHelper::ErrorCallback ErrorCallback;
  typedef ShillClientHelper::StringCallback StringCallback;

  // Interface for setting up services for testing. Accessed through
  // GetTestInterface(), only implemented in the stub implementation.
  class TestInterface {
   public:
    // Adds a Service to the Manager and Service stubs.
    virtual void AddService(const std::string& service_path,
                            const std::string& guid,
                            const std::string& name,
                            const std::string& type,
                            const std::string& state,
                            bool visible) = 0;
    virtual void AddServiceWithIPConfig(const std::string& service_path,
                                        const std::string& guid,
                                        const std::string& name,
                                        const std::string& type,
                                        const std::string& state,
                                        const std::string& ipconfig_path,
                                        bool visible) = 0;

    // Sets the properties for a service but does not add it to the Manager
    // or Profile. Returns the properties for the service as a dictionary Value.
    virtual base::Value::Dict* SetServiceProperties(
        const std::string& service_path,
        const std::string& guid,
        const std::string& name,
        const std::string& type,
        const std::string& state,
        bool visible) = 0;

    // Removes a Service to the Manager and Service stubs.
    virtual void RemoveService(const std::string& service_path) = 0;

    // Returns false if a Service matching |service_path| does not exist.
    virtual bool SetServiceProperty(const std::string& service_path,
                                    const std::string& property,
                                    const base::Value& value) = 0;

    // Returns properties for |service_path| as a dictionary Value or null if no
    // Service matches.
    virtual const base::Value::Dict* GetServiceProperties(
        const std::string& service_path) const = 0;

    // If the service referenced by |service_path| is not visible (according to
    // its |shill::kVisibleProperty| or if it's VPN or Cellular service then,
    // it is removed completely. Otherwise keeps only its "intrinsic" properties
    // and removes all other properties. Intrinsic properties are properties
    // that describe the identity or the state of  the service and are not
    // configurable, such as SSID (for wifi), signal strength (for wifi). All
    // other properties are removed.
    virtual bool ClearConfiguredServiceProperties(
        const std::string& service_path) = 0;

    // Returns the service path for the service which has the GUID property set
    // to |guid|. If no such service exists, returns the empty string.
    virtual std::string FindServiceMatchingGUID(const std::string& guid) = 0;

    // Returns the first service path for the service which has the name
    // property set  to |name|. If no such service exists, returns the empty
    // string.
    virtual std::string FindServiceMatchingName(const std::string& name) = 0;

    // Returns the service path for a service which is similar to the service
    // described by |template_service_properties|. For Wifi, this means that
    // security and mode match. Returns the empty string if no similar service
    // is found.
    virtual std::string FindSimilarService(
        const base::Value::Dict& template_service_properties) = 0;

    // Gets the default Modb APN dict value that will be used to set on each
    // cellular service.
    virtual base::Value::Dict GetFakeDefaultModbApnDict() = 0;

    // Clears all Services from the Manager and Service stubs.
    virtual void ClearServices() = 0;

    virtual void SetConnectBehavior(const std::string& service_path,
                                    const base::RepeatingClosure& behavior) = 0;

    // Sets a Connect error. If set, the next connect call will fail with given
    // |error_name|
    virtual void SetErrorForNextConnectionAttempt(
        const std::string& error_name) = 0;

    // Sets a SetProperties error. If set, the next SetProperties call will
    // fail with the given |error_name|
    virtual void SetErrorForNextSetPropertiesAttempt(
        const std::string& error_name) = 0;

    // Sets a state property to set after a call to RequestPortalDetection.
    virtual void SetRequestPortalState(const std::string& state) = 0;

    // If |hold_back| is set to true, stops sending service property updates to
    // observers and records them instead. Then if this is called again with
    // |hold_back| == false, sends all recorded property updates.
    virtual void SetHoldBackServicePropertyUpdates(bool hold_back) = 0;

    // Sets whether the fake should fail if requested to fetch properties for a
    // service that is not known by Shill.
    virtual void SetRequireServiceToGetProperties(
        bool require_service_to_get_properties) = 0;

    // Sets a fake traffic counters that can be used in tests.
    virtual void SetFakeTrafficCounters(
        base::Value::List fake_traffic_counters) = 0;

    // Sets the callback used to get the mocked time in tests.
    virtual void SetTimeGetterForTest(
        base::RepeatingCallback<base::Time()>) = 0;

   protected:
    virtual ~TestInterface() = default;
  };

  // Creates and initializes the global instance. |bus| must not be null.
  static void Initialize(dbus::Bus* bus);

  // Creates the global instance with a fake implementation.
  static void InitializeFake();

  // Destroys the global instance which must have been initialized.
  static void Shutdown();

  // Returns the global instance if initialized. May return null.
  static ShillServiceClient* Get();

  ShillServiceClient(const ShillServiceClient&) = delete;
  ShillServiceClient& operator=(const ShillServiceClient&) = delete;

  // Adds a property changed |observer| to the service at |service_path|.
  virtual void AddPropertyChangedObserver(
      const dbus::ObjectPath& service_path,
      ShillPropertyChangedObserver* observer) = 0;

  // Removes a property changed |observer| to the service at |service_path|.
  virtual void RemovePropertyChangedObserver(
      const dbus::ObjectPath& service_path,
      ShillPropertyChangedObserver* observer) = 0;

  // Calls the GetProperties DBus method and invokes |callback| when complete.
  // |callback| receives a dictionary containing the Service properties on
  // success or nullopt on failure.
  virtual void GetProperties(
      const dbus::ObjectPath& service_path,
      chromeos::DBusMethodCallback<base::Value::Dict> callback) = 0;

  // Calls SetProperty method.
  // |callback| is called after the method call succeeds.
  virtual void SetProperty(const dbus::ObjectPath& service_path,
                           const std::string& name,
                           const base::Value& value,
                           base::OnceClosure callback,
                           ErrorCallback error_callback) = 0;

  // Calls the SetProperties DBus method with |properties|. Invokes |callback|
  // on success or |error_callback| on failure.
  virtual void SetProperties(const dbus::ObjectPath& service_path,
                             const base::Value::Dict& properties,
                             base::OnceClosure callback,
                             ErrorCallback error_callback) = 0;

  // Calls ClearProperty method.
  // |callback| is called after the method call succeeds.
  virtual void ClearProperty(const dbus::ObjectPath& service_path,
                             const std::string& name,
                             base::OnceClosure callback,
                             ErrorCallback error_callback) = 0;

  // Calls ClearProperties method.
  // |callback| is called after the method call succeeds.
  virtual void ClearProperties(const dbus::ObjectPath& service_path,
                               const std::vector<std::string>& names,
                               ListValueCallback callback,
                               ErrorCallback error_callback) = 0;

  // Calls Connect method.
  // |callback| is called after the method call succeeds.
  virtual void Connect(const dbus::ObjectPath& service_path,
                       base::OnceClosure callback,
                       ErrorCallback error_callback) = 0;

  // Calls Disconnect method.
  // |callback| is called after the method call succeeds.
  virtual void Disconnect(const dbus::ObjectPath& service_path,
                          base::OnceClosure callback,
                          ErrorCallback error_callback) = 0;

  // Calls Remove method.
  // |callback| is called after the method call succeeds.
  virtual void Remove(const dbus::ObjectPath& service_path,
                      base::OnceClosure callback,
                      ErrorCallback error_callback) = 0;

  // Calls the CompleteCellularActivation method.
  // |callback| is called after the method call succeeds.
  virtual void CompleteCellularActivation(const dbus::ObjectPath& service_path,
                                          base::OnceClosure callback,
                                          ErrorCallback error_callback) = 0;

  // Calls the GetLoadableProfileEntries method.
  // |callback| is called after the method call succeeds.
  virtual void GetLoadableProfileEntries(
      const dbus::ObjectPath& service_path,
      chromeos::DBusMethodCallback<base::Value::Dict> callback) = 0;

  // Retrieves the saved WiFi passphrase for the given network.
  virtual void GetWiFiPassphrase(const dbus::ObjectPath& service_path,
                                 StringCallback callback,
                                 ErrorCallback error_callback) = 0;

  // Retrieves the saved EAP passphrase for the given network.
  virtual void GetEapPassphrase(const dbus::ObjectPath& service_path,
                                StringCallback callback,
                                ErrorCallback error_callback) = 0;

  // Calls the RequestPortalDetection method.
  // |callback| is called after the method call completes.
  virtual void RequestPortalDetection(
      const dbus::ObjectPath& service_path,
      chromeos::VoidDBusMethodCallback callback) = 0;

  // Calls the RequestTrafficCounters method.
  // |callback| is called after the method call succeeds.
  virtual void RequestTrafficCounters(
      const dbus::ObjectPath& service_path,
      chromeos::DBusMethodCallback<base::Value> callback) = 0;

  // Calls the ResetTrafficCounters method.
  // |callback| is called after the method call succeeds.
  virtual void ResetTrafficCounters(const dbus::ObjectPath& service_path,
                                    base::OnceClosure callback,
                                    ErrorCallback error_callback) = 0;

  // Returns an interface for testing (stub only), or returns null.
  virtual TestInterface* GetTestInterface() = 0;

 protected:
  friend class ShillServiceClientTest;

  // Initialize/Shutdown should be used instead.
  ShillServiceClient();
  virtual ~ShillServiceClient();
};

}  // namespace ash

#endif  // CHROMEOS_ASH_COMPONENTS_DBUS_SHILL_SHILL_SERVICE_CLIENT_H_
