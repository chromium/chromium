// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_DBUS_SYSTEM_CLOCK_SYSTEM_CLOCK_CLIENT_H_
#define CHROMEOS_ASH_COMPONENTS_DBUS_SYSTEM_CLOCK_SYSTEM_CLOCK_CLIENT_H_

#include <stdint.h>

#include "base/component_export.h"
#include "base/functional/callback.h"
#include "dbus/object_proxy.h"

namespace dbus {
class Bus;
}

namespace ash {

// SystemClockClient is used to communicate with the system clock. This class is
// safe to use from multiple processes (e.g. Chrome + Ash).
class COMPONENT_EXPORT(SYSTEM_CLOCK) SystemClockClient {
 public:
  using GetLastSyncInfoCallback = base::OnceCallback<void(bool synchronized)>;

  // Interface for observing changes from the system clock.
  class Observer {
   public:
    // Called when the status is updated.
    virtual void SystemClockUpdated() {}

    // Called when the system clock has become settable or unsettable, e.g.
    // when the clock syncs with or goes out of sync with the network.
    virtual void SystemClockCanSetTimeChanged(bool can_set_time) {}

   protected:
    virtual ~Observer() {}
  };

  // Interface for testing. Only implemented in the fake implementation.
  class TestInterface {
   public:
    // Sets the |synchronized| value passed to GetLastSyncInfo().
    virtual void SetNetworkSynchronized(bool network_synchronized) = 0;

    // Calls SystemClockUpdated for observers.
    virtual void NotifyObserversSystemClockUpdated() = 0;

    // If |is_available| is false callbacks passed to
    // WaitForServiceToBeAvailable will pile up, until |is_available| is set
    // back to true.
    virtual void SetServiceIsAvailable(bool is_available) = 0;

    // Configures service to be permanently disabled. Callbacks passed to
    // WaitForServiceToBeAvailable are immediately invoked with
    // |service_is_available| set to false. This includes any callbacks that
    // piled up after SetServiceIsAvailable(false). To enable service again,
    // invoke SetServiceIsAvailable(true);
    virtual void DisableService() = 0;
  };

  // Creates and initializes the global instance. |bus| must not be null.
  static void Initialize(dbus::Bus* bus);

  // Creates and initializes a fake global instance if not already created.
  static void InitializeFake();

  // Destroys the global instance which must have been initialized.
  static void Shutdown();

  // Returns the global instance if initialized. May return null.
  static SystemClockClient* Get();

  SystemClockClient(const SystemClockClient&) = delete;
  SystemClockClient& operator=(const SystemClockClient&) = delete;

  // Adds the given observer.
  virtual void AddObserver(Observer* observer) = 0;
  // Removes the given observer if this object has the observer.
  virtual void RemoveObserver(Observer* observer) = 0;
  // Returns true if this object has the given observer.
  virtual bool HasObserver(const Observer* observer) const = 0;

  // Sets the system clock.
  virtual void SetTime(int64_t time_in_seconds) = 0;

  // Checks if the system time can be set.
  virtual bool CanSetTime() = 0;

  // Runs |callback| asynchronously with the system time's current
  // synchronization state with network time.
  virtual void GetLastSyncInfo(GetLastSyncInfoCallback callback) = 0;

  // Runs the callback as soon as the service becomes available.
  virtual void WaitForServiceToBeAvailable(
      dbus::ObjectProxy::WaitForServiceToBeAvailableCallback callback) = 0;

  virtual TestInterface* GetTestInterface() = 0;

 protected:
  // Initialize/Shutdown should be used instead.
  SystemClockClient();
  virtual ~SystemClockClient();
};

}  // namespace ash

#endif  // CHROMEOS_ASH_COMPONENTS_DBUS_SYSTEM_CLOCK_SYSTEM_CLOCK_CLIENT_H_
