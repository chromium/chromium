// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_CONSTANTS_DEVICETYPE_H_
#define CHROMEOS_CONSTANTS_DEVICETYPE_H_

#include "base/component_export.h"

namespace chromeos {

// The form factor of the device.
enum class DeviceType {
  kChromebook,
  kChromebase,
  kChromebit,
  kChromebox,
  kUnknown,  // Unknown fallback device.
};

// Returns the current device type, e.g. Chromebook, Chromebox.
COMPONENT_EXPORT(CHROMEOS_CONSTANTS) DeviceType GetDeviceType();

}  // namespace chromeos

#endif  // CHROMEOS_CONSTANTS_DEVICETYPE_H_
