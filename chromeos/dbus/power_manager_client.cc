// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/dbus/power_manager_client.h"

#include <stdint.h>

#include <algorithm>
#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/format_macros.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/observer_list.h"
#include "base/power_monitor/power_monitor_device_source.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/stringprintf.h"
#include "base/threading/platform_thread.h"
#include "base/timer/timer.h"
#include "chromeos/dbus/dbus_switches.h"
#include "chromeos/dbus/fake_power_manager_client.h"
#include "chromeos/dbus/power_manager/backlight.pb.h"
#include "chromeos/dbus/power_manager/idle.pb.h"
#include "chromeos/dbus/power_manager/input_event.pb.h"
#include "chromeos/dbus/power_manager/peripheral_battery_status.pb.h"
#include "chromeos/dbus/power_manager/switch_states.pb.h"
#include "components/device_event_log/device_event_log.h"
#include "dbus/bus.h"
#include "dbus/message.h"
#include "dbus/object_path.h"
#include "dbus/object_proxy.h"

namespace chromeos {

namespace {

// Maximum amount of time that the power manager will wait for Chrome to
// say that it's ready for the system to be suspended, in milliseconds.
const int kSuspendDelayTimeoutMs = 5000;

// Human-readable description of Chrome's suspend delay.
const char kSuspendDelayDescription[] = "chrome";

// Returns a modified version of |proto| where fields are consistent.
power_manager::PowerSupplyProperties SanitizePowerSupplyProperties(
    const power_manager::PowerSupplyProperties& proto) {
  power_manager::PowerSupplyProperties sanitized = proto;

  if (sanitized.battery_state() ==
      power_manager::PowerSupplyProperties_BatteryState_FULL) {
    sanitized.set_battery_percent(100.0);
  }

  if (!sanitized.is_calculating_battery_time()) {
    const bool on_line_power =
        sanitized.external_power() !=
        power_manager::PowerSupplyProperties_ExternalPower_DISCONNECTED;
    if ((on_line_power && sanitized.battery_time_to_full_sec() < 0) ||
        (!on_line_power && sanitized.battery_time_to_empty_sec() < 0)) {
      sanitized.set_is_calculating_battery_time(true);
    }
  }
  return sanitized;
}

// Converts a LidState value from a power_manager::SwitchStates proto to the
// corresponding PowerManagerClient::LidState value.
PowerManagerClient::LidState GetLidStateFromProtoEnum(
    power_manager::SwitchStates::LidState state) {
  switch (state) {
    case power_manager::SwitchStates_LidState_OPEN:
      return PowerManagerClient::LidState::OPEN;
    case power_manager::SwitchStates_LidState_CLOSED:
      return PowerManagerClient::LidState::CLOSED;
    case power_manager::SwitchStates_LidState_NOT_PRESENT:
      return PowerManagerClient::LidState::NOT_PRESENT;
  }
  NOTREACHED() << "Unhandled lid state " << state;
  return PowerManagerClient::LidState::NOT_PRESENT;
}

// Converts a TabletMode value from a power_manager::SwitchStates proto to the
// corresponding PowerManagerClient::TabletMode value.
PowerManagerClient::TabletMode GetTabletModeFromProtoEnum(
    power_manager::SwitchStates::TabletMode mode) {
  switch (mode) {
    case power_manager::SwitchStates_TabletMode_ON:
      return PowerManagerClient::TabletMode::ON;
    case power_manager::SwitchStates_TabletMode_OFF:
      return PowerManagerClient::TabletMode::OFF;
    case power_manager::SwitchStates_TabletMode_UNSUPPORTED:
      return PowerManagerClient::TabletMode::UNSUPPORTED;
  }
  NOTREACHED() << "Unhandled tablet mode " << mode;
  return PowerManagerClient::TabletMode::UNSUPPORTED;
}

// Callback for D-Bus call made in |CreateArcTimers|.
void OnCreateArcTimersDBusMethod(
    DBusMethodCallback<std::vector<PowerManagerClient::TimerId>> callback,
    dbus::Response* response) {
  if (response == nullptr) {
    std::move(callback).Run(base::nullopt);
    return;
  }

  dbus::MessageReader reader(response);
  dbus::MessageReader array_reader(nullptr);
  if (!reader.PopArray(&array_reader)) {
    POWER_LOG(ERROR) << "No timer ids returned";
    std::move(callback).Run(base::nullopt);
    return;
  }

  std::vector<PowerManagerClient::TimerId> timer_ids;
  while (array_reader.HasMoreData()) {
    int32_t timer_id;
    if (!array_reader.PopInt32(&timer_id)) {
      POWER_LOG(ERROR) << "Failed to pop timer id";
      std::move(callback).Run(base::nullopt);
      return;
    }
    timer_ids.push_back(timer_id);
  }
  std::move(callback).Run(std::move(timer_ids));
}

// Callback for D-Bus call made in |StartArcTimer| and |DeleteArcTimers|.
void OnVoidDBusMethod(VoidDBusMethodCallback callback,
                      dbus::Response* response) {
  std::move(callback).Run(response != nullptr);
}

}  // namespace

// The PowerManagerClient implementation used in production.
class PowerManagerClientImpl : public PowerManagerClient {
 public:
  PowerManagerClientImpl()
      : origin_thread_id_(base::PlatformThread::CurrentId()),
        weak_ptr_factory_(this) {}

  ~PowerManagerClientImpl() override {
    // Here we should unregister suspend notifications from powerd,
    // however:
    // - The lifetime of the PowerManagerClientImpl can extend past that of
    //   the objectproxy,
    // - power_manager can already detect that the client is gone and
    //   unregister our suspend delay.
  }

  // PowerManagerClient overrides:

  void AddObserver(Observer* observer) override {
    CHECK(observer);  // http://crbug.com/119976
    observers_.AddObserver(observer);
  }

  void RemoveObserver(Observer* observer) override {
    observers_.RemoveObserver(observer);
  }

  bool HasObserver(const Observer* observer) const override {
    return observers_.HasObserver(observer);
  }

  void WaitForServiceToBeAvailable(
      WaitForServiceToBeAvailableCallback callback) override {
    power_manager_proxy_->WaitForServiceToBeAvailable(std::move(callback));
  }

  void SetRenderProcessManagerDelegate(
      base::WeakPtr<RenderProcessManagerDelegate> delegate) override {
    DCHECK(!render_process_manager_delegate_)
        << "There can be only one! ...RenderProcessManagerDelegate";
    render_process_manager_delegate_ = delegate;
  }

  void DecreaseScreenBrightness(bool allow_off) override {
    dbus::MethodCall method_call(
        power_manager::kPowerManagerInterface,
        power_manager::kDecreaseScreenBrightnessMethod);
    dbus::MessageWriter writer(&method_call);
    writer.AppendBool(allow_off);
    power_manager_proxy_->CallMethod(&method_call,
                                     dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
                                     base::DoNothing());
  }

  void IncreaseScreenBrightness() override {
    SimpleMethodCallToPowerManager(
        power_manager::kIncreaseScreenBrightnessMethod);
  }

  void DecreaseKeyboardBrightness() override {
    SimpleMethodCallToPowerManager(
        power_manager::kDecreaseKeyboardBrightnessMethod);
  }

  void IncreaseKeyboardBrightness() override {
    SimpleMethodCallToPowerManager(
        power_manager::kIncreaseKeyboardBrightnessMethod);
  }

  const base::Optional<power_manager::PowerSupplyProperties>& GetLastStatus()
      override {
    return proto_;
  }

  void SetScreenBrightness(
      const power_manager::SetBacklightBrightnessRequest& request) override {
    dbus::MethodCall method_call(power_manager::kPowerManagerInterface,
                                 power_manager::kSetScreenBrightnessMethod);
    if (!dbus::MessageWriter(&method_call).AppendProtoAsArrayOfBytes(request)) {
      POWER_LOG(ERROR) << "Error serializing "
                       << power_manager::kSetScreenBrightnessMethod
                       << " request";
      return;
    }
    power_manager_proxy_->CallMethod(&method_call,
                                     dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
                                     base::DoNothing());
  }

