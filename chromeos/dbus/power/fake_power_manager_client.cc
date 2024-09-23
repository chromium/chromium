// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/dbus/power/fake_power_manager_client.h"

#include <set>
#include <utility>
#include <vector>

#include "base/containers/contains.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/posix/unix_domain_socket.h"
#include "base/strings/string_util.h"
#include "base/task/single_thread_task_runner.h"
#include "base/threading/thread_restrictions.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "chromeos/dbus/power_manager/backlight.pb.h"

namespace chromeos {

namespace {

FakePowerManagerClient* g_instance = nullptr;

// Minimum power for a USB power source to be classified as AC.
constexpr double kUsbMinAcWatts = 24;

// The time power manager will wait before resuspending from a dark resume.
constexpr base::TimeDelta kDarkSuspendDelayTimeout = base::Seconds(20);

// Callback fired when timer started through |StartArcTimer| expires. In
// non-test environments this does a potentially blocking call on the UI
// thread. However, the clients that exercise this code path don't run in
// non-test environments.
void ArcTimerExpirationCallback(int expiration_fd) {
  // The instance expects 8 bytes on the read end similar to what happens on
  // a timerfd expiration. The timerfd API expects this to be the number of
  // expirations, however, more than one expiration isn't tracked currently.
  const uint64_t timer_data = 1;
  if (!base::UnixDomainSocket::SendMsg(
          expiration_fd, &timer_data, sizeof(timer_data), std::vector<int>())) {
    PLOG(ERROR) << "Failed to indicate timer expiration to the instance";
  }
}

power_manager::BacklightBrightnessChange_Cause RequestCauseToChangeCause(
    power_manager::SetBacklightBrightnessRequest_Cause cause) {
  switch (cause) {
    case power_manager::SetBacklightBrightnessRequest_Cause_USER_REQUEST:
      return power_manager::BacklightBrightnessChange_Cause_USER_REQUEST;
    case power_manager::SetBacklightBrightnessRequest_Cause_MODEL:
      return power_manager::BacklightBrightnessChange_Cause_MODEL;
    case power_manager::
        SetBacklightBrightnessRequest_Cause_USER_REQUEST_FROM_SETTINGS_APP:
      return power_manager::
          BacklightBrightnessChange_Cause_USER_REQUEST_FROM_SETTINGS_APP;
    case power_manager::
        SetBacklightBrightnessRequest_Cause_RESTORED_FROM_USER_PREFERENCE:
      return power_manager::
          BacklightBrightnessChange_Cause_RESTORED_FROM_USER_PREFERENCE;
  }
  NOTREACHED_IN_MIGRATION() << "Unhandled brightness request cause " << cause;
  return power_manager::BacklightBrightnessChange_Cause_USER_REQUEST;
}

power_manager::AmbientLightSensorChange_Cause
AmbientLightSensorRequestCauseToChangeCause(
    power_manager::SetAmbientLightSensorEnabledRequest_Cause cause) {
  switch (cause) {
    case power_manager::
        SetAmbientLightSensorEnabledRequest_Cause_USER_REQUEST_FROM_SETTINGS_APP:
      return power_manager::
          AmbientLightSensorChange_Cause_USER_REQUEST_SETTINGS_APP;
    case power_manager::
        SetAmbientLightSensorEnabledRequest_Cause_RESTORED_FROM_USER_PREFERENCE:
      return power_manager::
          AmbientLightSensorChange_Cause_RESTORED_FROM_USER_PREFERENCE;
  }
  NOTREACHED_IN_MIGRATION() << "Unhandled brightness request cause " << cause;
  return power_manager::
      AmbientLightSensorChange_Cause_USER_REQUEST_SETTINGS_APP;
}

// Copied from Chrome's //base/time/time_now_posix.cc.
// Returns count of |clk_id| in the form of a time delta. Returns an empty
// time delta if |clk_id| isn't present on the system.
base::TimeDelta ClockNow(clockid_t clk_id) {
  struct timespec ts;
  if (clock_gettime(clk_id, &ts) != 0) {
    NOTREACHED_IN_MIGRATION() << "clock_gettime(" << clk_id << ") failed.";
    return base::TimeDelta();
  }
  return base::TimeDelta::FromTimeSpec(ts);
}

}  // namespace

// static
FakePowerManagerClient* FakePowerManagerClient::Get() {
  CHECK(g_instance);
  return g_instance;
}

FakePowerManagerClient::FakePowerManagerClient()
    : props_(power_manager::PowerSupplyProperties()), tick_clock_(nullptr) {
  CHECK(!g_instance);
  g_instance = this;

  props_->set_battery_percent(50);
  props_->set_is_calculating_battery_time(false);
  props_->set_battery_state(
      power_manager::PowerSupplyProperties_BatteryState_DISCHARGING);
  props_->set_external_power(
      power_manager::PowerSupplyProperties_ExternalPower_DISCONNECTED);
  props_->set_battery_time_to_full_sec(0);
  props_->set_battery_time_to_empty_sec(18000);
}

FakePowerManagerClient::~FakePowerManagerClient() {
  CHECK_EQ(g_instance, this);
  g_instance = nullptr;
}

void FakePowerManagerClient::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
  if (service_availability_.has_value()) {
    observer->PowerManagerBecameAvailable(service_availability_.value());
  }
  observer->PowerManagerInitialized();
}

