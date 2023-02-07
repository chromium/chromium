// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_DBUS_SHILL_SHILL_IPCONFIG_CLIENT_H_
#define CHROMEOS_ASH_COMPONENTS_DBUS_SHILL_SHILL_IPCONFIG_CLIENT_H_

#include <string>

#include "base/component_export.h"
#include "base/functional/callback.h"
#include "chromeos/ash/components/dbus/shill/shill_client_helper.h"

namespace base {
class Value;
}

namespace dbus {
class Bus;
class ObjectPath;
}  // namespace dbus

namespace ash {

class ShillPropertyChangedObserver;

// ShillIPConfigClient is used to communicate with the Shill IPConfig
// service.  All methods should be called from the origin thread which
// initializes the DBusThreadManager instance.
class COMPONENT_EXPORT(SHILL_CLIENT) ShillIPConfigClient {
 public:
  class TestInterface {
   public:
    // Adds an IPConfig entry.
    virtual void AddIPConfig(const std::string& ip_config_path,
                             base::Value::Dict properties) = 0;

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
  static ShillIPConfigClient* Get();

  ShillIPConfigClient(const ShillIPConfigClient&) = delete;
  ShillIPConfigClient& operator=(const ShillIPConfigClient&) = delete;

  // Adds a property changed |observer| for the ipconfig at |ipconfig_path|.
  virtual void AddPropertyChangedObserver(
      const dbus::ObjectPath& ipconfig_path,
      ShillPropertyChangedObserver* observer) = 0;

  // Removes a property changed |observer| for the ipconfig at |ipconfig_path|.
  virtual void RemovePropertyChangedObserver(
      const dbus::ObjectPath& ipconfig_path,
      ShillPropertyChangedObserver* observer) = 0;

  // Calls the GetProperties DBus method and invokes |callback| when complete.
  // |callback| receives a dictionary containing the IPCOnfig properties on
  // success or nullopt on failure.
  virtual void GetProperties(
      const dbus::ObjectPath& ipconfig_path,
      chromeos::DBusMethodCallback<base::Value::Dict> callback) = 0;

  // Calls SetProperty method.
  // |callback| is called after the method call succeeds.
  virtual void SetProperty(const dbus::ObjectPath& ipconfig_path,
                           const std::string& name,
                           const base::Value& value,
                           chromeos::VoidDBusMethodCallback callback) = 0;

  // Calls ClearProperty method.
  // |callback| is called after the method call succeeds.
  virtual void ClearProperty(const dbus::ObjectPath& ipconfig_path,
                             const std::string& name,
                             chromeos::VoidDBusMethodCallback callback) = 0;

  // Calls Remove method.
  // |callback| is called after the method call succeeds.
  virtual void Remove(const dbus::ObjectPath& ipconfig_path,
                      chromeos::VoidDBusMethodCallback callback) = 0;

  // Returns an interface for testing (stub only), or returns null.
  virtual ShillIPConfigClient::TestInterface* GetTestInterface() = 0;

 protected:
  friend class ShillIPConfigClientTest;

  // Initialize/Shutdown should be used instead.
  ShillIPConfigClient();
  virtual ~ShillIPConfigClient();
};

}  // namespace ash

#endif  // CHROMEOS_ASH_COMPONENTS_DBUS_SHILL_SHILL_IPCONFIG_CLIENT_H_
