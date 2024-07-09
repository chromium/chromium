// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_DBUS_SENESCHAL_SENESCHAL_CLIENT_H_
#define CHROMEOS_ASH_COMPONENTS_DBUS_SENESCHAL_SENESCHAL_CLIENT_H_

#include "base/component_export.h"
#include "base/observer_list_types.h"
#include "chromeos/ash/components/dbus/seneschal/seneschal_service.pb.h"
#include "chromeos/dbus/common/dbus_callback.h"
#include "chromeos/dbus/common/dbus_client.h"
#include "dbus/object_proxy.h"

namespace ash {

// SeneschalClient is used to communicate with Seneschal, which manages
// 9p file servers.
class COMPONENT_EXPORT(SENESCHAL) SeneschalClient
    : public chromeos::DBusClient {
 public:
  class Observer : public base::CheckedObserver {
   public:
    // Called when Seneschal service exits.
    virtual void SeneschalServiceStopped() = 0;
    // Called when Seneschal service is either started or restarted.
    virtual void SeneschalServiceStarted() = 0;
  };

  // Adds an observer.
  virtual void AddObserver(Observer* observer) = 0;

  // Removes an observer if added.
  virtual void RemoveObserver(Observer* observer) = 0;

  SeneschalClient(const SeneschalClient&) = delete;
  SeneschalClient& operator=(const SeneschalClient&) = delete;

  ~SeneschalClient() override;

  // Factory function, creates a new instance and returns ownership.
  // For normal usage, access the singleton via Get().
  static std::unique_ptr<SeneschalClient> Create();

  // Registers |callback| to run when the Concierge service becomes available.
  // If the service is already available, or if connecting to the name-owner-
  // changed signal fails, |callback| will be run once asynchronously.
  // Otherwise, |callback| will be run once in the future after the service
  // becomes available.
  virtual void WaitForServiceToBeAvailable(
      dbus::ObjectProxy::WaitForServiceToBeAvailableCallback callback) = 0;

  // Shares a path in the Chrome OS host with the container.
  // |callback| is called after the method call finishes.
  virtual void SharePath(
      const vm_tools::seneschal::SharePathRequest& request,
      chromeos::DBusMethodCallback<vm_tools::seneschal::SharePathResponse>
          callback) = 0;

  // Unshares a path in the Chrome OS host with the container.
  // |callback| is called after the method call finishes.
  virtual void UnsharePath(
      const vm_tools::seneschal::UnsharePathRequest& request,
      chromeos::DBusMethodCallback<vm_tools::seneschal::UnsharePathResponse>
          callback) = 0;

  // Creates and initializes the global instance. |bus| must not be null.
  static void Initialize(dbus::Bus* bus);

  // Creates and initializes a fake global instance if not already created.
  static void InitializeFake();

  // Destroys the global instance which must have been initialized.
  static void Shutdown();

  // Returns the global instance if initialized. May return null.
  static SeneschalClient* Get();

 protected:
  // Initialize() should be used instead.
  SeneschalClient();
};

}  // namespace ash

#endif  // CHROMEOS_ASH_COMPONENTS_DBUS_SENESCHAL_SENESCHAL_CLIENT_H_