void FakePowerManagerClient::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

bool FakePowerManagerClient::HasObserver(const Observer* observer) const {
  return observers_.HasObserver(observer);
}

void FakePowerManagerClient::SetRenderProcessManagerDelegate(
    base::WeakPtr<RenderProcessManagerDelegate> delegate) {
  render_process_manager_delegate_ = delegate;
}

void FakePowerManagerClient::DecreaseScreenBrightness(bool allow_off) {
  // Simulate the real behavior of the platform by disabling the ambient light
  // sensor when the brightness is manually changed.
  power_manager::SetAmbientLightSensorEnabledRequest set_als_request;
  set_als_request.set_sensor_enabled(false);
  SetAmbientLightSensorEnabled(set_als_request);
}

void FakePowerManagerClient::IncreaseScreenBrightness() {
  // Simulate the real behavior of the platform by disabling the ambient light
  // sensor when the brightness is manually changed.
  power_manager::SetAmbientLightSensorEnabledRequest set_als_request;
  set_als_request.set_sensor_enabled(false);
  SetAmbientLightSensorEnabled(set_als_request);
}

void FakePowerManagerClient::SetScreenBrightness(
    const power_manager::SetBacklightBrightnessRequest& request) {
  screen_brightness_percent_ = request.percent();
  requested_screen_brightness_percent_ = request.percent();
  requested_screen_brightness_cause_ = request.cause();

  power_manager::BacklightBrightnessChange change;
  change.set_percent(request.percent());
  change.set_cause(RequestCauseToChangeCause(request.cause()));
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(&FakePowerManagerClient::SendScreenBrightnessChanged,
                     weak_ptr_factory_.GetWeakPtr(), change));

  // Simulate the real behavior of the platform by disabling the ambient light
  // sensor when the brightness is manually changed.
  power_manager::SetAmbientLightSensorEnabledRequest set_als_request;
  set_als_request.set_sensor_enabled(false);
  SetAmbientLightSensorEnabled(set_als_request);
}

void FakePowerManagerClient::GetScreenBrightnessPercent(
    DBusMethodCallback<double> callback) {
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(std::move(callback), screen_brightness_percent_));
}

