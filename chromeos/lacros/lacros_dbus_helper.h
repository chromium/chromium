// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_LACROS_LACROS_DBUS_HELPER_H_
#define CHROMEOS_LACROS_LACROS_DBUS_HELPER_H_

#include "base/component_export.h"

namespace chromeos {

// Initializes the D-Bus thread manager and Chrome DBus services for Lacros.
COMPONENT_EXPORT(CHROMEOS_LACROS) void LacrosInitializeDBus();

// Shuts down the D-Bus thread manager and Chrome DBus services for Lacros.
COMPONENT_EXPORT(CHROMEOS_LACROS) void LacrosShutdownDBus();

}  // namespace chromeos

#endif  // CHROMEOS_LACROS_LACROS_DBUS_HELPER_H_
