// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef CHROMEOS_ASH_COMPONENTS_NETWORK_TETHER_CONSTANTS_H_
#define CHROMEOS_ASH_COMPONENTS_NETWORK_TETHER_CONSTANTS_H_

#include "base/component_export.h"

namespace chromeos {

// This file contains constants for Chrome OS tether networks which are used
// wherever Shill constants are appropriate. Tether networks are a Chrome OS
// concept which does not exist as part of Shill, so these custom definitions
// are used instead. Tether networks are never intended to be passed to Shill
// code, so these constants are used primarily as part of NetworkStateHandler.

// Represents the tether network type.
COMPONENT_EXPORT(CHROMEOS_NETWORK) extern const char kTypeTether[];

// Properties associated with tether networks.
COMPONENT_EXPORT(CHROMEOS_NETWORK) extern const char kTetherBatteryPercentage[];
COMPONENT_EXPORT(CHROMEOS_NETWORK) extern const char kTetherCarrier[];
COMPONENT_EXPORT(CHROMEOS_NETWORK)
extern const char kTetherHasConnectedToHost[];
COMPONENT_EXPORT(CHROMEOS_NETWORK) extern const char kTetherSignalStrength[];

// The device path used for the tether DeviceState.
COMPONENT_EXPORT(CHROMEOS_NETWORK) extern const char kTetherDevicePath[];

// The name used for the tether DeviceState.
COMPONENT_EXPORT(CHROMEOS_NETWORK) extern const char kTetherDeviceName[];

}  // namespace chromeos

// TODO(https://crbug.com/1164001): remove when it moved to ash.
namespace ash {
using ::chromeos::kTetherBatteryPercentage;
using ::chromeos::kTetherCarrier;
using ::chromeos::kTetherHasConnectedToHost;
using ::chromeos::kTetherSignalStrength;
using ::chromeos::kTypeTether;
}  // namespace ash

#endif  // CHROMEOS_ASH_COMPONENTS_NETWORK_TETHER_CONSTANTS_H_