void FakePowerManagerClient::SetAmbientLightSensorEnabled(
    const power_manager::SetAmbientLightSensorEnabledRequest& request) {
  bool enabled = request.sensor_enabled();
  // If this is a no-op, don't emit a signal.
  if (is_ambient_light_sensor_enabled_ == enabled) {
    return;
  }

  is_ambient_light_sensor_enabled_ = enabled;
  requested_ambient_light_sensor_enabled_cause_ = request.cause();

  power_manager::AmbientLightSensorChange change;
  change.set_sensor_enabled(request.sensor_enabled());
  change.set_cause(
      AmbientLightSensorRequestCauseToChangeCause(request.cause()));

  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(
          &FakePowerManagerClient::SendAmbientLightSensorEnabledChanged,
          weak_ptr_factory_.GetWeakPtr(), change));
}

void FakePowerManagerClient::GetAmbientLightSensorEnabled(
    DBusMethodCallback<bool> callback) {
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(std::move(callback), is_ambient_light_sensor_enabled_));
}

void FakePowerManagerClient::HasAmbientLightSensor(
    DBusMethodCallback<bool> callback) {
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(std::move(callback), has_ambient_light_sensor_));
}

void FakePowerManagerClient::HasKeyboardBacklight(
    DBusMethodCallback<bool> callback) {
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), has_keyboard_backlight_));
}

void FakePowerManagerClient::DecreaseKeyboardBrightness() {
  // Simulate the real behavior of the platform by disabling the keyboard
  // ambient light sensor when the brightness is manually changed.
  power_manager::SetAmbientLightSensorEnabledRequest set_als_request;
  set_als_request.set_sensor_enabled(false);
  SetKeyboardAmbientLightSensorEnabled(set_als_request);
}

void FakePowerManagerClient::IncreaseKeyboardBrightness() {
  ++num_increase_keyboard_brightness_calls_;
  // Simulate the real behavior of the platform by disabling the keyboard
  // ambient light sensor when the brightness is manually changed.
  power_manager::SetAmbientLightSensorEnabledRequest set_als_request;
  set_als_request.set_sensor_enabled(false);
  SetKeyboardAmbientLightSensorEnabled(set_als_request);
}

void FakePowerManagerClient::GetKeyboardBrightnessPercent(
    DBusMethodCallback<double> callback) {
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(std::move(callback), keyboard_brightness_percent_));
}

void FakePowerManagerClient::SetKeyboardBrightness(
    const power_manager::SetBacklightBrightnessRequest& request) {
  keyboard_brightness_percent_ = request.percent();
  requested_keyboard_brightness_cause_ = request.cause();

  power_manager::BacklightBrightnessChange change;
  change.set_percent(request.percent());
  change.set_cause(RequestCauseToChangeCause(request.cause()));
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(&FakePowerManagerClient::SendKeyboardBrightnessChanged,
                     weak_ptr_factory_.GetWeakPtr(), change));
  // Simulate the real behavior of the platform by disabling the keyboard
  // ambient light sensor when the brightness is manually changed.
  power_manager::SetAmbientLightSensorEnabledRequest set_als_request;
  set_als_request.set_sensor_enabled(false);
  SetKeyboardAmbientLightSensorEnabled(set_als_request);
}

void FakePowerManagerClient::ToggleKeyboardBacklight() {}

void FakePowerManagerClient::SetKeyboardAmbientLightSensorEnabled(
    const power_manager::SetAmbientLightSensorEnabledRequest& request) {
  bool enabled = request.sensor_enabled();
  // If this is a no-op, don't emit a signal.
  if (keyboard_ambient_light_sensor_enabled_ == enabled) {
    return;
  }
  keyboard_ambient_light_sensor_enabled_ = enabled;

  power_manager::AmbientLightSensorChange change;
  change.set_sensor_enabled(request.sensor_enabled());
  change.set_cause(
      AmbientLightSensorRequestCauseToChangeCause(request.cause()));

  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(
          &FakePowerManagerClient::SendKeyboardAmbientLightSensorEnabledChanged,
          weak_ptr_factory_.GetWeakPtr(), change));
}

