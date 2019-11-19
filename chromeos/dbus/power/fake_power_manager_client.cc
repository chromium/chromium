// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/dbus/power/fake_power_manager_client.h"

#include <set>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/callback.h"
#include "base/location.h"
#include "base/posix/unix_domain_socket.h"
#include "base/threading/thread_restrictions.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/time/time.h"
#include "base/timer/timer.h"

namespace chromeos {

namespace {

FakePowerManagerClient* g_instance = nullptr;

// Minimum power for a USB power source to be classified as AC.
constexpr double kUsbMinAcWatts = 24;

// The time power manager will wait before resuspending from a dark resume.
constexpr base::TimeDelta kDarkSuspendDelayTimeout =
    base::TimeDelta::FromSeconds(20);

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
  }
  NOTREACHED() << "Unhandled brightness request cause " << cause;
  return power_manager::BacklightBrightnessChange_Cause_USER_REQUEST;
}

// Copied from Chrome's //base/time/time_now_posix.cc.
// Returns count of |clk_id| in the form of a time delta. Returns an empty
// time delta if |clk_id| isn't present on the system.
base::TimeDelta ClockNow(clockid_t clk_id) {
  struct timespec ts;
  if (clock_gettime(clk_id, &ts) != 0) {
    NOTREACHED() << "clock_gettime(" << clk_id << ") failed.";
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
  observer->PowerManagerBecameAvailable(true);
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

void FakePowerManagerClient::DecreaseScreenBrightness(bool allow_off) {}

void FakePowerManagerClient::IncreaseScreenBrightness() {}

void FakePowerManagerClient::SetScreenBrightness(
    const power_manager::SetBacklightBrightnessRequest& request) {
  screen_brightness_percent_ = request.percent();
  requested_screen_brightness_percent_ = request.percent();

  power_manager::BacklightBrightnessChange change;
  change.set_percent(request.percent());
  change.set_cause(RequestCauseToChangeCause(request.cause()));
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::BindOnce(&FakePowerManagerClient::SendScreenBrightnessChanged,
                     weak_ptr_factory_.GetWeakPtr(), change));
}

void FakePowerManagerClient::GetScreenBrightnessPercent(
    DBusMethodCallback<double> callback) {
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::BindOnce(std::move(callback), screen_brightness_percent_));
}

void FakePowerManagerClient::DecreaseKeyboardBrightness() {}

void FakePowerManagerClient::IncreaseKeyboardBrightness() {}

void FakePowerManagerClient::GetKeyboardBrightnessPercent(
    DBusMethodCallback<double> callback) {
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::BindOnce(std::move(callback), keyboard_brightness_percent_));
}

const base::Optional<power_manager::PowerSupplyProperties>&
FakePowerManagerClient::GetLastStatus() {
  return props_;
}

void FakePowerManagerClient::RequestStatusUpdate() {
  // RequestStatusUpdate() calls and notifies the observers
  // asynchronously on a real device. On the fake implementation, we call
  // observers in a posted task to emulate the same behavior.
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(&FakePowerManagerClient::NotifyObservers,
                                weak_ptr_factory_.GetWeakPtr()));
}

void FakePowerManagerClient::RequestSuspend() {}

void FakePowerManagerClient::RequestRestart(
    power_manager::RequestRestartReason reason,
    const std::string& description) {
  ++num_request_restart_calls_;
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
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE,
        base::BindOnce(&FakePowerManagerClient::SendScreenBrightnessChanged,
                       weak_ptr_factory_.GetWeakPtr(), change));
  }
}

void FakePowerManagerClient::GetBacklightsForcedOff(
    DBusMethodCallback<bool> callback) {
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), backlights_forced_off_));
}

void FakePowerManagerClient::GetSwitchStates(
    DBusMethodCallback<SwitchStates> callback) {
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback),
                                SwitchStates{lid_state_, tablet_mode_}));
}

void FakePowerManagerClient::GetInactivityDelays(
    DBusMethodCallback<power_manager::PowerManagementPolicy::Delays> callback) {
  base::ThreadTaskRunnerHandle::Get()->PostTask(
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

bool FakePowerManagerClient::SupportsAmbientColor() {
  return supports_ambient_color_;
}

void FakePowerManagerClient::CreateArcTimers(
    const std::string& tag,
    std::vector<std::pair<clockid_t, base::ScopedFD>> arc_timer_requests,
    DBusMethodCallback<std::vector<TimerId>> callback) {
  // Return error if tag is empty.
  if (tag.empty()) {
    base::ThreadTaskRunnerHandle::Get()->PostTask(
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
      base::ThreadTaskRunnerHandle::Get()->PostTask(
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
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), std::move(timer_ids)));
}

void FakePowerManagerClient::StartArcTimer(
    TimerId timer_id,
    base::TimeTicks absolute_expiration_time,
    VoidDBusMethodCallback callback) {
  if (simulate_start_arc_timer_failure_) {
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), false));
    return;
  }

  auto it = arc_timers_.find(timer_id);
  if (it == arc_timers_.end()) {
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), false));
    return;
  }

  // Post task to run |callback| and indicate success to the caller.
  base::ThreadTaskRunnerHandle::Get()->PostTask(
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
  timer->Start(FROM_HERE, task_delay,
               base::BindOnce(&ArcTimerExpirationCallback, expiration_fd));
}

void FakePowerManagerClient::DeleteArcTimers(const std::string& tag,
                                             VoidDBusMethodCallback callback) {
  DeleteArcTimersInternal(tag);
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), true));
}

base::TimeDelta FakePowerManagerClient::GetDarkSuspendDelayTimeout() {
  return kDarkSuspendDelayTimeout;
}

bool FakePowerManagerClient::PopVideoActivityReport() {
  CHECK(!video_activity_reports_.empty());
  bool fullscreen = video_activity_reports_.front();
  video_activity_reports_.pop_front();
  return fullscreen;
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
    const power_manager::PowerSupplyProperties& power_props) {
  props_ = power_props;
  NotifyObservers();
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

// Returns time ticks from boot including time ticks spent during sleeping.
base::TimeTicks FakePowerManagerClient::GetCurrentBootTime() {
  if (tick_clock_)
    return tick_clock_->NowTicks();
  return base::TimeTicks() + ClockNow(CLOCK_BOOTTIME);
}

}  // namespace chromeos
