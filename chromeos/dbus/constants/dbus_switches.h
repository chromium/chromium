// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_DBUS_CONSTANTS_DBUS_SWITCHES_H_
#define CHROMEOS_DBUS_CONSTANTS_DBUS_SWITCHES_H_

#include "base/component_export.h"

namespace chromeos {
namespace switches {

COMPONENT_EXPORT(CHROMEOS_DBUS_CONSTANTS)
extern const char kAttestationServer[];
COMPONENT_EXPORT(CHROMEOS_DBUS_CONSTANTS)
extern const char kDbusStub[];
COMPONENT_EXPORT(CHROMEOS_DBUS_CONSTANTS)
extern const char kFakeOobeConfiguration[];
COMPONENT_EXPORT(CHROMEOS_DBUS_CONSTANTS)
extern const char kShillStub[];
COMPONENT_EXPORT(CHROMEOS_DBUS_CONSTANTS)
extern const char kSmsTestMessages[];
COMPONENT_EXPORT(CHROMEOS_DBUS_CONSTANTS)
extern const char kSystemDevMode[];
COMPONENT_EXPORT(CHROMEOS_DBUS_CONSTANTS)
extern const char kRegisterMaxDarkSuspendDelay[];

}  // namespace switches
}  // namespace chromeos

#endif  // CHROMEOS_DBUS_CONSTANTS_DBUS_SWITCHES_H_