void FakePowerManagerClient::GetKeyboardAmbientLightSensorEnabled(
    DBusMethodCallback<bool> callback) {
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback),
                                keyboard_ambient_light_sensor_enabled_));
}

const std::optional<power_manager::PowerSupplyProperties>&
FakePowerManagerClient::GetLastStatus() {
  return props_;
}

void FakePowerManagerClient::RequestStatusUpdate() {
  // RequestStatusUpdate() calls and notifies the observers
  // asynchronously on a real device. On the fake implementation, we call
  // observers in a posted task to emulate the same behavior.
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(&FakePowerManagerClient::NotifyObservers,
                                weak_ptr_factory_.GetWeakPtr()));
}

void FakePowerManagerClient::RequestAllPeripheralBatteryUpdate() {}

void FakePowerManagerClient::RequestThermalState() {}

void FakePowerManagerClient::RequestSuspend(
    std::optional<uint64_t> wakeup_count,
    int32_t duration_secs,
    power_manager::RequestSuspendFlavor flavor) {
  ++num_request_suspend_calls_;
}

void FakePowerManagerClient::RequestRestart(
    power_manager::RequestRestartReason reason,
    const std::string& description) {
  ++num_request_restart_calls_;
  for (auto& observer : observers_)
    observer.RestartRequested(reason);
  if (restart_callback_)
    std::move(restart_callback_).Run();
}

void FakePowerManagerClient::RequestShutdown(
    power_manager::RequestShutdownReason reason,
    const std::string& description) {
  ++num_request_shutdown_calls_;
}

void FakePowerManagerClient::NotifyUserActivity(
    power_manager::UserActivityType type) {
  if (user_activity_callback_)
    user_activity_callback_.Run();
}

void FakePowerManagerClient::NotifyVideoActivity(bool is_fullscreen) {
  video_activity_reports_.push_back(is_fullscreen);
}

void FakePowerManagerClient::NotifyWakeNotification() {
  ++num_wake_notification_calls_;
}

void FakePowerManagerClient::SetPolicy(
    const power_manager::PowerManagementPolicy& policy) {
  policy_ = policy;
  ++num_set_policy_calls_;

  if (power_policy_quit_closure_)
    std::move(power_policy_quit_closure_).Run();
}

void FakePowerManagerClient::SetIsProjecting(bool is_projecting) {
  ++num_set_is_projecting_calls_;
  is_projecting_ = is_projecting;
}

void FakePowerManagerClient::SetPowerSource(const std::string& id) {
  props_->set_external_power_source_id(id);
  props_->set_external_power(
      power_manager::PowerSupplyProperties_ExternalPower_DISCONNECTED);
  for (const auto& source : props_->available_external_power_source()) {
    if (source.id() == id) {
      props_->set_external_power(
          !source.active_by_default() || source.max_power() < kUsbMinAcWatts
              ? power_manager::PowerSupplyProperties_ExternalPower_USB
              : power_manager::PowerSupplyProperties_ExternalPower_AC);
      break;
    }
  }

  NotifyObservers();
}

void FakePowerManagerClient::SetBacklightsForcedOff(bool forced_off) {
  backlights_forced_off_ = forced_off;
  ++num_set_backlights_forced_off_calls_;

  power_manager::BacklightBrightnessChange change;
  change.set_percent(forced_off ? 0 : requested_screen_brightness_percent_);
  change.set_cause(
      forced_off ? power_manager::BacklightBrightnessChange_Cause_FORCED_OFF
                 : power_manager::
                       BacklightBrightnessChange_Cause_NO_LONGER_FORCED_OFF);

  if (enqueue_brightness_changes_on_backlights_forced_off_) {
    pending_screen_brightness_changes_.push(change);
  } else {
    screen_brightness_percent_ = change.percent();
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce(&FakePowerManagerClient::SendScreenBrightnessChanged,
                       weak_ptr_factory_.GetWeakPtr(), change));
  }
}

