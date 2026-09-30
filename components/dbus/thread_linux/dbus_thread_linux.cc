// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/dbus/thread_linux/dbus_thread_linux.h"

#include <utility>

#include "base/check.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/logging.h"
#include "base/memory/weak_ptr.h"
#include "base/message_loop/message_pump_type.h"
#include "base/task/single_thread_task_runner.h"
#include "base/threading/thread.h"
#include "dbus/bus.h"

namespace dbus_thread_linux {

namespace {

// Owns the D-Bus thread and both shared buses. The thread needs an I/O pump
// (dbus::Bus watches its socket with base::FileDescriptorWatcher) and is never
// a ThreadPool runner, so a bus can never hold a freed per-test runner.
class DBusThreadLinux {
 public:
  DBusThreadLinux() : thread_("D-Bus thread") {
    CHECK(thread_.StartWithOptions(
        base::Thread::Options(base::MessagePumpType::IO, 0)));
    session_bus_ = CreateBus(dbus::Bus::SESSION);
    system_bus_ = CreateBus(dbus::Bus::SYSTEM);
  }

  DBusThreadLinux(const DBusThreadLinux&) = delete;
  DBusThreadLinux& operator=(const DBusThreadLinux&) = delete;

  ~DBusThreadLinux() {
    CHECK(!session_bus_ && !system_bus_) << "ShutdownBuses() must run first";
  }

  void set_disconnected_callback(base::RepeatingClosure callback) {
    disconnected_callback_ = std::move(callback);
  }

  scoped_refptr<dbus::Bus> session_bus() { return session_bus_; }
  scoped_refptr<dbus::Bus> system_bus() { return system_bus_; }

  // Blocks; must run on the buses' origin thread.
  void ShutdownBuses() {
    session_bus_->ShutdownOnDBusThreadAndBlock();
    session_bus_ = nullptr;
    system_bus_->ShutdownOnDBusThreadAndBlock();
    system_bus_ = nullptr;
  }

 private:
  scoped_refptr<dbus::Bus> CreateBus(dbus::Bus::BusType bus_type) {
    dbus::Bus::Options options;
    options.bus_type = bus_type;
    options.connection_type = dbus::Bus::PRIVATE;
    options.dbus_task_runner = thread_.task_runner();
    // The bus posts this to the origin thread, where Shutdown() may already
    // have deleted `this`.
    options.disconnected_callback = base::BindOnce(
        &DBusThreadLinux::OnDisconnected, weak_factory_.GetWeakPtr());
    return base::MakeRefCounted<dbus::Bus>(std::move(options));
  }

  void OnDisconnected() {
    if (!disconnected_callback_) {
      LOG(FATAL) << "D-Bus connection was disconnected. Aborting.";
    }
    disconnected_callback_.Run();
  }

  base::Thread thread_;
  scoped_refptr<dbus::Bus> session_bus_;
  scoped_refptr<dbus::Bus> system_bus_;
  base::RepeatingClosure disconnected_callback_;
  base::WeakPtrFactory<DBusThreadLinux> weak_factory_{this};
};

// Only touched on the thread that called Initialize().
DBusThreadLinux* g_instance = nullptr;

DBusThreadLinux* Instance() {
  if (!g_instance) {
    g_instance = new DBusThreadLinux();
  }
  return g_instance;
}

}  // namespace

void Initialize(base::RepeatingClosure disconnected_callback) {
  Instance()->set_disconnected_callback(std::move(disconnected_callback));
}

scoped_refptr<dbus::Bus> GetSharedSessionBus() {
  return Instance()->session_bus();
}

scoped_refptr<dbus::Bus> GetSharedSystemBus() {
  return Instance()->system_bus();
}

void Shutdown() {
  if (!g_instance) {
    return;
  }
  g_instance->ShutdownBuses();
  delete g_instance;
  g_instance = nullptr;
}

}  // namespace dbus_thread_linux
