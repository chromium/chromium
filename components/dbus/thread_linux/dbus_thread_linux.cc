// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/dbus/thread_linux/dbus_thread_linux.h"

#include <utility>

#include "base/no_destructor.h"
#include "base/synchronization/lock.h"
#include "base/task/lazy_thread_pool_task_runner.h"
#include "base/task/single_thread_task_runner.h"
#include "dbus/bus.h"

namespace dbus_thread_linux {

namespace {

// Returns a global singleton reference to a session bus.
scoped_refptr<dbus::Bus>& GetSharedSessionBusRefPtrInstance() {
  // Store the bus in a lazy static variable instead of a global to avoid a
  // static initializer.
  static base::NoDestructor<scoped_refptr<dbus::Bus>> bus;
  return *bus;
}

// Returns a global singleton reference to a system bus.
scoped_refptr<dbus::Bus>& GetSharedSystemBusRefPtrInstance() {
  // Store the bus in a lazy static variable instead of a global to avoid a
  // static initializer.
  static base::NoDestructor<scoped_refptr<dbus::Bus>> bus;
  return *bus;
}

// Use TaskPriority::USER_BLOCKING, because there is a client
// (NotificationPlatformBridgeLinuxImpl) which needs to run user-blocking tasks
// on this thread. Use SingleThreadTaskRunnerThreadMode::SHARED, because DBus
// does not require an exclusive use of the thread, only the existence of a
// single thread for all tasks.
base::LazyThreadPoolSingleThreadTaskRunner g_dbus_thread_task_runner =
    LAZY_THREAD_POOL_SINGLE_THREAD_TASK_RUNNER_INITIALIZER(
        base::TaskTraits(base::MayBlock(), base::TaskPriority::USER_BLOCKING),
        base::SingleThreadTaskRunnerThreadMode::SHARED);

scoped_refptr<dbus::Bus> CreateSharedBus(dbus::Bus::BusType bus_type) {
  dbus::Bus::Options options;
  options.bus_type = bus_type;
  options.connection_type = dbus::Bus::PRIVATE;
  options.dbus_task_runner = g_dbus_thread_task_runner.Get();
  return base::MakeRefCounted<dbus::Bus>(std::move(options));
}

}  // namespace

scoped_refptr<dbus::Bus> GetSharedSessionBus() {
  static base::Lock lock;
  base::AutoLock guard(lock);
  auto& session_bus = GetSharedSessionBusRefPtrInstance();
  if (!session_bus) {
    session_bus = CreateSharedBus(dbus::Bus::SESSION);
  }
  return session_bus;
}

scoped_refptr<dbus::Bus> GetSharedSystemBus() {
  static base::Lock lock;
  base::AutoLock guard(lock);
  auto& system_bus = GetSharedSystemBusRefPtrInstance();
  if (!system_bus) {
    system_bus = CreateSharedBus(dbus::Bus::SYSTEM);
  }
  return system_bus;
}

void ShutdownOnDBusThreadAndBlock() {
  if (auto& session_bus = GetSharedSessionBusRefPtrInstance()) {
    session_bus->ShutdownOnDBusThreadAndBlock();
    session_bus = nullptr;
  }
  if (auto& system_bus = GetSharedSystemBusRefPtrInstance()) {
    system_bus->ShutdownOnDBusThreadAndBlock();
    system_bus = nullptr;
  }
}

}  // namespace dbus_thread_linux
