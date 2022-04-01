// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_DBUS_RESOURCED_RESOURCED_CLIENT_H_
#define CHROMEOS_DBUS_RESOURCED_RESOURCED_CLIENT_H_

#include "base/component_export.h"
#include "base/observer_list_types.h"
#include "chromeos/dbus/common/dbus_method_call_status.h"

namespace dbus {
class Bus;
}

namespace chromeos {

// ResourcedClient is used to communicate with the org.chromium.ResourceManager
// service. The browser uses the ResourceManager service to get resource usage
// status.
class COMPONENT_EXPORT(RESOURCED) ResourcedClient {
 public:
  enum PressureLevel {
    // There is enough memory to use.
    NONE = 0,
    // Chrome is advised to free buffers that are cheap to re-allocate and not
    // immediately needed.
    MODERATE = 1,
    // Chrome is advised to free all possible memory.
    CRITICAL = 2,
  };

  // Observer class for memory pressure signal.
  class Observer : public base::CheckedObserver {
   public:
    ~Observer() override = default;

    virtual void OnMemoryPressure(PressureLevel level,
                                  uint64_t reclaim_target_kb) = 0;
  };

  enum class PressureLevelArcVm {
    // There is enough memory to use.
    NONE = 0,
    // ARCVM is advised to kill cached apps to free memory.
    CACHED = 1,
    // ARCVM is advised to kill perceptible apps to free memory.
    PERCEPTIBLE = 2,
    // ARCVM is advised to kill foreground apps to free memory.
    FOREGROUND = 3,
  };

  // Observer class for ARCVM memory pressure signal.
  class ArcVmObserver : public base::CheckedObserver {
   public:
    ~ArcVmObserver() override = default;

    virtual void OnMemoryPressure(PressureLevelArcVm level,
                                  uint64_t reclaim_target_kb) = 0;
  };

  ResourcedClient(const ResourcedClient&) = delete;
  ResourcedClient& operator=(const ResourcedClient&) = delete;

  // Creates and initializes the global instance. |bus| must not be null.
  static void Initialize(dbus::Bus* bus);

  // Creates and initializes a fake global instance if not already created.
  static void InitializeFake();

  // Destroys the global instance.
  static void Shutdown();

  // Returns the global instance which may be null if not initialized.
  static ResourcedClient* Get();

  // Attempts to enter game mode if state is true, exit if state is false.
  // Will automatically exit game mode once refresh_seconds is reached.
  // Callback will be called with whether game mode was on prior to this.
  virtual void SetGameModeWithTimeout(bool state,
                                      uint32_t refresh_seconds,
                                      DBusMethodCallback<bool> callback) = 0;

  using SetMemoryMarginsBpsCallback =
      base::OnceCallback<void(bool, uint64_t, uint64_t)>;

  // Informs resourced that it should use a different value for the critical
  // threshold. The value provided for |critical_bps| and |moderate_bps| must be
  // in basis points which represent one-one hundredth of a percent, eg. 1354
  // = 13.54%.
  virtual void SetMemoryMarginsBps(uint32_t critical_bps,
                                   uint32_t moderate_bps,
                                   SetMemoryMarginsBpsCallback callback) = 0;

  // Adds an observer to the observer list to listen on memory pressure events.
  virtual void AddObserver(Observer* observer) = 0;

  // Removes an observer from observer list.
  virtual void RemoveObserver(Observer* observer) = 0;

  // Adds an observer to be called when there is an ARCVM memory pressure
  // signal.
  virtual void AddArcVmObserver(ArcVmObserver* observer) = 0;

  // Stops a previously added observer from being called on ARCVM memory
  // pressure signals.
  virtual void RemoveArcVmObserver(ArcVmObserver* observer) = 0;

 protected:
  ResourcedClient();
  virtual ~ResourcedClient();
};

}  // namespace chromeos

// TODO(https://crbug.com/1164001): remove when moved to ash.
namespace ash {
using ::chromeos::ResourcedClient;
}  // namespace ash

#endif  // CHROMEOS_DBUS_RESOURCED_RESOURCED_CLIENT_H_
