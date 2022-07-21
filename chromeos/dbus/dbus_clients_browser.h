// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_DBUS_DBUS_CLIENTS_BROWSER_H_
#define CHROMEOS_DBUS_DBUS_CLIENTS_BROWSER_H_

#include "base/component_export.h"

namespace dbus {
class Bus;
}

namespace chromeos {

// TODO(jamescook): Delete this class. http://crbug.com/647367
class COMPONENT_EXPORT(CHROMEOS_DBUS) DBusClientsBrowser {
 public:
  // Creates real implementations if |use_real_clients| is true and fakes
  // otherwise. Fakes are used when running on Linux desktop and in tests.
  explicit DBusClientsBrowser(bool use_real_clients);

  DBusClientsBrowser(const DBusClientsBrowser&) = delete;
  DBusClientsBrowser& operator=(const DBusClientsBrowser&) = delete;

  ~DBusClientsBrowser();

  void Initialize(dbus::Bus* system_bus);
};

}  // namespace chromeos

#endif  // CHROMEOS_DBUS_DBUS_CLIENTS_BROWSER_H_
