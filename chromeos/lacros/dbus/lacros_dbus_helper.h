// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_LACROS_DBUS_LACROS_DBUS_HELPER_H_
#define CHROMEOS_LACROS_DBUS_LACROS_DBUS_HELPER_H_

#include "base/component_export.h"

namespace chromeos {

// Initializes the D-Bus thread manager and Chrome DBus services for Lacros.
COMPONENT_EXPORT(CHROMEOS_LACROS) void LacrosInitializeDBus();

// D-Bus clients may depend on feature list. This initializes only those clients
// in Lacros, and must be called after feature list initialization.
COMPONENT_EXPORT(CHROMEOS_LACROS)
void LacrosInitializeFeatureListDependentDBus();

// Shuts down the D-Bus thread manager and Chrome DBus services for Lacros.
COMPONENT_EXPORT(CHROMEOS_LACROS) void LacrosShutdownDBus();

}  // namespace chromeos

#endif  // CHROMEOS_LACROS_DBUS_LACROS_DBUS_HELPER_H_
