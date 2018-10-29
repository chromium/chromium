// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_DBUS_POWER_MANAGER_CLIENT_H_
#define CHROMEOS_DBUS_POWER_MANAGER_CLIENT_H_

#include <map>
#include <string>
#include <utility>
#include <vector>

#include "base/callback.h"
#include "base/files/scoped_file.h"
#include "base/location.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/optional.h"
#include "base/time/time.h"
#include "chromeos/chromeos_export.h"
#include "chromeos/dbus/dbus_client.h"
#include "chromeos/dbus/dbus_client_implementation_type.h"
#include "chromeos/dbus/dbus_method_call_status.h"
#include "chromeos/dbus/power_manager/policy.pb.h"
#include "chromeos/dbus/power_manager/power_supply_properties.pb.h"
#include "chromeos/dbus/power_manager/suspend.pb.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

namespace power_manager {
class BacklightBrightnessChange;
class ScreenIdleState;
class SetBacklightBrightnessRequest;
}  // namespace power_manager

namespace chromeos {

// PowerManagerClient is used to communicate with the power manager.
class CHROMEOS_EXPORT PowerManagerClient : public DBusClient {
 public:
  using TimerId = int32_t;

  enum class LidState {
    OPEN,
    CLOSED,
    NOT_PRESENT,
  };

  enum class TabletMode {
    ON,
    OFF,
    UNSUPPORTED,
  };

  struct SwitchStates {
    LidState lid_state;
    TabletMode tablet_mode;
  };

  // Interface for observing changes from the power manager.
  class Observer {
   public:
    virtual ~Observer() {}

    // Called if the power manager process restarts.
    virtual void PowerManagerRestarted() {}

    // Called when the screen brightness is changed.
    virtual void ScreenBrightnessChanged(
        const power_manager::BacklightBrightnessChange& change) {}

    // Called when the keyboard brightness is changed.
    virtual void KeyboardBrightnessChanged(
        const power_manager::BacklightBrightnessChange& change) {}

    // Called when screen-related inactivity timeouts are triggered or reset.
    virtual void ScreenIdleStateChanged(
        const power_manager::ScreenIdleState& proto) {}

    // Called when powerd announces a change to the current inactivity delays.
    // Some or all of these delays may be temporarily ignored due to e.g. wake
    // locks or audio activity.
    virtual void InactivityDelaysChanged(
        const power_manager::PowerManagementPolicy::Delays& delays) {}

    // Called when peripheral device battery status is received.
    // |path| is the sysfs path for the battery of the peripheral device.
    // |name| is the human readble name of the device.
    // |level| within [0, 100] represents the device battery level and -1
    // means an unknown level or device is disconnected.
    virtual void PeripheralBatteryStatusReceived(const std::string& path,
                                                 const std::string& name,
                                                 int level) {}

    // Called when updated information about the power supply is available.
    // The status is automatically updated periodically, but
    // RequestStatusUpdate() can be used to trigger an immediate update.
    virtual void PowerChanged(
        const power_manager::PowerSupplyProperties& proto) {}

    // Called when the system is about to suspend. Suspend is deferred until
    // all observers' implementations of this method have finished running.
    //
    // If an observer wishes to asynchronously delay suspend,
    // PowerManagerClient::GetSuspendReadinessCallback() may be called from
    // within SuspendImminent().  The returned callback must be called once
    // the observer is ready for suspend.
    virtual void SuspendImminent(
        power_manager::SuspendImminent::Reason reason) {}

    // Called when a suspend attempt (previously announced via
    // SuspendImminent()) has completed. The system may not have actually
    // suspended (if e.g. the user canceled the suspend attempt).
    virtual void SuspendDone(const base::TimeDelta& sleep_duration) {}

    // Called when the system is about to resuspend from a dark resume.  Like
    // SuspendImminent(), the suspend will be deferred until all observers have
    // finished running and those observers that wish to asynchronously delay
    // the suspend should call PowerManagerClient::GetSuspendReadinessCallback()
    // from within this method.  The returned callback should be run once the
    // observer is ready for suspend.
    virtual void DarkSuspendImminent() {}

    // Called when the power button is pressed or released.
    virtual void PowerButtonEventReceived(bool down,
                                          const base::TimeTicks& timestamp) {}

