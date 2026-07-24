// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/power_monitor/power_monitor_device_source_linux.h"

#include <utility>

#include "base/check.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/memory/scoped_refptr.h"
#include "components/dbus/thread_linux/dbus_thread_linux.h"
#include "components/dbus/xdg/portal.h"
#include "dbus/bus.h"
#include "dbus/message.h"
#include "dbus/object_path.h"
#include "dbus/object_proxy.h"

namespace {

constexpr char kPortalServiceName[] = "org.freedesktop.portal.Desktop";
constexpr char kPortalObjectPath[] = "/org/freedesktop/portal/desktop";
constexpr char kPortalPowerProfileMonitorInterface[] =
    "org.freedesktop.portal.PowerProfileMonitor";
constexpr char kPowerSaverEnabledProperty[] = "power-saver-enabled";

constexpr char kDBusPropertiesInterface[] = "org.freedesktop.DBus.Properties";
constexpr char kDBusPropertiesGet[] = "Get";
constexpr char kPortalPropertiesChangedSignal[] = "PropertiesChanged";

}  // namespace

PowerMonitorDeviceSourceLinux::PowerMonitorDeviceSourceLinux()
    : PowerMonitorDeviceSourceLinux(dbus_thread_linux::GetSharedSystemBus(),
                                    dbus_thread_linux::GetSharedSessionBus()) {}

PowerMonitorDeviceSourceLinux::PowerMonitorDeviceSourceLinux(
    scoped_refptr<dbus::Bus> system_bus,
    scoped_refptr<dbus::Bus> session_bus)
    : system_bus_(std::move(system_bus)), session_bus_(std::move(session_bus)) {
  CHECK(system_bus_);
  CHECK(session_bus_);

  system_bus_
      ->GetObjectProxy("org.freedesktop.login1",
                       dbus::ObjectPath("/org/freedesktop/login1"))
      ->ConnectToSignal(
          "org.freedesktop.login1.Manager", "PrepareForSleep",
          base::BindRepeating(&PowerMonitorDeviceSourceLinux::OnPrepareForSleep,
                              weak_ptr_factory_.GetWeakPtr()),
          base::BindOnce(&PowerMonitorDeviceSourceLinux::OnSignalConnected,
                         weak_ptr_factory_.GetWeakPtr()));

  dbus_xdg::RequestXdgDesktopPortal(
      session_bus_.get(),
      base::BindOnce(&PowerMonitorDeviceSourceLinux::OnPortalRequested,
                     weak_ptr_factory_.GetWeakPtr()));
}

PowerMonitorDeviceSourceLinux::~PowerMonitorDeviceSourceLinux() = default;

base::PowerStateObserver::BatteryPowerStatus
PowerMonitorDeviceSourceLinux::GetBatteryPowerStatus() const {
  if (power_saver_enabled_) {
    return base::PowerStateObserver::BatteryPowerStatus::kBatteryPower;
  }
  // TODO(crbug.com/40836663): Use org.freedesktop.UPower to check for
  // OnBattery. One possibility is to connect to the DeviceService's
  // BatteryMonitor.
  return base::PowerStateObserver::BatteryPowerStatus::kUnknown;
}

void PowerMonitorDeviceSourceLinux::OnPortalRequested(uint32_t portal_version) {
  if (portal_version == 0) {
    return;
  }

  portal_proxy_ = session_bus_->GetObjectProxy(
      kPortalServiceName, dbus::ObjectPath(kPortalObjectPath));

  dbus_utils::ConnectToSignal<"sa{sv}as">(
      portal_proxy_, kDBusPropertiesInterface, kPortalPropertiesChangedSignal,
      base::BindRepeating(&PowerMonitorDeviceSourceLinux::OnPropertiesChanged,
                          weak_ptr_factory_.GetWeakPtr()),
      base::BindOnce(&PowerMonitorDeviceSourceLinux::OnSignalConnected,
                     weak_ptr_factory_.GetWeakPtr()));

  dbus_utils::CallMethod<"ss", "v">(
      portal_proxy_, kDBusPropertiesInterface, kDBusPropertiesGet,
      base::BindOnce(&PowerMonitorDeviceSourceLinux::OnGetPowerSaverEnabled,
                     weak_ptr_factory_.GetWeakPtr()),
      kPortalPowerProfileMonitorInterface, kPowerSaverEnabledProperty);
}

void PowerMonitorDeviceSourceLinux::OnSignalConnected(
    const std::string& interface_name,
    const std::string& signal_name,
    bool connected) {
  if (connected) {
    return;
  }

  DLOG(ERROR) << "Failed to connect to " << interface_name << " for signal "
              << signal_name;
}

void PowerMonitorDeviceSourceLinux::OnPrepareForSleep(dbus::Signal* signal) {
  dbus::MessageReader reader(signal);
  if (bool start = false; !reader.PopBool(&start) || reader.HasMoreData()) {
    DLOG(ERROR) << "Received malformed PrepareForSleep signal from systemd";
  } else if (start) {
    ProcessPowerEvent(SUSPEND_EVENT);
  } else {
    ProcessPowerEvent(RESUME_EVENT);
  }
}

void PowerMonitorDeviceSourceLinux::OnGetPowerSaverEnabled(
    dbus_utils::CallMethodResultSig<"v"> result) {
  if (!result.has_value()) {
    DLOG(WARNING) << "Failed to get power-saver-enabled property";
    return;
  }

  auto [variant] = std::move(*result);
  std::optional<bool> power_saver = std::move(variant).Take<bool>();
  if (power_saver.has_value()) {
    SetPowerSaverEnabled(*power_saver);
  }
}

void PowerMonitorDeviceSourceLinux::OnPropertiesChanged(
    dbus_utils::ConnectToSignalResultSig<"sa{sv}as"> result) {
  if (!result.has_value()) {
    return;
  }

  auto [interface_name, changed_properties, invalidated_properties] =
      std::move(*result);
  if (interface_name != kPortalPowerProfileMonitorInterface) {
    return;
  }

  auto it = changed_properties.find(kPowerSaverEnabledProperty);
  if (it != changed_properties.end()) {
    std::optional<bool> power_saver = std::move(it->second).Take<bool>();
    if (power_saver.has_value()) {
      SetPowerSaverEnabled(*power_saver);
    }
  }
}

void PowerMonitorDeviceSourceLinux::SetPowerSaverEnabled(bool enabled) {
  if (power_saver_enabled_ == enabled) {
    return;
  }
  power_saver_enabled_ = enabled;
  ProcessPowerEvent(POWER_STATE_EVENT);
}
