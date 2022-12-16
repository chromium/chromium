// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_DBUS_OS_INSTALL_OS_INSTALL_CLIENT_H_
#define CHROMEOS_ASH_COMPONENTS_DBUS_OS_INSTALL_OS_INSTALL_CLIENT_H_

#include <string>

#include "base/component_export.h"
#include "base/observer_list_types.h"

namespace dbus {
class Bus;
}

namespace ash {

// OsInstallClient is used to communicate with the
// org.chromium.OsInstallService system service. The browser uses this
// service to install the OS from a USB device to disk.
class COMPONENT_EXPORT(OS_INSTALL) OsInstallClient {
 public:
  // These values are persisted to logs. Entries should not be renumbered and
  // numeric values should never be reused.
  enum class Status {
    InProgress = 0,
    Succeeded = 1,
    Failed = 2,
    NoDestinationDeviceFound = 3,

    kMaxValue = NoDestinationDeviceFound,
  };

  // Interface for observing changes from the OS install service.
  class Observer : public base::CheckedObserver {
   public:
    // Called when the installation status changes.
    virtual void StatusChanged(Status status,
                               const std::string& service_log) = 0;
  };

  class TestInterface {
   public:
    virtual void UpdateStatus(Status status) = 0;
  };

  OsInstallClient(const OsInstallClient&) = delete;
  OsInstallClient& operator=(const OsInstallClient&) = delete;

  // Creates and initializes the global instance. |bus| must not be null.
  static void Initialize(dbus::Bus* bus);

  // Creates and initializes a fake global instance if not already created.
  static void InitializeFake();

  // Destroys the global instance.
  static void Shutdown();

  // Returns the global instance which may be null if not initialized.
  static OsInstallClient* Get();

  // Adds and removes the observer.
  virtual void AddObserver(Observer* observer) = 0;
  virtual void RemoveObserver(Observer* observer) = 0;
  // Returns true if this object has the given observer.
  virtual bool HasObserver(const Observer* observer) const = 0;

  virtual TestInterface* GetTestInterface() = 0;

  // Start the installation process. Status updates can be monitored
  // by adding an Observer.
  virtual void StartOsInstall() = 0;

 protected:
  OsInstallClient();
  virtual ~OsInstallClient();
};

}  // namespace ash

#endif  // CHROMEOS_ASH_COMPONENTS_DBUS_OS_INSTALL_OS_INSTALL_CLIENT_H_