void FakePowerManagerClient::GetBacklightsForcedOff(
    DBusMethodCallback<bool> callback) {
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), backlights_forced_off_));
}

void FakePowerManagerClient::GetBatterySaverModeState(
    DBusMethodCallback<power_manager::BatterySaverModeState> callback) {
  power_manager::BatterySaverModeState state;
  state.set_enabled(battery_saver_mode_enabled_);
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), state));
}

void FakePowerManagerClient::SetBatterySaverModeState(
    const power_manager::SetBatterySaverModeStateRequest& request) {
  bool changed = battery_saver_mode_enabled_ != request.enabled();
  if (!changed) {
    return;
  }

  battery_saver_mode_enabled_ = request.enabled();

  power_manager::BatterySaverModeState proto;
  proto.set_enabled(battery_saver_mode_enabled_);
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(&FakePowerManagerClient::SendBatterySaverModeStateChanged,
                     weak_ptr_factory_.GetWeakPtr(), proto));
}

void FakePowerManagerClient::GetSwitchStates(
    DBusMethodCallback<SwitchStates> callback) {
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback),
                                SwitchStates{lid_state_, tablet_mode_}));
}

void FakePowerManagerClient::GetInactivityDelays(
    DBusMethodCallback<power_manager::PowerManagementPolicy::Delays> callback) {
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), inactivity_delays_));
}

void FakePowerManagerClient::BlockSuspend(const base::UnguessableToken& token,
                                          const std::string& debug_info) {
  ++num_pending_suspend_readiness_callbacks_;
}

void FakePowerManagerClient::UnblockSuspend(
    const base::UnguessableToken& token) {
  CHECK_GT(num_pending_suspend_readiness_callbacks_, 0);

  --num_pending_suspend_readiness_callbacks_;
}

void FakePowerManagerClient::CreateArcTimers(
    const std::string& tag,
    std::vector<std::pair<clockid_t, base::ScopedFD>> arc_timer_requests,
    DBusMethodCallback<std::vector<TimerId>> callback) {
  // Return error if tag is empty.
  if (tag.empty()) {
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), std::vector<TimerId>()));
    return;
  }

  // Just like the real implementation, delete any old timers associated with
  // |tag|.
  DeleteArcTimersInternal(tag);

  // First, ensure that there are no duplicate clocks in the arguments. Return
  // error if there are.
  std::set<clockid_t> seen_clock_ids;
  for (const auto& request : arc_timer_requests) {
    if (!seen_clock_ids.emplace(request.first).second) {
      base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
          FROM_HERE,
          base::BindOnce(std::move(callback), std::vector<TimerId>()));
      return;
    }
  }

  // For each request, create a timer id and map the timer id to the expiration
  // fd that will be written to on timer expiry.
  std::vector<TimerId> timer_ids;
  for (auto& request : arc_timer_requests) {
    // Insert is safe as |next_timer_id_| is always incremented.
    arc_timers_.emplace(
        next_timer_id_,
        std::make_pair(new base::OneShotTimer(), std::move(request.second)));
    timer_ids.emplace_back(next_timer_id_);
    next_timer_id_++;
  }

  // Associate timer ids with the client's tag. The insert is safe because
  // duplicate client tags are checked for earlier.
  client_timer_ids_[tag] = timer_ids;
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), std::move(timer_ids)));
}

