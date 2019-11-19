// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_DBUS_SHILL_SHILL_SERVICE_CLIENT_H_
#define CHROMEOS_DBUS_SHILL_SHILL_SERVICE_CLIENT_H_

#include <string>
#include <vector>

#include "base/callback.h"
#include "base/component_export.h"
#include "base/macros.h"
#include "chromeos/dbus/shill/shill_client_helper.h"

namespace base {
class Value;
class DictionaryValue;
}  // namespace base

namespace dbus {
class Bus;
class ObjectPath;
}  // namespace dbus

namespace chromeos {

// ShillServiceClient is used to communicate with the Shill Service
// service.
// All methods should be called from the origin thread which initializes the
// DBusThreadManager instance.
class COMPONENT_EXPORT(SHILL_CLIENT) ShillServiceClient {
 public:
  typedef ShillClientHelper::PropertyChangedHandler PropertyChangedHandler;
  typedef ShillClientHelper::DictionaryValueCallback DictionaryValueCallback;
  typedef ShillClientHelper::ListValueCallback ListValueCallback;
  typedef ShillClientHelper::ErrorCallback ErrorCallback;

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
    // or Profile. Returns the properties for the service.
    virtual base::DictionaryValue* SetServiceProperties(
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

    // Returns properties for |service_path| or null if no Service matches.
    virtual const base::DictionaryValue* GetServiceProperties(
        const std::string& service_path) const = 0;

    // If the service referenced by |service_path| is not visible (according to
    // its |shill::kVisibleProperty|, it is removed completely. If the service
    // referenced by |service_path| is visible, keeps only its "intrinsic"
    // properties and removes all other properties. Intrinsic properties are
    // properties that describe the identity or the state of  the service and
    // are not configurable, such as SSID (for wifi), signal strength (for wifi)
    // or provider (for VPN). All other properties are removed.
    virtual bool ClearConfiguredServiceProperties(
        const std::string& service_path) = 0;

    // Returns the service path for the service which has the GUID property set
    // to |guid|. If no such service exists, returns the empty string.
    virtual std::string FindServiceMatchingGUID(const std::string& guid) = 0;

    // Returns the service path for a service which is similar to the service
    // described by |template_service_properties|. For Wifi, this means that
    // security and mode match. Returns the empty string if no similar service
    // is found.
    virtual std::string FindSimilarService(
        const base::Value& template_service_properties) = 0;

    // Clears all Services from the Manager and Service stubs.
    virtual void ClearServices() = 0;

    virtual void SetConnectBehavior(const std::string& service_path,
                                    const base::Closure& behavior) = 0;

    // If |hold_back| is set to true, stops sending service property updates to
    // observers and records them instead. Then if this is called again with
    // |hold_back| == false, sends all recorded property updates.
    virtual void SetHoldBackServicePropertyUpdates(bool hold_back) = 0;

   protected:
    virtual ~TestInterface() {}
  };

  // Creates and initializes the global instance. |bus| must not be null.
  static void Initialize(dbus::Bus* bus);

  // Creates the global instance with a fake implementation.
  static void InitializeFake();

  // Destroys the global instance which must have been initialized.
  static void Shutdown();

  // Returns the global instance if initialized. May return null.
  static ShillServiceClient* Get();

  // Adds a property changed |observer| to the service at |service_path|.
  virtual void AddPropertyChangedObserver(
      const dbus::ObjectPath& service_path,
      ShillPropertyChangedObserver* observer) = 0;

  // Removes a property changed |observer| to the service at |service_path|.
  virtual void RemovePropertyChangedObserver(
      const dbus::ObjectPath& service_path,
      ShillPropertyChangedObserver* observer) = 0;

  // Calls GetProperties method.
  // |callback| is called after the method call succeeds.
  virtual void GetProperties(const dbus::ObjectPath& service_path,
                             const DictionaryValueCallback& callback) = 0;

  // Calls SetProperty method.
  // |callback| is called after the method call succeeds.
  virtual void SetProperty(const dbus::ObjectPath& service_path,
                           const std::string& name,
                           const base::Value& value,
                           const base::Closure& callback,
                           const ErrorCallback& error_callback) = 0;

  // Calls SetProperties method.
  // |callback| is called after the method call succeeds.
  virtual void SetProperties(const dbus::ObjectPath& service_path,
                             const base::DictionaryValue& properties,
                             const base::Closure& callback,
                             const ErrorCallback& error_callback) = 0;

  // Calls ClearProperty method.
  // |callback| is called after the method call succeeds.
  virtual void ClearProperty(const dbus::ObjectPath& service_path,
                             const std::string& name,
                             const base::Closure& callback,
                             const ErrorCallback& error_callback) = 0;

  // Calls ClearProperties method.
  // |callback| is called after the method call succeeds.
  virtual void ClearProperties(const dbus::ObjectPath& service_path,
                               const std::vector<std::string>& names,
                               const ListValueCallback& callback,
                               const ErrorCallback& error_callback) = 0;

  // Calls Connect method.
  // |callback| is called after the method call succeeds.
  virtual void Connect(const dbus::ObjectPath& service_path,
                       const base::Closure& callback,
                       const ErrorCallback& error_callback) = 0;

  // Calls Disconnect method.
  // |callback| is called after the method call succeeds.
  virtual void Disconnect(const dbus::ObjectPath& service_path,
                          const base::Closure& callback,
                          const ErrorCallback& error_callback) = 0;

  // Calls Remove method.
  // |callback| is called after the method call succeeds.
  virtual void Remove(const dbus::ObjectPath& service_path,
                      const base::Closure& callback,
                      const ErrorCallback& error_callback) = 0;

  // Calls ActivateCellularModem method.
  // |callback| is called after the method call succeeds.
  virtual void ActivateCellularModem(const dbus::ObjectPath& service_path,
                                     const std::string& carrier,
                                     const base::Closure& callback,
                                     const ErrorCallback& error_callback) = 0;

  // Calls the CompleteCellularActivation method.
  // |callback| is called after the method call succeeds.
  virtual void CompleteCellularActivation(
      const dbus::ObjectPath& service_path,
      const base::Closure& callback,
      const ErrorCallback& error_callback) = 0;

  // Calls the GetLoadableProfileEntries method.
  // |callback| is called after the method call succeeds.
  virtual void GetLoadableProfileEntries(
      const dbus::ObjectPath& service_path,
      const DictionaryValueCallback& callback) = 0;

  // Returns an interface for testing (stub only), or returns null.
  virtual TestInterface* GetTestInterface() = 0;

 protected:
  friend class ShillServiceClientTest;

  // Initialize/Shutdown should be used instead.
  ShillServiceClient();
  virtual ~ShillServiceClient();

 private:
  DISALLOW_COPY_AND_ASSIGN(ShillServiceClient);
};

}  // namespace chromeos

#endif  // CHROMEOS_DBUS_SHILL_SHILL_SERVICE_CLIENT_H_
