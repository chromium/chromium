// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_POWER_MONITOR_POWER_MONITOR_DEVICE_SOURCE_LINUX_H_
#define COMPONENTS_POWER_MONITOR_POWER_MONITOR_DEVICE_SOURCE_LINUX_H_

#include <string>

#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/power_monitor/power_monitor_source.h"
#include "components/dbus/utils/call_method.h"
#include "components/dbus/utils/connect_to_signal.h"

namespace dbus {
class Bus;
class ObjectProxy;
class Signal;
}  // namespace dbus

// A PowerMonitorSource that observes sleep/resume signals issued by systemd
// and power-saver status issued by XDG PowerProfileMonitor portal on Linux
// systems.
class PowerMonitorDeviceSourceLinux : public base::PowerMonitorSource {
 public:
  PowerMonitorDeviceSourceLinux();
  PowerMonitorDeviceSourceLinux(scoped_refptr<dbus::Bus> system_bus,
                                scoped_refptr<dbus::Bus> session_bus);
  PowerMonitorDeviceSourceLinux(const PowerMonitorDeviceSourceLinux&) = delete;
  PowerMonitorDeviceSourceLinux& operator=(
      const PowerMonitorDeviceSourceLinux&) = delete;
  ~PowerMonitorDeviceSourceLinux() override;

  // base::PowerMonitorSource:
  base::PowerStateObserver::BatteryPowerStatus GetBatteryPowerStatus()
      const override;

 private:
  void OnSignalConnected(const std::string& interface_name,
                         const std::string& signal_name,
                         bool connected);
  void OnPrepareForSleep(dbus::Signal* signal);
  void OnPortalRequested(uint32_t portal_version);
  void OnGetPowerSaverEnabled(dbus_utils::CallMethodResultSig<"v"> result);
  void OnPropertiesChanged(
      dbus_utils::ConnectToSignalResultSig<"sa{sv}as"> result);
  void SetPowerSaverEnabled(bool enabled);

  scoped_refptr<dbus::Bus> system_bus_;
  scoped_refptr<dbus::Bus> session_bus_;
  raw_ptr<dbus::ObjectProxy> portal_proxy_ = nullptr;
  bool power_saver_enabled_ = false;

  base::WeakPtrFactory<PowerMonitorDeviceSourceLinux> weak_ptr_factory_{this};
};

#endif  // COMPONENTS_POWER_MONITOR_POWER_MONITOR_DEVICE_SOURCE_LINUX_H_
