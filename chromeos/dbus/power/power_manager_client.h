// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_DBUS_POWER_POWER_MANAGER_CLIENT_H_
#define CHROMEOS_DBUS_POWER_POWER_MANAGER_CLIENT_H_

#include <map>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "base/component_export.h"
#include "base/files/scoped_file.h"
#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list_types.h"
#include "base/power_monitor/power_observer.h"
#include "base/time/time.h"
#include "chromeos/dbus/common/dbus_callback.h"
#include "chromeos/dbus/power_manager/backlight.pb.h"
#include "chromeos/dbus/power_manager/battery_saver.pb.h"
#include "chromeos/dbus/power_manager/charge_history_state.pb.h"
#include "chromeos/dbus/power_manager/peripheral_battery_status.pb.h"
#include "chromeos/dbus/power_manager/policy.pb.h"
#include "chromeos/dbus/power_manager/power_supply_properties.pb.h"
#include "chromeos/dbus/power_manager/suspend.pb.h"
#include "third_party/cros_system_api/dbus/power_manager/dbus-constants.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

namespace base {
class UnguessableToken;
}

namespace dbus {
class Bus;
}

namespace power_manager {
class BacklightBrightnessChange;
class ScreenIdleState;
class SetBacklightBrightnessRequest;
class SetAmbientLightSensorEnabledRequest;
}  // namespace power_manager

namespace chromeos {

// PowerManagerClient is used to communicate with the power manager.
class COMPONENT_EXPORT(DBUS_POWER) PowerManagerClient {
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
  class Observer : public base::CheckedObserver {
   public:
    // Called when the power manager service becomes available. Will be called
    // immediately and synchronously when a new observer is added to
    // PowerManagerClient if the service's availability is already known.
    virtual void PowerManagerBecameAvailable(bool available) {}

    // Called when the power manager is completely initialized.
    virtual void PowerManagerInitialized() {}

    // Called if the power manager process restarts.
    virtual void PowerManagerRestarted() {}

    // Called when the screen brightness is changed.
    virtual void ScreenBrightnessChanged(
        const power_manager::BacklightBrightnessChange& change) {}

    // Called when the ambient light sensor status changes.
    virtual void AmbientLightSensorEnabledChanged(
        const power_manager::AmbientLightSensorChange& change) {}

    // Called when the keyboard ambient light sensor status changes.
    virtual void KeyboardAmbientLightSensorEnabledChanged(
        const power_manager::AmbientLightSensorChange& change) {}

    // Called when the ambient light changed.
    virtual void AmbientColorChanged(const int32_t color_temperature) {}

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

    // Called when the state of Battery Saver Mode has changed, and on powerd
    // startup.
    virtual void BatterySaverModeStateChanged(
        const power_manager::BatterySaverModeState& state) {}

    // Called when peripheral device battery status is received.
    // |path| is the sysfs path for the battery of the peripheral device.
    // |name| is the human-readable name of the device.
    // |level| within [0, 100] represents the device battery level and -1
    // means an unknown level or device is disconnected.
    // |status| charging status, primarily for peripheral chargers.
    // Note that peripherals and peripheral chargers may be separate
    // (such as stylus vs. internal stylus charger), and have two distinct
    // charge levels.
    // |serial_number| Text representation of peripheral S/N, if available
    // and retrievable, empty string otherwise.
    // |active_update| true if peripheral event triggered update, false
    // if due to periodic poll or restart, and value may be stale.
    virtual void PeripheralBatteryStatusReceived(
        const std::string& path,
        const std::string& name,
        int level,
        power_manager::PeripheralBatteryStatus_ChargeStatus status,
        const std::string& serial_number,
        bool active_update) {}

    // Called when updated information about the power supply is available.
    // The status is automatically updated periodically, but
    // RequestStatusUpdate() can be used to trigger an immediate update.
    virtual void PowerChanged(
        const power_manager::PowerSupplyProperties& proto) {}

    // Called when the system is about to suspend. Suspend is deferred until
    // all observers' implementations of this method have finished running.
    //
    // If an observer wishes to asynchronously delay suspend,
    // PowerManagerClient::BlockSuspend() may be called from within
    // SuspendImminent().  UnblockSuspend() must be called once the observer is
    // ready for suspend.
    virtual void SuspendImminent(
        power_manager::SuspendImminent::Reason reason) {}

    // Called when a suspend attempt (previously announced via
    // SuspendImminent()) has completed. The system may not have actually
    // suspended (if e.g. the user canceled the suspend attempt).
    virtual void SuspendDone(base::TimeDelta sleep_duration) {}

