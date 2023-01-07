// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_DBUS_CONSTANTS_CRYPTOHOME_KEY_DELEGATE_CONSTANTS_H_
#define CHROMEOS_ASH_COMPONENTS_DBUS_CONSTANTS_CRYPTOHOME_KEY_DELEGATE_CONSTANTS_H_

#include "base/component_export.h"

namespace cryptohome {

// Name and path of the D-Bus service that is run by Chrome and implements the
// org.chromium.CryptohomeKeyDelegateInterface interface. See the interface
// definition in the Chrome OS repo in
// src/platform2/cryptohome/dbus_bindings/
//   org.chromium.CryptohomeKeyDelegateInterface.xml .
COMPONENT_EXPORT(ASH_DBUS_CONSTANTS)
extern const char kCryptohomeKeyDelegateServiceName[];
COMPONENT_EXPORT(ASH_DBUS_CONSTANTS)
extern const char kCryptohomeKeyDelegateServicePath[];

}  // namespace cryptohome

#endif  // CHROMEOS_ASH_COMPONENTS_DBUS_CONSTANTS_CRYPTOHOME_KEY_DELEGATE_CONSTANTS_H_
