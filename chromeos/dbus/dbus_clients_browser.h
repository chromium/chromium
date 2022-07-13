// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_DBUS_DBUS_CLIENTS_BROWSER_H_
#define CHROMEOS_DBUS_DBUS_CLIENTS_BROWSER_H_

#include <memory>

#include "base/component_export.h"

namespace dbus {
class Bus;
}

namespace chromeos {

class CrosDisksClient;
class DebugDaemonClient;
class EasyUnlockClient;
class FwupdClient;

// Owns D-Bus clients.
// TODO(jamescook): Rename this class. "Browser" refers to the browser process
// versus ash process distinction from the mustash project, which was cancelled
// in 2019.
class COMPONENT_EXPORT(CHROMEOS_DBUS) DBusClientsBrowser {
 public:
  // Creates real implementations if |use_real_clients| is true and fakes
  // otherwise. Fakes are used when running on Linux desktop and in tests.
  explicit DBusClientsBrowser(bool use_real_clients);

  DBusClientsBrowser(const DBusClientsBrowser&) = delete;
  DBusClientsBrowser& operator=(const DBusClientsBrowser&) = delete;

  ~DBusClientsBrowser();

  void Initialize(dbus::Bus* system_bus);

 private:
  friend class DBusThreadManager;
  friend class DBusThreadManagerSetter;

  std::unique_ptr<CrosDisksClient> cros_disks_client_;
  std::unique_ptr<DebugDaemonClient> debug_daemon_client_;
  std::unique_ptr<EasyUnlockClient> easy_unlock_client_;
  std::unique_ptr<FwupdClient> fwupd_client_;
};

}  // namespace chromeos

#endif  // CHROMEOS_DBUS_DBUS_CLIENTS_BROWSER_H_