void FakePowerManagerClient::StartArcTimer(
    TimerId timer_id,
    base::TimeTicks absolute_expiration_time,
    VoidDBusMethodCallback callback) {
  if (simulate_start_arc_timer_failure_) {
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), false));
    return;
  }

  auto it = arc_timers_.find(timer_id);
  if (it == arc_timers_.end()) {
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), false));
    return;
  }

  // Post task to run |callback| and indicate success to the caller.
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), true));

  // Post task to write to |clock_id|'s expiration fd. This will simulate the
  // timer expiring to the caller. Ignore delaying by
  // |absolute_expiration_time| for test purposes.
  base::TimeTicks current_ticks = GetCurrentBootTime();
  base::TimeDelta task_delay;
  if (absolute_expiration_time > current_ticks)
    task_delay = absolute_expiration_time - current_ticks;
  auto& timer = it->second.first;
  int expiration_fd = it->second.second.get();

  auto timer_expiration_callback =
      base::BindOnce(&ArcTimerExpirationCallback, expiration_fd);
  // When an arc timer expires it is expected to send a `SuspendDone` dbus
  // signal which wakes the device up from suspend state. Simulate that by
  // calling a method that increases the count of wake notifications when the
  // timer expires.
  auto notify_wake_callback =
      base::BindOnce(&FakePowerManagerClient::NotifyWakeNotification,
                     weak_ptr_factory_.GetWeakPtr());
  auto combined_callback = std::move(timer_expiration_callback)
                               .Then(std::move(notify_wake_callback));
  timer->Start(FROM_HERE, task_delay, std::move(combined_callback));
}

void FakePowerManagerClient::DeleteArcTimers(const std::string& tag,
                                             VoidDBusMethodCallback callback) {
  DeleteArcTimersInternal(tag);
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), true));
}

base::TimeDelta FakePowerManagerClient::GetDarkSuspendDelayTimeout() {
  return kDarkSuspendDelayTimeout;
}

void FakePowerManagerClient::SetExternalDisplayALSBrightness(bool enabled) {
  external_display_als_brightness_enabled_ = enabled;
}

void FakePowerManagerClient::GetExternalDisplayALSBrightness(
    DBusMethodCallback<bool> callback) {
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback),
                                external_display_als_brightness_enabled_));
}

// The real implementation of ChargeNowForAdaptiveCharging is just a simple
// Dbus call without any callback, so there is not much to test for now.
void FakePowerManagerClient::ChargeNowForAdaptiveCharging() {}

void FakePowerManagerClient::GetChargeHistoryForAdaptiveCharging(
    DBusMethodCallback<power_manager::ChargeHistoryState> callback) {
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), charge_history_));
}

void FakePowerManagerClient::SetServiceAvailability(
    std::optional<bool> availability) {
  service_availability_ = availability;

  if (!service_availability_) {
    return;
  }

  for (auto& observer : observers_) {
    observer.PowerManagerBecameAvailable(service_availability_.value());
  }
}

bool FakePowerManagerClient::PopVideoActivityReport() {
  CHECK(!video_activity_reports_.empty());
  bool fullscreen = video_activity_reports_.front();
  video_activity_reports_.pop_front();
  return fullscreen;
}

void FakePowerManagerClient::SendBatterySaverModeStateChanged(
    const power_manager::BatterySaverModeState& proto) {
  for (auto& observer : observers_) {
    observer.BatterySaverModeStateChanged(proto);
  }
}

void FakePowerManagerClient::SendSuspendImminent(
    power_manager::SuspendImminent::Reason reason) {
  for (auto& observer : observers_)
    observer.SuspendImminent(reason);
  if (render_process_manager_delegate_)
    render_process_manager_delegate_->SuspendImminent();
}

void FakePowerManagerClient::SendSuspendDone(base::TimeDelta sleep_duration) {
  if (render_process_manager_delegate_)
    render_process_manager_delegate_->SuspendDone();

  for (auto& observer : observers_)
    observer.SuspendDone(sleep_duration);
}

void FakePowerManagerClient::SendDarkSuspendImminent() {
  for (auto& observer : observers_)
    observer.DarkSuspendImminent();
}

void FakePowerManagerClient::SendScreenBrightnessChanged(
    const power_manager::BacklightBrightnessChange& change) {
  for (auto& observer : observers_)
    observer.ScreenBrightnessChanged(change);
}

