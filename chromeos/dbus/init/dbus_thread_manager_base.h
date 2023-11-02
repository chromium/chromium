// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_DBUS_INIT_DBUS_THREAD_MANAGER_BASE_H_
#define CHROMEOS_DBUS_INIT_DBUS_THREAD_MANAGER_BASE_H_

#include <memory>

#include "base/component_export.h"
#include "base/memory/ref_counted.h"

namespace base {
class Thread;
}  // namespace base

namespace dbus {
class Bus;
}  // namespace dbus

namespace chromeos {

// DBusThreadManagerBase manages the D-Bus thread (the thread dedicated to
// handling asynchronous D-Bus operations) and the D-Bus connection.

// Derived classes and callers are responsible for managing D-Bus clients (which
// depend on the D-Bus thread), and need to ensure that D-Bus clients shut down
// before the D-Bus connection closes and the D-Bus thread stops in
// ~DBusThreadManagerBase().
class COMPONENT_EXPORT(CHROMEOS_DBUS_INIT) DBusThreadManagerBase {
 public:
  // Returns true if clients are faked.
  bool IsUsingFakes();

  // Returns various D-Bus bus instances, owned by DBusThreadManager.
  dbus::Bus* GetSystemBus();

 protected:
  DBusThreadManagerBase();
  DBusThreadManagerBase(const DBusThreadManagerBase&) = delete;
  const DBusThreadManagerBase& operator=(const DBusThreadManagerBase&) = delete;
  virtual ~DBusThreadManagerBase();

 private:
  // Whether to use real or fake dbus clients.
  const bool use_real_clients_;

  std::unique_ptr<base::Thread> dbus_thread_;
  scoped_refptr<dbus::Bus> system_bus_;
};

}  // namespace chromeos

#endif  // CHROMEOS_DBUS_INIT_DBUS_THREAD_MANAGER_BASE_H_