    // Called when a suspend attempt (previously announced via
    // SuspendImminent()) has completed. The system may not have actually
    // suspended (if e.g. the user canceled the suspend attempt). This is the
    // same callback as SuspendDone() except that it receives the complete
    // SuspendDone protobuf rather than only the sleep duration. Clients that
    // override SuspendDoneEx() will not also get a SuspendDone() callback.
    virtual void SuspendDoneEx(const power_manager::SuspendDone& proto);

    // Called when the system is about to resuspend from a dark resume.  Like
    // SuspendImminent(), the suspend will be deferred until all observers have
    // finished running and those observers that wish to asynchronously delay
    // the suspend should call PowerManagerClient::BlockSuspend()
    // from within this method.  UnblockSuspend() must be called once the
    // observer is ready for suspend.
    virtual void DarkSuspendImminent() {}

    // Called when the browser is about to request system restart. Restart is
    // deferred until all observers' implementations of this method have
    // finished running.
    virtual void RestartRequested(power_manager::RequestRestartReason reason) {}

    // Called when the browser is about to request shutdown. Shutdown is
    // deferred until all observers' implementations of this method have
    // finished running.
    virtual void ShutdownRequested(
        power_manager::RequestShutdownReason reason) {}

    // Called when the power button is pressed or released.
    virtual void PowerButtonEventReceived(bool down,
                                          base::TimeTicks timestamp) {}

    // Called when the device's lid is opened or closed. LidState::NOT_PRESENT
    // is never passed.
    virtual void LidEventReceived(LidState state, base::TimeTicks timestamp) {}

    // Called when the device's tablet mode switch is on or off.
    // TabletMode::UNSUPPORTED is never passed.
    virtual void TabletModeEventReceived(TabletMode mode,
                                         base::TimeTicks timestamp) {}

    // Called when the idle action will be performed after
    // |time_until_idle_action|.
    virtual void IdleActionImminent(base::TimeDelta time_until_idle_action) {}

    // Called after IdleActionImminent() when the inactivity timer is reset
    // before the idle action has been performed.
    virtual void IdleActionDeferred() {}
  };

  // Adds and removes the observer.
  virtual void AddObserver(Observer* observer) = 0;
  virtual void RemoveObserver(Observer* observer) = 0;
  virtual bool HasObserver(const Observer* observer) const = 0;

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

  // Sets whether the ambient light sensor should be used in brightness
  // calculations.
  virtual void SetAmbientLightSensorEnabled(
      const power_manager::SetAmbientLightSensorEnabledRequest& request) = 0;

  // Asynchronously gets whether the ambient light sensor is currently enabled
  // (i.e. whether it's being used in brightness calculations). On error (e.g.
  // powerd not running), |callback| will be run with nullopt.
  virtual void GetAmbientLightSensorEnabled(
      DBusMethodCallback<bool> callback) = 0;

  // Asynchronously gets whether the device has at least one ambient light
  // sensor. On error (e.g. powerd not running), |callback| will be run with
  // nullopt.
  virtual void HasAmbientLightSensor(DBusMethodCallback<bool> callback) = 0;

  // Check if the keyboard has a backlight.
  virtual void HasKeyboardBacklight(DBusMethodCallback<bool> callback) = 0;

  // Decreases the keyboard brightness.
  virtual void DecreaseKeyboardBrightness() = 0;

  // Increases the keyboard brightness.
  virtual void IncreaseKeyboardBrightness() = 0;

  // Similar to GetScreenBrightnessPercent, but gets the keyboard brightness
  // instead.
  virtual void GetKeyboardBrightnessPercent(
      DBusMethodCallback<double> callback) = 0;

  // Sets the keyboard backlight brightness per |request|.
  virtual void SetKeyboardBrightness(
      const power_manager::SetBacklightBrightnessRequest& request) = 0;

  // Toggle the keyboard backlight on or off.
  virtual void ToggleKeyboardBacklight() = 0;

  // Sets whether the ambient light sensor should be used in keyboard brightness
  // calculations.
  virtual void SetKeyboardAmbientLightSensorEnabled(
      const power_manager::SetAmbientLightSensorEnabledRequest& request) = 0;

  // Asynchronously gets whether the keyboard ambient light sensor is currently
  // enabled. On error (e.g. powerd not running), |callback| will be run with
  // nullopt.
  virtual void GetKeyboardAmbientLightSensorEnabled(
      DBusMethodCallback<bool> callback) = 0;

  // Returns the last power status that was received from D-Bus, if any.
  virtual const std::optional<power_manager::PowerSupplyProperties>&
  GetLastStatus() = 0;

  // Requests an updated copy of the power status. Observer::PowerChanged()
  // will be called asynchronously.
  virtual void RequestStatusUpdate() = 0;

  // Requests all peripheral batteries have status re-issued.
  // Observer::PeripheralBatteryStatusReceived() will be called asynchronously,
  virtual void RequestAllPeripheralBatteryUpdate() = 0;