void FakePowerManagerClient::SendAmbientLightSensorEnabledChanged(
    const power_manager::AmbientLightSensorChange& proto) {
  for (auto& observer : observers_) {
    observer.AmbientLightSensorEnabledChanged(proto);
  }
}

void FakePowerManagerClient::SendKeyboardAmbientLightSensorEnabledChanged(
    const power_manager::AmbientLightSensorChange& proto) {
  for (auto& observer : observers_) {
    observer.KeyboardAmbientLightSensorEnabledChanged(proto);
  }
}

void FakePowerManagerClient::SendKeyboardBrightnessChanged(
    const power_manager::BacklightBrightnessChange& change) {
  for (auto& observer : observers_)
    observer.KeyboardBrightnessChanged(change);
}

void FakePowerManagerClient::SendScreenIdleStateChanged(
    const power_manager::ScreenIdleState& proto) {
  for (auto& observer : observers_)
    observer.ScreenIdleStateChanged(proto);
}

void FakePowerManagerClient::SendPowerButtonEvent(
    bool down,
    const base::TimeTicks& timestamp) {
  for (auto& observer : observers_)
    observer.PowerButtonEventReceived(down, timestamp);
}

void FakePowerManagerClient::SetLidState(LidState state,
                                         const base::TimeTicks& timestamp) {
  lid_state_ = state;
  for (auto& observer : observers_)
    observer.LidEventReceived(state, timestamp);
}

void FakePowerManagerClient::SetTabletMode(TabletMode mode,
                                           const base::TimeTicks& timestamp) {
  tablet_mode_ = mode;
  for (auto& observer : observers_)
    observer.TabletModeEventReceived(mode, timestamp);
}

void FakePowerManagerClient::SetInactivityDelays(
    const power_manager::PowerManagementPolicy::Delays& delays) {
  inactivity_delays_ = delays;
  for (auto& observer : observers_)
    observer.InactivityDelaysChanged(delays);
}

void FakePowerManagerClient::UpdatePowerProperties(
    std::optional<power_manager::PowerSupplyProperties> power_props) {
  props_ = power_props;
  // Only notify observer when power supply properties are available.
  if (props_.has_value()) {
    NotifyObservers();
  }
}

void FakePowerManagerClient::NotifyObservers() {
  for (auto& observer : observers_)
    observer.PowerChanged(*props_);
}

void FakePowerManagerClient::DeleteArcTimersInternal(const std::string& tag) {
  // Retrieve all timer ids associated with |tag|. Delete all timers associated
  // with these timer ids.
  auto it = client_timer_ids_.find(tag);
  if (it == client_timer_ids_.end())
    return;

  for (auto timer_id : it->second)
    arc_timers_.erase(timer_id);

  client_timer_ids_.erase(it);
}

void FakePowerManagerClient::SetPowerPolicyQuitClosure(
    base::OnceClosure quit_closure) {
  power_policy_quit_closure_ = std::move(quit_closure);
}

bool FakePowerManagerClient::ApplyPendingScreenBrightnessChange() {
  if (pending_screen_brightness_changes_.empty())
    return false;

  power_manager::BacklightBrightnessChange change =
      pending_screen_brightness_changes_.front();
  pending_screen_brightness_changes_.pop();

  screen_brightness_percent_ = change.percent();
  SendScreenBrightnessChanged(change);
  return true;
}

void FakePowerManagerClient::SetChargeHistoryForAdaptiveCharging(
    const power_manager::ChargeHistoryState& charge_history) {
  charge_history_ = charge_history;
}

// Returns time ticks from boot including time ticks spent during sleeping.
base::TimeTicks FakePowerManagerClient::GetCurrentBootTime() {
  if (tick_clock_)
    return tick_clock_->NowTicks();
  return base::TimeTicks() + ClockNow(CLOCK_BOOTTIME);
}

}  // namespace chromeos