    // Called when the device's lid is opened or closed. LidState::NOT_PRESENT
    // is never passed.
    virtual void LidEventReceived(LidState state,
                                  const base::TimeTicks& timestamp) {}

    // Called when the device's tablet mode switch is on or off.
    // TabletMode::UNSUPPORTED is never passed.
    virtual void TabletModeEventReceived(TabletMode mode,
                                         const base::TimeTicks& timestamp) {}

    // Called just before the screen is dimmed in response to user inactivity.
    virtual void ScreenDimImminent() {}

    // Called when the idle action will be performed after
    // |time_until_idle_action|.
    virtual void IdleActionImminent(
        const base::TimeDelta& time_until_idle_action) {}

    // Called after IdleActionImminent() when the inactivity timer is reset
    // before the idle action has been performed.
    virtual void IdleActionDeferred() {}
  };

  // Adds and removes the observer.
  virtual void AddObserver(Observer* observer) = 0;
  virtual void RemoveObserver(Observer* observer) = 0;
  virtual bool HasObserver(const Observer* observer) const = 0;

  // Runs the callback as soon as the service becomes available.
  virtual void WaitForServiceToBeAvailable(
      WaitForServiceToBeAvailableCallback callback) = 0;

  // Interface for managing the power consumption of renderer processes.
  class RenderProcessManagerDelegate {
   public:
    virtual ~RenderProcessManagerDelegate() {}

    // Called when a suspend attempt is imminent but after all registered
    // observers have reported readiness to suspend.  This is only called for
    // suspends from the fully powered on state and not for suspends from dark
    // resume.
    virtual void SuspendImminent() = 0;

    // Called when a previously announced suspend attempt has completed but
    // before observers are notified about it.
    virtual void SuspendDone() = 0;
  };

  // Sets the PowerManagerClient's RenderProcessManagerDelegate.  There can only
  // be one delegate.
  virtual void SetRenderProcessManagerDelegate(
      base::WeakPtr<RenderProcessManagerDelegate> delegate) = 0;

  // Decreases the screen brightness. |allow_off| controls whether or not
  // it's allowed to turn off the back light.
  virtual void DecreaseScreenBrightness(bool allow_off) = 0;

  // Increases the screen brightness.
  virtual void IncreaseScreenBrightness() = 0;

  // Sets the screen brightness per |request|.
  virtual void SetScreenBrightness(
      const power_manager::SetBacklightBrightnessRequest& request) = 0;

  // Asynchronously gets the current screen brightness, in the range
  // [0.0, 100.0]. On error (e.g. powerd not running), |callback| will be run
  // with nullopt.
  virtual void GetScreenBrightnessPercent(
      DBusMethodCallback<double> callback) = 0;

  // Decreases the keyboard brightness.
  virtual void DecreaseKeyboardBrightness() = 0;

  // Increases the keyboard brightness.
  virtual void IncreaseKeyboardBrightness() = 0;

  // Similar to GetScreenBrightnessPercent, but gets the keyboard brightness
  // instead.
  virtual void GetKeyboardBrightnessPercent(
      DBusMethodCallback<double> callback) = 0;

  // Returns the last power status that was received from D-Bus, if any.
  virtual const base::Optional<power_manager::PowerSupplyProperties>&
  GetLastStatus() = 0;

  // Requests an updated copy of the power status. Observer::PowerChanged()
  // will be called asynchronously.
  virtual void RequestStatusUpdate() = 0;

  // Requests suspend of the system.
  virtual void RequestSuspend() = 0;

  // Requests restart of the system. |description| contains a human-readable
  // string describing the source of the request that will be logged by powerd.
  virtual void RequestRestart(power_manager::RequestRestartReason reason,
                              const std::string& description) = 0;

  // Requests shutdown of the system. |description| contains a human-readable
  // string describing the source of the request that will be logged by powerd.
  virtual void RequestShutdown(power_manager::RequestShutdownReason reason,
                               const std::string& description) = 0;

  // Notifies the power manager that the user is active (i.e. generating input
  // events).
  virtual void NotifyUserActivity(power_manager::UserActivityType type) = 0;

  // Notifies the power manager that a video is currently playing. It also
  // includes whether or not the containing window for the video is fullscreen.
  virtual void NotifyVideoActivity(bool is_fullscreen) = 0;

  // Tells the power manager to begin using |policy|.
  virtual void SetPolicy(
      const power_manager::PowerManagementPolicy& policy) = 0;

