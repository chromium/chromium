// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_DBUS_RESOURCED_RESOURCED_CLIENT_H_
#define CHROMEOS_ASH_COMPONENTS_DBUS_RESOURCED_RESOURCED_CLIENT_H_

#include <cstdint>
#include <vector>

#include "base/component_export.h"
#include "base/observer_list_types.h"
#include "base/process/process_handle.h"
#include "base/threading/platform_thread.h"
#include "base/time/time.h"
#include "chromeos/dbus/common/dbus_callback.h"
#include "components/memory_pressure/reclaim_target.h"
#include "dbus/dbus_result.h"
#include "dbus/object_proxy.h"
#include "third_party/cros_system_api/dbus/resource_manager/dbus-constants.h"

namespace dbus {
class Bus;
}

namespace ash {

class FakeResourcedClient;

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
                                  memory_pressure::ReclaimTarget target) = 0;
  };

  // Indicates whether game mode is on, and which kind of game mode if it is on.
  // Borealis game mode will put more memory pressure on ARCVM processes than
  // will ARC game mode.
  // D-Bus serializes this as a u8, hence the uint8_t specifier.
  enum class GameMode : uint8_t {
    OFF = 0,
    BOREALIS = 1,
    ARC = 2,
  };

  enum class PressureLevelArcContainer {
    // There is enough memory to use.
    kNone = 0,
    // ARC container is advised to kill cached apps to free memory.
    kCached = 1,
    // ARC container is advised to kill perceptible apps to free memory.
    kPerceptible = 2,
    // ARC container is advised to kill foreground apps to free memory.
    kForeground = 3,
  };

  // Observer class for ARC container memory pressure signal.
  class ArcContainerObserver : public base::CheckedObserver {
   public:
    ~ArcContainerObserver() override = default;

    virtual void OnMemoryPressure(PressureLevelArcContainer level,
                                  uint64_t reclaim_target_kb) = 0;
  };

  ResourcedClient(const ResourcedClient&) = delete;
  ResourcedClient& operator=(const ResourcedClient&) = delete;

  // Creates and initializes the global instance. |bus| must not be null.
  static void Initialize(dbus::Bus* bus);

  // Creates and initializes a fake global instance if not already created.
  // The newly created object will persist until Shutdown() is called.
  static FakeResourcedClient* InitializeFake();

  // Destroys the global instance.
  static void Shutdown();

  // Returns the global instance which may be null if not initialized.
  static ResourcedClient* Get();

  // Attempts to enter or exit game mode depending on the value of |game_mode|.
  // Will automatically exit game mode once refresh_seconds is reached.
  // Callback will be called with whether game mode was on prior to this.
  virtual void SetGameModeWithTimeout(
      GameMode game_mode,
      uint32_t refresh_seconds,
      chromeos::DBusMethodCallback<GameMode> callback) = 0;

  // The bps fields are in basis points which represent one-one hundredth of a
  // percent, e.g., 1354 bps = 13.54%.
  struct MemoryMargins {
    uint32_t moderate_bps = 0;
    uint32_t critical_bps = 0;
    uint32_t critical_protected_bps = 0;
  };

  // Informs resourced that it should use a different value for the memory
  // margins.
  virtual void SetMemoryMargins(MemoryMargins margins) = 0;

  enum class Component {
    kAsh = 0,
    kLacros = 1,
  };

  struct Process {
    Process(base::ProcessHandle pid,
            bool is_protected,
            bool is_visible,
            bool is_focused,
            base::TimeTicks last_visible)
        : pid(pid),
          is_protected(is_protected),
          is_visible(is_visible),
          is_focused(is_focused),
          last_visible(last_visible) {}
    base::ProcessHandle pid;
    bool is_protected;
    bool is_visible;
    bool is_focused;
    base::TimeTicks last_visible;
  };

  virtual void ReportBrowserProcesses(
      Component component,
      const std::vector<Process>& processes) = 0;

  using SetQoSStateCallback = base::OnceCallback<void(dbus::DBusResult)>;

  // Set qos state of a process.
  virtual void SetProcessState(base::ProcessId,
                               resource_manager::ProcessState,
                               SetQoSStateCallback) = 0;

  // Set qos state of a thread.
  virtual void SetThreadState(base::ProcessId,
                              base::PlatformThreadId,
                              resource_manager::ThreadState,
                              SetQoSStateCallback) = 0;

  // Adds an observer to the observer list to listen on memory pressure events.
  virtual void AddObserver(Observer* observer) = 0;

  // Removes an observer from observer list.
  virtual void RemoveObserver(Observer* observer) = 0;

  virtual void AddArcContainerObserver(ArcContainerObserver* observer) = 0;

  virtual void RemoveArcContainerObserver(ArcContainerObserver* observer) = 0;

  // Registers |callback| to run when the ResourceManager service becomes
  // available. If the service is already available, or if connecting to the
  // name-owner-changed signal fails, |callback| will be run once
  // asynchronously.
  // Otherwise, |callback| will be run once in the future after the service
  // becomes available.
  virtual void WaitForServiceToBeAvailable(
      dbus::ObjectProxy::WaitForServiceToBeAvailableCallback callback) = 0;

 protected:
  ResourcedClient();
  virtual ~ResourcedClient();
};

}  // namespace ash

#endif  // CHROMEOS_ASH_COMPONENTS_DBUS_RESOURCED_RESOURCED_CLIENT_H_
