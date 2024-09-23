// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_DBUS_CHUNNELD_CHUNNELD_CLIENT_H_
#define CHROMEOS_ASH_COMPONENTS_DBUS_CHUNNELD_CHUNNELD_CLIENT_H_

#include "base/component_export.h"
#include "base/observer_list_types.h"
#include "chromeos/ash/components/dbus/chunneld/chunneld_service.pb.h"
#include "chromeos/dbus/common/dbus_callback.h"
#include "chromeos/dbus/common/dbus_client.h"
#include "dbus/object_proxy.h"

namespace ash {

// ChunneldClient is used to communicate with chunneld and monitor chunneld.
class COMPONENT_EXPORT(ASH_DBUS_CHUNNELD) ChunneldClient
    : public chromeos::DBusClient {
 public:
  class Observer : public base::CheckedObserver {
   public:
    // Called when Chunneld service exits.
    virtual void ChunneldServiceStopped() = 0;
    // Called when Chunneld service is either started or restarted.
    virtual void ChunneldServiceStarted() = 0;
  };

  // Returns the global instance if initialized. May return null.
  static ChunneldClient* Get();

  // Creates and initializes the global instance. |bus| must not be null.
  static void Initialize(dbus::Bus* bus);

  // Creates and initializes a fake global instance used on Linux desktop, if
  // no instance already exists.
  static void InitializeFake();

  // Destroys the global instance if it has been initialized.
  static void Shutdown();

  ChunneldClient(const ChunneldClient&) = delete;
  ChunneldClient& operator=(const ChunneldClient&) = delete;

  // Adds an observer.
  virtual void AddObserver(Observer* observer) = 0;

  // Removes an observer if added.
  virtual void RemoveObserver(Observer* observer) = 0;

  // Registers |callback| to run when the Concierge service becomes available.
  // If the service is already available, or if connecting to the name-owner-
  // changed signal fails, |callback| will be run once asynchronously.
  // Otherwise, |callback| will be run once in the future after the service
  // becomes available.
  virtual void WaitForServiceToBeAvailable(
      dbus::ObjectProxy::WaitForServiceToBeAvailableCallback callback) = 0;

 protected:
  // Initialize() should be used instead.
  ChunneldClient();
  ~ChunneldClient() override;
};

}  // namespace ash

#endif  // CHROMEOS_ASH_COMPONENTS_DBUS_CHUNNELD_CHUNNELD_CLIENT_H_
