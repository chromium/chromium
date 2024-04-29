// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/dbus/init/dbus_thread_manager_base.h"

#include "base/check.h"
#include "base/command_line.h"
#include "base/system/sys_info.h"
#include "base/threading/thread.h"
#include "chromeos/dbus/constants/dbus_switches.h"
#include "dbus/bus.h"
#include "dbus/dbus_statistics.h"

namespace chromeos {

namespace {

// Returns whether to use a real D-Bus client or a stub.
bool GetUseRealClients() {
#if defined(USE_REAL_DBUS_CLIENTS)
  return true;
#else
  // TODO(crbug.com/41452889): Always use fakes after adding
  // use_real_dbus_clients=true to where needed.
  return (base::SysInfo::IsRunningOnChromeOS() &&
          !base::CommandLine::ForCurrentProcess()->HasSwitch(
              chromeos::switches::kDbusStub)) ||
         base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
             chromeos::switches::kDbusStub) == "never";
#endif
}

}  // namespace

DBusThreadManagerBase::DBusThreadManagerBase()
    : use_real_clients_(GetUseRealClients()) {
  dbus::statistics::Initialize();

  if (use_real_clients_) {
    // Create the D-Bus thread.
    base::Thread::Options thread_options;
    thread_options.message_pump_type = base::MessagePumpType::IO;
    dbus_thread_ = std::make_unique<base::Thread>("D-Bus thread");
    dbus_thread_->StartWithOptions(std::move(thread_options));

    // Create the connection to the system bus.
    dbus::Bus::Options system_bus_options;
    system_bus_options.bus_type = dbus::Bus::SYSTEM;
    system_bus_options.connection_type = dbus::Bus::PRIVATE;
    system_bus_options.dbus_task_runner = dbus_thread_->task_runner();
    system_bus_ = new dbus::Bus(system_bus_options);
  }
}

DBusThreadManagerBase::~DBusThreadManagerBase() {
  // Shut down the bus. During the browser shutdown, it's ok to shut down
  // the bus synchronously.
  if (system_bus_.get())
    system_bus_->ShutdownOnDBusThreadAndBlock();

  // Stop the D-Bus thread.
  if (dbus_thread_)
    dbus_thread_->Stop();

  dbus::statistics::Shutdown();
}

bool DBusThreadManagerBase::IsUsingFakes() {
  return !use_real_clients_;
}

dbus::Bus* DBusThreadManagerBase::GetSystemBus() {
  return system_bus_.get();
}

}  // namespace chromeos