  // Requests the current thermal state.
  virtual void RequestThermalState() = 0;

  // Requests suspend of the system. If |duration_secs| is non-zero, an alarm
  // will be set to wake up the system after this many seconds (a dark resume).
  // |flavor| is a platform-specific flavor of suspend (to RAM, disk, etc.).
  // |wakeup_count| is an optional wakeup count to pass to powerd.
  virtual void RequestSuspend(std::optional<uint64_t> wakeup_count,
                              int32_t duration_secs,
                              power_manager::RequestSuspendFlavor flavor) = 0;

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

  // Notifies the power manager that a wake notification, i.e. a notification
  // that is allowed to wake up the device from suspend, was just created or
  // updated.
  virtual void NotifyWakeNotification() = 0;

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

  // Gets the current state of Battery Saver Mode. On error (e.g. powerd not
  // running), |callback| will be called with nullopt.
  virtual void GetBatterySaverModeState(
      DBusMethodCallback<power_manager::BatterySaverModeState> callback) = 0;

  // Updates the state of Battery Saver Mode.
  virtual void SetBatterySaverModeState(
      const power_manager::SetBatterySaverModeStateRequest& request) = 0;

  // Asynchronously fetches the current state of various hardware switches (e.g.
  // the lid switch and the tablet-mode switch). On error (e.g. powerd not
  // running), |callback| will be called with nullopt.
  virtual void GetSwitchStates(DBusMethodCallback<SwitchStates> callback) = 0;

  // Gets the inactivity delays currently used by powerd. Some or all of these
  // delays may be temporarily ignored due to e.g. wake locks or audio activity.
  virtual void GetInactivityDelays(
      DBusMethodCallback<power_manager::PowerManagementPolicy::Delays>
          callback) = 0;

  // Used by client code to temporarily block an imminent suspend (for up to
  // kSuspendDelayTimeoutMs, i.e. 5 seconds). See Observer::SuspendImminent.
  // |debug_info| should be a human-readable string that assists in determining
  // what code called BlockSuspend(). Afterwards, callers must release the block
  // via UnblockSuspend.
  virtual void BlockSuspend(const base::UnguessableToken& token,
                            const std::string& debug_info) = 0;

  // Used to indicate that the client code which passed |token| before is now
  // ready for a suspend.
  virtual void UnblockSuspend(const base::UnguessableToken& token) = 0;

  // Creates timers corresponding to clocks present in |arc_timer_requests|.
  // ScopedFDs are used to indicate timer expiration as described in
  // |StartArcTimer|. Aysnchronously runs |callback| with the created timers'
  // ids corresponding to all clocks in the arguments i.e timer id at index 0
  // corresponds to the clock id at position 0 in |arc_timer_requests|. Only one
  // timer per clock is allowed per tag, asynchronously runs |callback| with
  // std::nullopt if the same clock is present more than once in the arguments.
  // Also, runs |callback| with std::nullopt if timers are already created for
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

  // The time power manager will wait before resuspending from a dark resume.
  virtual base::TimeDelta GetDarkSuspendDelayTimeout() = 0;

  // On devices that support external displays with ambient light sensors, this
  // enables/disables the ALS-based brightness adjustment on those displays.
  virtual void SetExternalDisplayALSBrightness(bool enabled) = 0;

  // On devices that support external displays with ambient light sensors, this
  // returns true when the ALS-based brightness feature is enabled on those
  // displays.
  virtual void GetExternalDisplayALSBrightness(
      DBusMethodCallback<bool> callback) = 0;

  // Stop delaying charging for Adaptive Charging for this charge session.
  // This should be called when AdaptiveCharging is active (although calling it
  // when AdaptiveCharging is inactive will not cause any issue except extra
  // execution which does nothing).
  virtual void ChargeNowForAdaptiveCharging() = 0;

  // Get charge history for Adaptive Charging.
  virtual void GetChargeHistoryForAdaptiveCharging(
      DBusMethodCallback<power_manager::ChargeHistoryState> callback) = 0;

  PowerManagerClient();

  PowerManagerClient(const PowerManagerClient&) = delete;
  PowerManagerClient& operator=(const PowerManagerClient&) = delete;

  virtual ~PowerManagerClient();

  // Creates and initializes the global instance. |bus| must not be null.
  static void Initialize(dbus::Bus* bus);

  // Creates and initializes a fake global instance if not already created.
  static void InitializeFake();

  // Destroys the global instance which must have been initialized.
  static void Shutdown();

  // Returns the global instance if initialized. May return null.
  static PowerManagerClient* Get();
};

}  // namespace chromeos

#endif  // CHROMEOS_DBUS_POWER_POWER_MANAGER_CLIENT_H_
