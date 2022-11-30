// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_DBUS_SERVICES_CROS_DBUS_SERVICE_H_
#define CHROMEOS_ASH_COMPONENTS_DBUS_SERVICES_CROS_DBUS_SERVICE_H_

#include <memory>
#include <string>
#include <vector>

#include "base/memory/ref_counted.h"
#include "base/threading/platform_thread.h"

namespace dbus {
class Bus;
class ExportedObject;
class ObjectPath;
}  // namespace dbus

namespace ash {

// CrosDBusService is used to run a D-Bus service inside Chrome for Chrome OS.
// It exports D-Bus methods through service provider classes that implement
// CrosDBusService::ServiceProviderInterface.
class CrosDBusService {
 public:
  // CrosDBusService consists of service providers that implement this
  // interface.
  class ServiceProviderInterface {
   public:
    // Starts the service provider. |exported_object| is used to export
    // D-Bus methods.
    virtual void Start(
        scoped_refptr<dbus::ExportedObject> exported_object) = 0;

    virtual ~ServiceProviderInterface();
  };

  using ServiceProviderList =
      std::vector<std::unique_ptr<ServiceProviderInterface>>;

  // Creates, starts, and returns a new instance owning |service_name| and
  // exporting |service_providers|'s methods on |object_path|. If not null,
  // |system_bus| is used for the service, otherwise a fake/stub implementation
  // is used.
  static std::unique_ptr<CrosDBusService> Create(
      dbus::Bus* system_bus,
      const std::string& service_name,
      const dbus::ObjectPath& object_path,
      ServiceProviderList service_providers);

  // Creates a ServiceProviderList containing a single provider.
  static ServiceProviderList CreateServiceProviderList(
      std::unique_ptr<ServiceProviderInterface> provider);

  CrosDBusService(const CrosDBusService&) = delete;
  CrosDBusService& operator=(const CrosDBusService&) = delete;

  virtual ~CrosDBusService();

 protected:
  CrosDBusService();

 private:
  friend class CrosDBusServiceTest;

  // Creates, starts, and returns a real implementation of CrosDBusService that
  // uses |bus|. Called by Create(), but can also be called directly by tests
  // that need a non-stub implementation even when not running on a device.
  static std::unique_ptr<CrosDBusService> CreateRealImpl(
      dbus::Bus* bus,
      const std::string& service_name,
      const dbus::ObjectPath& object_path,
      ServiceProviderList service_providers);
};

}  // namespace ash

#endif  // CHROMEOS_ASH_COMPONENTS_DBUS_SERVICES_CROS_DBUS_SERVICE_H_
