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
#include "dbus/bus.h"
#include "dbus/message.h"
#include "dbus/object_path.h"
#include "dbus/object_proxy.h"

namespace {

scoped_refptr<dbus::Bus> CreateBus() {
  dbus::Bus::Options options;
  options.bus_type = dbus::Bus::SYSTEM;
  options.connection_type = dbus::Bus::PRIVATE;
  options.dbus_task_runner = dbus_thread_linux::GetTaskRunner();
  return base::MakeRefCounted<dbus::Bus>(options);
}

}  // namespace

PowerMonitorDeviceSourceLinux::PowerMonitorDeviceSourceLinux()
    : bus_(CreateBus()) {
  bus_->GetObjectProxy("org.freedesktop.login1",
                       dbus::ObjectPath("/org/freedesktop/login1"))
      ->ConnectToSignal(
          "org.freedesktop.login1.Manager", "PrepareForSleep",
          base::BindRepeating(&PowerMonitorDeviceSourceLinux::OnPrepareForSleep,
                              weak_ptr_factory_.GetWeakPtr()),
          base::BindOnce(&PowerMonitorDeviceSourceLinux::OnSignalConnected,
                         weak_ptr_factory_.GetWeakPtr()));
}

PowerMonitorDeviceSourceLinux::~PowerMonitorDeviceSourceLinux() {
  if (bus_)
    ShutdownBus();
}

base::PowerStateObserver::BatteryPowerStatus
PowerMonitorDeviceSourceLinux::GetBatteryPowerStatus() const {
  // TODO(crbug.com/40836663): Use org.freedesktop.UPower to check for
  // OnBattery. One possibility is to connect to the DeviceService's
  // BatteryMonitor.
  return base::PowerStateObserver::BatteryPowerStatus::kUnknown;
}

void PowerMonitorDeviceSourceLinux::ShutdownBus() {
  DCHECK(bus_);
  dbus::Bus* const bus_ptr = bus_.get();
  bus_ptr->GetDBusTaskRunner()->PostTask(
      FROM_HERE, base::BindOnce(&dbus::Bus::ShutdownAndBlock, std::move(bus_)));
}

void PowerMonitorDeviceSourceLinux::OnSignalConnected(
    const std::string& interface_name,
    const std::string& signal_name,
    bool connected) {
  if (connected)
    return;

  DLOG(ERROR) << "Failed to connect to " << interface_name << " for signal "
              << signal_name;
  if (bus_)
    ShutdownBus();
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