  // Tells powerd whether or not we are in a projecting mode.  This is used to
  // adjust idleness thresholds and derived, on this side, from the number of
  // video outputs attached.
  virtual void SetIsProjecting(bool is_projecting) = 0;

  // Tells powerd to change the power source to the given ID. An empty string
  // causes powerd to switch to using the battery on devices with type-C ports.
  virtual void SetPowerSource(const std::string& id) = 0;

  // Forces the display and (if present) keyboard backlights to |forced_off|.
  // This method doesn't support multiple callers. Instead of calling it
  // directly, please use ash::BacklightsForcedOffSetter.
  virtual void SetBacklightsForcedOff(bool forced_off) = 0;

  // Gets the display and (if present) keyboard backlights' forced-off state. On
  // error (e.g. powerd not running), |callback| will be called with nullopt.
  virtual void GetBacklightsForcedOff(DBusMethodCallback<bool> callback) = 0;

  // Asynchronously fetches the current state of various hardware switches (e.g.
  // the lid switch and the tablet-mode switch). On error (e.g. powerd not
  // running), |callback| will be called with nullopt.
  virtual void GetSwitchStates(DBusMethodCallback<SwitchStates> callback) = 0;

  // Gets the inactivity delays currently used by powerd. Some or all of these
  // delays may be temporarily ignored due to e.g. wake locks or audio activity.
  virtual void GetInactivityDelays(
      DBusMethodCallback<power_manager::PowerManagementPolicy::Delays>
          callback) = 0;

  // Returns a callback that can be called by an observer to report readiness
  // for suspend. See Observer::SuspendImminent().
  virtual base::Closure GetSuspendReadinessCallback(
      const base::Location& from_where) = 0;

  // Returns the number of callbacks returned by GetSuspendReadinessCallback()
  // for the current suspend attempt but not yet called. Used by tests.
  virtual int GetNumPendingSuspendReadinessCallbacks() = 0;

  // Creates timers corresponding to clocks present in |arc_timer_requests|.
  // ScopedFDs are used to indicate timer expiration as described in
  // |StartArcTimer|. Aysnchronously runs |callback| with the created timers'
  // ids corresponding to all clocks in the arguments i.e timer id at index 0
  // corresponds to the clock id at position 0 in |arc_timer_requests|. Only one
  // timer per clock is allowed per tag, asynchronously runs |callback| with
  // base::nullopt if the same clock is present more than once in the arguments.
  // Also, runs |callback| with base::nullopt if timers are already created for
  // |tag|.
  virtual void CreateArcTimers(
      const std::string& tag,
      std::vector<std::pair<clockid_t, base::ScopedFD>> arc_timer_requests,
      DBusMethodCallback<std::vector<TimerId>> callback) = 0;

  // Starts a timer created via |CreateArcTimers|. Starts the timer of type
  // |clock_id| (from <sys/timerfd.h>) to run at |absolute_expiration_time| in
  // the future. If the timer is already running, it will be replaced.
  // Notification will be performed as an 8-byte write to the associated
  // expiration fd. Asynchronously runs |callback| with true iff the timer can
  // be started successfully or false otherwise.
  virtual void StartArcTimer(TimerId timer_id,
                             base::TimeTicks absolute_expiration_time,
                             VoidDBusMethodCallback callback) = 0;

  // Deletes all timer state and clears any pending timers started by
  // |StartArcTimer|. Asynchronously runs |callback| with true on success or
  // false on failure.
  virtual void DeleteArcTimers(const std::string& tag,
                               VoidDBusMethodCallback callback) = 0;

  // Instructs powerd to defer dimming the screen. This only has an effect when
  // called shortly (i.e. seconds) after observers have received
  // ScreenDimImminent notifications.
  virtual void DeferScreenDim() = 0;

  // Creates the instance.
  static PowerManagerClient* Create(DBusClientImplementationType type);

  ~PowerManagerClient() override;

 protected:
  // Needs to call DBusClient::Init().
  friend class PowerManagerClientTest;

  // Create() should be used instead.
  PowerManagerClient();

 private:
  DISALLOW_COPY_AND_ASSIGN(PowerManagerClient);
};

}  // namespace chromeos

#endif  // CHROMEOS_DBUS_POWER_MANAGER_CLIENT_H_