  void GetScreenBrightnessPercent(
      DBusMethodCallback<double> callback) override {
    dbus::MethodCall method_call(
        power_manager::kPowerManagerInterface,
        power_manager::kGetScreenBrightnessPercentMethod);
    power_manager_proxy_->CallMethod(
        &method_call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
        base::BindOnce(
            &PowerManagerClientImpl::OnGetScreenOrKeyboardBrightnessPercent,
            weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
  }

  void GetKeyboardBrightnessPercent(
      DBusMethodCallback<double> callback) override {
    dbus::MethodCall method_call(
        power_manager::kPowerManagerInterface,
        power_manager::kGetKeyboardBrightnessPercentMethod);
    power_manager_proxy_->CallMethod(
        &method_call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
        base::BindOnce(
            &PowerManagerClientImpl::OnGetScreenOrKeyboardBrightnessPercent,
            weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
  }

  void RequestStatusUpdate() override {
    POWER_LOG(USER) << "RequestStatusUpdate";
    dbus::MethodCall method_call(
        power_manager::kPowerManagerInterface,
        power_manager::kGetPowerSupplyPropertiesMethod);
    power_manager_proxy_->CallMethod(
        &method_call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
        base::BindOnce(
            &PowerManagerClientImpl::OnGetPowerSupplyPropertiesMethod,
            weak_ptr_factory_.GetWeakPtr()));
  }

  void RequestSuspend() override {
    POWER_LOG(USER) << "RequestSuspend";
    SimpleMethodCallToPowerManager(power_manager::kRequestSuspendMethod);
  }

  void RequestRestart(power_manager::RequestRestartReason reason,
                      const std::string& description) override {
    POWER_LOG(USER) << "RequestRestart: " << reason << " (" << description
                    << ")";
    dbus::MethodCall method_call(power_manager::kPowerManagerInterface,
                                 power_manager::kRequestRestartMethod);
    dbus::MessageWriter writer(&method_call);
    writer.AppendInt32(reason);
    writer.AppendString(description);
    power_manager_proxy_->CallMethod(&method_call,
                                     dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
                                     base::DoNothing());
  }

  void RequestShutdown(power_manager::RequestShutdownReason reason,
                       const std::string& description) override {
    POWER_LOG(USER) << "RequestShutdown: " << reason << " (" << description
                    << ")";
    dbus::MethodCall method_call(power_manager::kPowerManagerInterface,
                                 power_manager::kRequestShutdownMethod);
    dbus::MessageWriter writer(&method_call);
    writer.AppendInt32(reason);
    writer.AppendString(description);
    power_manager_proxy_->CallMethod(&method_call,
                                     dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
                                     base::DoNothing());
  }

  void NotifyUserActivity(power_manager::UserActivityType type) override {
    dbus::MethodCall method_call(power_manager::kPowerManagerInterface,
                                 power_manager::kHandleUserActivityMethod);
    dbus::MessageWriter writer(&method_call);
    writer.AppendInt32(type);

    power_manager_proxy_->CallMethod(&method_call,
                                     dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
                                     base::DoNothing());
  }

  void NotifyVideoActivity(bool is_fullscreen) override {
    dbus::MethodCall method_call(power_manager::kPowerManagerInterface,
                                 power_manager::kHandleVideoActivityMethod);
    dbus::MessageWriter writer(&method_call);
    writer.AppendBool(is_fullscreen);

    power_manager_proxy_->CallMethod(&method_call,
                                     dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
                                     base::DoNothing());
  }

  void SetPolicy(const power_manager::PowerManagementPolicy& policy) override {
    POWER_LOG(USER) << "SetPolicy";
    dbus::MethodCall method_call(power_manager::kPowerManagerInterface,
                                 power_manager::kSetPolicyMethod);
    dbus::MessageWriter writer(&method_call);
    if (!writer.AppendProtoAsArrayOfBytes(policy)) {
      POWER_LOG(ERROR) << "Error calling " << power_manager::kSetPolicyMethod;
      return;
    }
    power_manager_proxy_->CallMethod(&method_call,
                                     dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
                                     base::DoNothing());
  }

  void SetIsProjecting(bool is_projecting) override {
    POWER_LOG(USER) << "SetIsProjecting";
    dbus::MethodCall method_call(power_manager::kPowerManagerInterface,
                                 power_manager::kSetIsProjectingMethod);
    dbus::MessageWriter writer(&method_call);
    writer.AppendBool(is_projecting);
    power_manager_proxy_->CallMethod(&method_call,
                                     dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
                                     base::DoNothing());
    last_is_projecting_ = is_projecting;
  }

  void SetPowerSource(const std::string& id) override {
    POWER_LOG(USER) << "SetPowerSource: " << id;
    dbus::MethodCall method_call(power_manager::kPowerManagerInterface,
                                 power_manager::kSetPowerSourceMethod);
    dbus::MessageWriter writer(&method_call);
    writer.AppendString(id);
    power_manager_proxy_->CallMethod(&method_call,
                                     dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
                                     base::DoNothing());
  }

  void SetBacklightsForcedOff(bool forced_off) override {
    dbus::MethodCall method_call(power_manager::kPowerManagerInterface,
                                 power_manager::kSetBacklightsForcedOffMethod);
    dbus::MessageWriter(&method_call).AppendBool(forced_off);
    power_manager_proxy_->CallMethod(&method_call,
                                     dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
                                     base::DoNothing());
  }

  void GetBacklightsForcedOff(DBusMethodCallback<bool> callback) override {
    dbus::MethodCall method_call(power_manager::kPowerManagerInterface,
                                 power_manager::kGetBacklightsForcedOffMethod);
    power_manager_proxy_->CallMethod(
        &method_call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
        base::BindOnce(&PowerManagerClientImpl::OnGetBacklightsForcedOff,
                       weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
  }

  void GetSwitchStates(DBusMethodCallback<SwitchStates> callback) override {
    dbus::MethodCall method_call(power_manager::kPowerManagerInterface,
                                 power_manager::kGetSwitchStatesMethod);
    power_manager_proxy_->CallMethod(
        &method_call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
        base::BindOnce(&PowerManagerClientImpl::OnGetSwitchStates,
                       weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
  }

  void GetInactivityDelays(
      DBusMethodCallback<power_manager::PowerManagementPolicy::Delays> callback)
      override {
    dbus::MethodCall method_call(power_manager::kPowerManagerInterface,
                                 power_manager::kGetInactivityDelaysMethod);
    power_manager_proxy_->CallMethod(
        &method_call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
        base::BindOnce(&PowerManagerClientImpl::OnGetInactivityDelays,
                       weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
  }

  base::Closure GetSuspendReadinessCallback(
      const base::Location& from_where) override {
    DCHECK(OnOriginThread());
    DCHECK(suspend_is_pending_);

    const int callback_id = next_suspend_readiness_callback_id_++;
    pending_suspend_readiness_callbacks_[callback_id] = from_where;
    return base::Bind(&PowerManagerClientImpl::HandleObserverSuspendReadiness,
                      weak_ptr_factory_.GetWeakPtr(), pending_suspend_id_,
                      suspending_from_dark_resume_, callback_id);
  }

  int GetNumPendingSuspendReadinessCallbacks() override {
    return pending_suspend_readiness_callbacks_.size();
  }

  void CreateArcTimers(
      const std::string& tag,
      std::vector<std::pair<clockid_t, base::ScopedFD>> arc_timer_requests,
      DBusMethodCallback<std::vector<TimerId>> callback) override {
    dbus::MethodCall method_call(power_manager::kPowerManagerInterface,
                                 power_manager::kCreateArcTimersMethod);

    // Write mojo arguments i.e. client's tag and array of {int, fd} as a D-Bus
    // message.
    dbus::MessageWriter writer(&method_call);
    writer.AppendString(tag);
    dbus::MessageWriter array_writer(nullptr);
    writer.OpenArray("(ih)", &array_writer);
    for (const auto& request : arc_timer_requests) {
      dbus::MessageWriter struct_writer(nullptr);
      array_writer.OpenStruct(&struct_writer);
      struct_writer.AppendInt32(static_cast<int32_t>(request.first));
      // This dups the file descriptor and the original one stored as
      // base::ScopedFD in |arc_timer_requests| will be closed when the function
      // ends and it goes out of scope.
      struct_writer.AppendFileDescriptor(request.second.get());
      array_writer.CloseContainer(&struct_writer);
    }
    writer.CloseContainer(&array_writer);

    power_manager_proxy_->CallMethod(
        &method_call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
        base::BindOnce(&OnCreateArcTimersDBusMethod, std::move(callback)));
  }

  void StartArcTimer(TimerId timer_id,
                     base::TimeTicks absolute_expiration_time,
                     VoidDBusMethodCallback callback) override {
    dbus::MethodCall method_call(power_manager::kPowerManagerInterface,
                                 power_manager::kStartArcTimerMethod);

    // Write clock id and 64-bit expiration time ticks value as a D-Bus message.
    dbus::MessageWriter writer(&method_call);
    writer.AppendInt32(timer_id);
    // The absolute ticks are still being sent because base::TimeTicks() returns
    // 0.
    writer.AppendInt64(
        (absolute_expiration_time - base::TimeTicks()).InMicroseconds());

    power_manager_proxy_->CallMethod(
        &method_call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
        base::BindOnce(&OnVoidDBusMethod, std::move(callback)));
  }

  void DeleteArcTimers(const std::string& tag,
                       VoidDBusMethodCallback callback) override {
    dbus::MethodCall method_call(power_manager::kPowerManagerInterface,
                                 power_manager::kDeleteArcTimersMethod);
    dbus::MessageWriter writer(&method_call);
    writer.AppendString(tag);
    power_manager_proxy_->CallMethod(
        &method_call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
        base::BindOnce(&OnVoidDBusMethod, std::move(callback)));
  }

  void DeferScreenDim() override {
    SimpleMethodCallToPowerManager(power_manager::kDeferScreenDimMethod);
  }

 protected:
  void Init(dbus::Bus* bus) override {
    power_manager_proxy_ = bus->GetObjectProxy(
        power_manager::kPowerManagerServiceName,
        dbus::ObjectPath(power_manager::kPowerManagerServicePath));

    power_manager_proxy_->SetNameOwnerChangedCallback(
        base::Bind(&PowerManagerClientImpl::NameOwnerChangedReceived,
                   weak_ptr_factory_.GetWeakPtr()));

    // Listen to D-Bus signals emitted by powerd.
    typedef void (PowerManagerClientImpl::*SignalMethod)(dbus::Signal*);
    const std::map<const char*, SignalMethod> kSignalMethods = {
        {power_manager::kScreenBrightnessChangedSignal,
         &PowerManagerClientImpl::ScreenBrightnessChangedReceived},
        {power_manager::kKeyboardBrightnessChangedSignal,
         &PowerManagerClientImpl::KeyboardBrightnessChangedReceived},
        {power_manager::kScreenIdleStateChangedSignal,
         &PowerManagerClientImpl::ScreenIdleStateChangedReceived},
        {power_manager::kInactivityDelaysChangedSignal,
         &PowerManagerClientImpl::InactivityDelaysChangedReceived},
        {power_manager::kPeripheralBatteryStatusSignal,
         &PowerManagerClientImpl::PeripheralBatteryStatusReceived},
        {power_manager::kPowerSupplyPollSignal,
         &PowerManagerClientImpl::PowerSupplyPollReceived},
        {power_manager::kInputEventSignal,
         &PowerManagerClientImpl::InputEventReceived},
        {power_manager::kSuspendImminentSignal,
         &PowerManagerClientImpl::SuspendImminentReceived},
        {power_manager::kSuspendDoneSignal,
         &PowerManagerClientImpl::SuspendDoneReceived},
        {power_manager::kDarkSuspendImminentSignal,
         &PowerManagerClientImpl::DarkSuspendImminentReceived},
        {power_manager::kScreenDimImminentSignal,
         &PowerManagerClientImpl::ScreenDimImminentReceived},
        {power_manager::kIdleActionImminentSignal,
         &PowerManagerClientImpl::IdleActionImminentReceived},
        {power_manager::kIdleActionDeferredSignal,
         &PowerManagerClientImpl::IdleActionDeferredReceived},
    };
    for (const auto& it : kSignalMethods) {
      power_manager_proxy_->ConnectToSignal(
          power_manager::kPowerManagerInterface, it.first,
          base::BindRepeating(it.second, weak_ptr_factory_.GetWeakPtr()),
          base::BindOnce(&PowerManagerClientImpl::SignalConnected,
                         weak_ptr_factory_.GetWeakPtr()));
    }

    RegisterSuspendDelays();
    RequestStatusUpdate();
  }

 private:
  // Returns true if the current thread is the origin thread.
  bool OnOriginThread() {
    return base::PlatformThread::CurrentId() == origin_thread_id_;
  }

  // Called when a dbus signal is initially connected.
  void SignalConnected(const std::string& interface_name,
                       const std::string& signal_name,
                       bool success) {
    if (!success)
      POWER_LOG(ERROR) << "Failed to connect to signal " << signal_name << ".";
  }

  // Makes a method call to power manager with no arguments and no response.
  void SimpleMethodCallToPowerManager(const std::string& method_name) {
    dbus::MethodCall method_call(power_manager::kPowerManagerInterface,
                                 method_name);
    power_manager_proxy_->CallMethod(&method_call,
                                     dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
                                     base::DoNothing());
  }

  void NameOwnerChangedReceived(const std::string& old_owner,
                                const std::string& new_owner) {
    POWER_LOG(EVENT) << "Power manager restarted. Old owner: "
                     << (old_owner.empty() ? "[none]" : old_owner.c_str())
                     << " New owner: "
                     << (new_owner.empty() ? "[none]" : new_owner.c_str());
    suspend_is_pending_ = false;
    pending_suspend_id_ = -1;
    suspending_from_dark_resume_ = false;
    if (!new_owner.empty()) {
      POWER_LOG(EVENT) << "Sending initial state to power manager";
      RegisterSuspendDelays();
      SetIsProjecting(last_is_projecting_);
      for (auto& observer : observers_)
        observer.PowerManagerRestarted();
    }
  }

  void ScreenBrightnessChangedReceived(dbus::Signal* signal) {
    dbus::MessageReader reader(signal);
    power_manager::BacklightBrightnessChange proto;
    if (!reader.PopArrayOfBytesAsProto(&proto)) {
      POWER_LOG(ERROR) << "Unable to decode protocol buffer from "
                       << power_manager::kScreenBrightnessChangedSignal
                       << " signal";
      return;
    }
    POWER_LOG(DEBUG) << "Screen brightness changed to " << proto.percent()
                     << ": cause " << proto.cause();
    for (auto& observer : observers_)
      observer.ScreenBrightnessChanged(proto);
  }

  void KeyboardBrightnessChangedReceived(dbus::Signal* signal) {
    dbus::MessageReader reader(signal);
    power_manager::BacklightBrightnessChange proto;
    if (!reader.PopArrayOfBytesAsProto(&proto)) {
      POWER_LOG(ERROR) << "Unable to decode protocol buffer from "
                       << power_manager::kKeyboardBrightnessChangedSignal
                       << " signal";
      return;
    }
    POWER_LOG(DEBUG) << "Keyboard brightness changed to " << proto.percent()
                     << ": cause " << proto.cause();
    for (auto& observer : observers_)
      observer.KeyboardBrightnessChanged(proto);
  }

  void ScreenIdleStateChangedReceived(dbus::Signal* signal) {
    dbus::MessageReader reader(signal);
    power_manager::ScreenIdleState proto;
    if (!reader.PopArrayOfBytesAsProto(&proto)) {
      POWER_LOG(ERROR) << "Unable to decode protocol buffer from "
                       << power_manager::kScreenIdleStateChangedSignal
                       << " signal";
      return;
    }
    for (auto& observer : observers_)
      observer.ScreenIdleStateChanged(proto);
  }

  void InactivityDelaysChangedReceived(dbus::Signal* signal) {
    dbus::MessageReader reader(signal);
    power_manager::PowerManagementPolicy::Delays proto;
    if (!reader.PopArrayOfBytesAsProto(&proto)) {
      POWER_LOG(ERROR) << "Unable to decode protocol buffer from "
                       << power_manager::kInactivityDelaysChangedSignal
                       << " signal";
      return;
    }
    for (auto& observer : observers_)
      observer.InactivityDelaysChanged(proto);
  }

  void PeripheralBatteryStatusReceived(dbus::Signal* signal) {
    dbus::MessageReader reader(signal);
    power_manager::PeripheralBatteryStatus protobuf_status;
    if (!reader.PopArrayOfBytesAsProto(&protobuf_status)) {
      POWER_LOG(ERROR) << "Unable to decode protocol buffer from "
                       << power_manager::kPeripheralBatteryStatusSignal
                       << " signal";
      return;
    }

    std::string path = protobuf_status.path();
    std::string name = protobuf_status.name();
    int level = protobuf_status.has_level() ? protobuf_status.level() : -1;

    POWER_LOG(DEBUG) << "Device battery status received " << level << " for "
                     << name << " at " << path;

    for (auto& observer : observers_)
      observer.PeripheralBatteryStatusReceived(path, name, level);
  }

  void PowerSupplyPollReceived(dbus::Signal* signal) {
    POWER_LOG(DEBUG) << "Received power supply poll signal.";
    dbus::MessageReader reader(signal);
    power_manager::PowerSupplyProperties protobuf;
    if (reader.PopArrayOfBytesAsProto(&protobuf)) {
      HandlePowerSupplyProperties(protobuf);
    } else {
      POWER_LOG(ERROR) << "Unable to decode "
                       << power_manager::kPowerSupplyPollSignal << " signal";
    }
  }

  void OnGetPowerSupplyPropertiesMethod(dbus::Response* response) {
    if (!response) {
      POWER_LOG(ERROR) << "Error calling "
                       << power_manager::kGetPowerSupplyPropertiesMethod;
      return;
    }

    dbus::MessageReader reader(response);
    power_manager::PowerSupplyProperties protobuf;
    if (reader.PopArrayOfBytesAsProto(&protobuf)) {
      HandlePowerSupplyProperties(protobuf);
    } else {
      POWER_LOG(ERROR) << "Unable to decode "
                       << power_manager::kGetPowerSupplyPropertiesMethod
                       << " response";
    }
  }

  void OnGetScreenOrKeyboardBrightnessPercent(
      DBusMethodCallback<double> callback,
      dbus::Response* response) {
    if (!response) {
      std::move(callback).Run(base::nullopt);
      return;
    }
    dbus::MessageReader reader(response);
    double percent = 0.0;
    if (!reader.PopDouble(&percent)) {
      POWER_LOG(ERROR) << "Error reading response from powerd: "
                       << response->ToString();
      std::move(callback).Run(base::nullopt);
      return;
    }
    std::move(callback).Run(percent);
  }

  void OnGetBacklightsForcedOff(DBusMethodCallback<bool> callback,
                                dbus::Response* response) {
    if (!response) {
      POWER_LOG(ERROR) << "Error calling "
                       << power_manager::kGetBacklightsForcedOffMethod;
      std::move(callback).Run(base::nullopt);
      return;
    }
    dbus::MessageReader reader(response);
    bool state = false;
    if (!reader.PopBool(&state)) {
      POWER_LOG(ERROR) << "Error reading response from powerd: "
                       << response->ToString();
      std::move(callback).Run(base::nullopt);
      return;
    }
    std::move(callback).Run(state);
  }

  void OnGetSwitchStates(DBusMethodCallback<SwitchStates> callback,
                         dbus::Response* response) {
    if (!response) {
      POWER_LOG(ERROR) << "Error calling "
                       << power_manager::kGetSwitchStatesMethod;
      std::move(callback).Run(base::nullopt);
      return;
    }

    dbus::MessageReader reader(response);
    power_manager::SwitchStates proto;
    if (!reader.PopArrayOfBytesAsProto(&proto)) {
      POWER_LOG(ERROR) << "Error parsing response from "
                       << power_manager::kGetSwitchStatesMethod;
      std::move(callback).Run(base::nullopt);
      return;
    }
    std::move(callback).Run(
        SwitchStates{GetLidStateFromProtoEnum(proto.lid_state()),
                     GetTabletModeFromProtoEnum(proto.tablet_mode())});
  }

  void OnGetInactivityDelays(
      DBusMethodCallback<power_manager::PowerManagementPolicy::Delays> callback,
      dbus::Response* response) {
    if (!response) {
      POWER_LOG(ERROR) << "Error calling "
                       << power_manager::kGetInactivityDelaysMethod;
      std::move(callback).Run(base::nullopt);
      return;
    }

    dbus::MessageReader reader(response);
    power_manager::PowerManagementPolicy::Delays proto;
    if (!reader.PopArrayOfBytesAsProto(&proto)) {
      POWER_LOG(ERROR) << "Error parsing response from "
                       << power_manager::kGetInactivityDelaysMethod;
      std::move(callback).Run(base::nullopt);
      return;
    }
    std::move(callback).Run(proto);
  }

  void HandlePowerSupplyProperties(
      const power_manager::PowerSupplyProperties& proto) {
    proto_ = SanitizePowerSupplyProperties(proto);
    for (auto& observer : observers_)
      observer.PowerChanged(*proto_);
    const bool on_battery =
        proto_->external_power() ==
        power_manager::PowerSupplyProperties_ExternalPower_DISCONNECTED;
    base::PowerMonitorDeviceSource::SetPowerSource(on_battery);
  }

  void HandleRegisterSuspendDelayReply(bool dark_suspend,
                                       const std::string& method_name,
                                       dbus::Response* response) {
    if (!response) {
      POWER_LOG(ERROR) << "Error calling " << method_name;
      return;
    }

    dbus::MessageReader reader(response);
    power_manager::RegisterSuspendDelayReply protobuf;
    if (!reader.PopArrayOfBytesAsProto(&protobuf)) {
      POWER_LOG(ERROR) << "Unable to parse reply from " << method_name;
      return;
    }

    if (dark_suspend) {
      dark_suspend_delay_id_ = protobuf.delay_id();
      has_dark_suspend_delay_id_ = true;
      POWER_LOG(EVENT) << "Registered dark suspend delay "
                       << dark_suspend_delay_id_;
    } else {
      suspend_delay_id_ = protobuf.delay_id();
      has_suspend_delay_id_ = true;
      POWER_LOG(EVENT) << "Registered suspend delay " << suspend_delay_id_;
    }
  }

  void SuspendImminentReceived(dbus::Signal* signal) {
    HandleSuspendImminent(false /* in_dark_resume */, signal);
  }

  void DarkSuspendImminentReceived(dbus::Signal* signal) {
    HandleSuspendImminent(true /* in_dark_resume */, signal);
  }

  void HandleSuspendImminent(bool in_dark_resume, dbus::Signal* signal) {
    std::string signal_name = signal->GetMember();
    if ((in_dark_resume && !has_dark_suspend_delay_id_) ||
        (!in_dark_resume && !has_suspend_delay_id_)) {
      POWER_LOG(ERROR) << "Received unrequested " << signal_name << " signal";
      return;
    }

    dbus::MessageReader reader(signal);
    power_manager::SuspendImminent proto;
    if (!reader.PopArrayOfBytesAsProto(&proto)) {
      POWER_LOG(ERROR) << "Unable to decode protocol buffer from "
                       << signal_name << " signal";
      return;
    }

    POWER_LOG(EVENT) << "Got " << signal_name
                     << " signal announcing suspend attempt "
                     << proto.suspend_id();

    // If a previous suspend is pending from the same state we are currently in
    // (fully powered on or in dark resume), then something's gone a little
    // wonky.
    if (suspend_is_pending_ && suspending_from_dark_resume_ == in_dark_resume) {
      POWER_LOG(ERROR) << "Got " << signal_name
                       << " signal about pending suspend attempt "
                       << proto.suspend_id()
                       << " while still waiting on attempt "
                       << pending_suspend_id_;
    }

    pending_suspend_id_ = proto.suspend_id();
    suspend_is_pending_ = true;
    suspending_from_dark_resume_ = in_dark_resume;
    pending_suspend_readiness_callbacks_.clear();

    // Record the fact that observers are being notified to ensure that we don't
    // report readiness prematurely if one of them calls
    // GetSuspendReadinessCallback() and then runs the callback synchonously
    // instead of asynchronously.
    notifying_observers_about_suspend_imminent_ = true;
    if (suspending_from_dark_resume_) {
      for (auto& observer : observers_)
        observer.DarkSuspendImminent();
    } else {
      for (auto& observer : observers_)
        observer.SuspendImminent(proto.reason());
    }
    notifying_observers_about_suspend_imminent_ = false;

    base::PowerMonitorDeviceSource::HandleSystemSuspending();
    MaybeReportSuspendReadiness();
  }

  void SuspendDoneReceived(dbus::Signal* signal) {
    dbus::MessageReader reader(signal);
    power_manager::SuspendDone proto;
    if (!reader.PopArrayOfBytesAsProto(&proto)) {
      POWER_LOG(ERROR) << "Unable to decode protocol buffer from "
                       << power_manager::kSuspendDoneSignal << " signal";
      return;
    }

    const base::TimeDelta duration =
        base::TimeDelta::FromInternalValue(proto.suspend_duration());
    POWER_LOG(EVENT) << "Got " << power_manager::kSuspendDoneSignal
                     << " signal:"
                     << " suspend_id=" << proto.suspend_id()
                     << " duration=" << duration.InSeconds() << " sec";

    // RenderProcessManagerDelegate is only notified that suspend is imminent
    // when readiness is being reported to powerd. If the suspend attempt was
    // cancelled before then, we shouldn't notify the delegate about completion.
    const bool cancelled_while_regular_suspend_pending =
        suspend_is_pending_ && !suspending_from_dark_resume_;
    if (render_process_manager_delegate_ &&
        !cancelled_while_regular_suspend_pending)
      render_process_manager_delegate_->SuspendDone();

    // powerd always pairs each SuspendImminent signal with SuspendDone before
    // starting the next suspend attempt, so we should no longer report
    // readiness for any in-progress suspend attempts.
    pending_suspend_id_ = -1;
    suspend_is_pending_ = false;
    suspending_from_dark_resume_ = false;

    // powerd gives clients a limited amount of time to report suspend
    // readiness. Log the stragglers within Chrome to aid in debugging.
    for (const auto it : pending_suspend_readiness_callbacks_) {
      LOG(WARNING) << "Didn't report suspend readiness due to "
                   << it.second.ToString();
    }
    pending_suspend_readiness_callbacks_.clear();

    for (auto& observer : observers_)
      observer.SuspendDone(duration);
    base::PowerMonitorDeviceSource::HandleSystemResumed();
  }

  void ScreenDimImminentReceived(dbus::Signal* signal) {
    for (auto& observer : observers_)
      observer.ScreenDimImminent();
  }

  void IdleActionImminentReceived(dbus::Signal* signal) {
    dbus::MessageReader reader(signal);
    power_manager::IdleActionImminent proto;
    if (!reader.PopArrayOfBytesAsProto(&proto)) {
      POWER_LOG(ERROR) << "Unable to decode protocol buffer from "
                       << power_manager::kIdleActionImminentSignal << " signal";
      return;
    }
    for (auto& observer : observers_) {
      observer.IdleActionImminent(
          base::TimeDelta::FromInternalValue(proto.time_until_idle_action()));
    }
  }

  void IdleActionDeferredReceived(dbus::Signal* signal) {
    for (auto& observer : observers_)
      observer.IdleActionDeferred();
  }

  void InputEventReceived(dbus::Signal* signal) {
    dbus::MessageReader reader(signal);
    power_manager::InputEvent proto;
    if (!reader.PopArrayOfBytesAsProto(&proto)) {
      POWER_LOG(ERROR) << "Unable to decode protocol buffer from "
                       << power_manager::kInputEventSignal << " signal";
      return;
    }

    base::TimeTicks timestamp =
        base::TimeTicks::FromInternalValue(proto.timestamp());
    POWER_LOG(USER) << "Got " << power_manager::kInputEventSignal << " signal:"
                    << " type=" << proto.type()
                    << " timestamp=" << proto.timestamp();
    switch (proto.type()) {
      case power_manager::InputEvent_Type_POWER_BUTTON_DOWN:
      case power_manager::InputEvent_Type_POWER_BUTTON_UP: {
        const bool down =
            (proto.type() == power_manager::InputEvent_Type_POWER_BUTTON_DOWN);
        for (auto& observer : observers_)
          observer.PowerButtonEventReceived(down, timestamp);

        // Tell powerd that Chrome has handled power button presses.
        if (down) {
          dbus::MethodCall method_call(
              power_manager::kPowerManagerInterface,
              power_manager::kHandlePowerButtonAcknowledgmentMethod);
          dbus::MessageWriter writer(&method_call);
          writer.AppendInt64(proto.timestamp());
          power_manager_proxy_->CallMethod(
              &method_call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
              base::DoNothing());
        }
        break;
      }
      case power_manager::InputEvent_Type_LID_OPEN:
        for (auto& observer : observers_)
          observer.LidEventReceived(LidState::OPEN, timestamp);
        break;
      case power_manager::InputEvent_Type_LID_CLOSED:
        for (auto& observer : observers_)
          observer.LidEventReceived(LidState::CLOSED, timestamp);
        break;
      case power_manager::InputEvent_Type_TABLET_MODE_ON:
        for (auto& observer : observers_)
          observer.TabletModeEventReceived(TabletMode::ON, timestamp);
        break;
      case power_manager::InputEvent_Type_TABLET_MODE_OFF:
        for (auto& observer : observers_)
          observer.TabletModeEventReceived(TabletMode::OFF, timestamp);
        break;
    }
  }

  void RegisterSuspendDelayImpl(
      const std::string& method_name,
      const power_manager::RegisterSuspendDelayRequest& protobuf_request,
      dbus::ObjectProxy::ResponseCallback callback) {
    dbus::MethodCall method_call(power_manager::kPowerManagerInterface,
                                 method_name);
    dbus::MessageWriter writer(&method_call);

    if (!writer.AppendProtoAsArrayOfBytes(protobuf_request)) {
      POWER_LOG(ERROR) << "Error constructing message for " << method_name;
      return;
    }

    power_manager_proxy_->CallMethod(&method_call,
                                     dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
                                     std::move(callback));
  }

  // Registers suspend delays with the power manager.  This is usually only
  // called at startup, but if the power manager restarts, we need to create new
  // delays.
  void RegisterSuspendDelays() {
    // Throw out any old delay that was registered.
    suspend_delay_id_ = -1;
    has_suspend_delay_id_ = false;
    dark_suspend_delay_id_ = -1;
    has_dark_suspend_delay_id_ = false;

    power_manager::RegisterSuspendDelayRequest protobuf_request;
    base::TimeDelta timeout =
        base::TimeDelta::FromMilliseconds(kSuspendDelayTimeoutMs);
    protobuf_request.set_timeout(timeout.ToInternalValue());
    protobuf_request.set_description(kSuspendDelayDescription);

    RegisterSuspendDelayImpl(
        power_manager::kRegisterSuspendDelayMethod, protobuf_request,
        base::BindOnce(&PowerManagerClientImpl::HandleRegisterSuspendDelayReply,
                       weak_ptr_factory_.GetWeakPtr(), false,
                       power_manager::kRegisterSuspendDelayMethod));
    RegisterSuspendDelayImpl(
        power_manager::kRegisterDarkSuspendDelayMethod, protobuf_request,
        base::BindOnce(&PowerManagerClientImpl::HandleRegisterSuspendDelayReply,
                       weak_ptr_factory_.GetWeakPtr(), true,
                       power_manager::kRegisterDarkSuspendDelayMethod));
  }

  // Records the fact that an observer has finished doing asynchronous work
  // that was blocking a pending suspend attempt and possibly reports
  // suspend readiness to powerd.  Called by callbacks returned via
  // GetSuspendReadinessCallback().
  void HandleObserverSuspendReadiness(int32_t suspend_id,
                                      bool in_dark_resume,
                                      int callback_id) {
    DCHECK(OnOriginThread());
    if (!suspend_is_pending_ || suspend_id != pending_suspend_id_ ||
        in_dark_resume != suspending_from_dark_resume_)
      return;

    pending_suspend_readiness_callbacks_.erase(callback_id);
    MaybeReportSuspendReadiness();
  }

  // Reports suspend readiness to powerd if no observers are still holding
  // suspend readiness callbacks.
  void MaybeReportSuspendReadiness() {
    CHECK(suspend_is_pending_);

    // Avoid reporting suspend readiness if some observers have yet to be
    // notified about the pending attempt.
    if (notifying_observers_about_suspend_imminent_)
      return;

    if (GetNumPendingSuspendReadinessCallbacks() > 0)
      return;

    std::string method_name;
    int32_t delay_id = -1;
    if (suspending_from_dark_resume_) {
      method_name = power_manager::kHandleDarkSuspendReadinessMethod;
      delay_id = dark_suspend_delay_id_;
    } else {
      method_name = power_manager::kHandleSuspendReadinessMethod;
      delay_id = suspend_delay_id_;
    }

    if (render_process_manager_delegate_ && !suspending_from_dark_resume_)
      render_process_manager_delegate_->SuspendImminent();

    dbus::MethodCall method_call(power_manager::kPowerManagerInterface,
                                 method_name);
    dbus::MessageWriter writer(&method_call);

    POWER_LOG(EVENT) << "Announcing readiness of suspend delay " << delay_id
                     << " for suspend attempt " << pending_suspend_id_;
    power_manager::SuspendReadinessInfo protobuf_request;
    protobuf_request.set_delay_id(delay_id);
    protobuf_request.set_suspend_id(pending_suspend_id_);

    pending_suspend_id_ = -1;
    suspend_is_pending_ = false;

    if (!writer.AppendProtoAsArrayOfBytes(protobuf_request)) {
      POWER_LOG(ERROR) << "Error constructing message for " << method_name;
      return;
    }
    power_manager_proxy_->CallMethod(&method_call,
                                     dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
                                     base::DoNothing());
  }

  // Origin thread (i.e. the UI thread in production).
  base::PlatformThreadId origin_thread_id_;

  dbus::ObjectProxy* power_manager_proxy_ = nullptr;
  base::ObserverList<Observer>::Unchecked observers_;

  // The delay ID obtained from the RegisterSuspendDelay request.
  int32_t suspend_delay_id_ = -1;
  bool has_suspend_delay_id_ = false;

  // The delay ID obtained from the RegisterDarkSuspendDelay request.
  int32_t dark_suspend_delay_id_ = -1;
  bool has_dark_suspend_delay_id_ = false;

  // powerd-supplied ID corresponding to an imminent (either regular or dark)
  // suspend attempt that is currently being delayed.
  int32_t pending_suspend_id_ = -1;
  bool suspend_is_pending_ = false;

  // Set to true when the suspend currently being delayed was triggered during a
  // dark resume.  Since |pending_suspend_id_| and |suspend_is_pending_| are
  // both shared by normal and dark suspends, |suspending_from_dark_resume_|
  // helps distinguish the context within which these variables are being used.
  bool suspending_from_dark_resume_ = false;

  // Next ID to be assigned to a callback returned via
  // GetSuspendReadinessCallback().
  int next_suspend_readiness_callback_id_ = 1;

  // Map from suspend readiness callback ID to the location of the code that
  // requested the callback.
  std::map<int, base::Location> pending_suspend_readiness_callbacks_;

  // Inspected by MaybeReportSuspendReadiness() to avoid prematurely notifying
  // powerd about suspend readiness while |observers_|' SuspendImminent()
  // methods are being called by HandleSuspendImminent().
  bool notifying_observers_about_suspend_imminent_ = false;

  // Last state passed to SetIsProjecting().
  bool last_is_projecting_ = false;

  // The last proto received from D-Bus; initially empty.
  base::Optional<power_manager::PowerSupplyProperties> proto_;

  // The delegate used to manage the power consumption of Chrome's renderer
  // processes.
  base::WeakPtr<RenderProcessManagerDelegate> render_process_manager_delegate_;

  // Note: This should remain the last member so it'll be destroyed and
  // invalidate its weak pointers before any other members are destroyed.
  base::WeakPtrFactory<PowerManagerClientImpl> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(PowerManagerClientImpl);
};

PowerManagerClient::PowerManagerClient() = default;

PowerManagerClient::~PowerManagerClient() = default;

// static
PowerManagerClient* PowerManagerClient::Create(
    DBusClientImplementationType type) {
  if (type == REAL_DBUS_CLIENT_IMPLEMENTATION)
    return new PowerManagerClientImpl();
  DCHECK_EQ(FAKE_DBUS_CLIENT_IMPLEMENTATION, type);
  return new FakePowerManagerClient();
}

}  // namespace chromeos
