// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_TETHER_DEVICE_STATUS_UTIL_H_
#define CHROMEOS_ASH_COMPONENTS_TETHER_DEVICE_STATUS_UTIL_H_

#include "chromeos/ash/components/tether/proto/tether.pb.h"

namespace ash {

namespace tether {

// Normalizes the values contained in |device_status| and outputs the normalized
// values for carrier, battery percentage, and signal strength. The values are
// normalized according to the following rules:
//   (1) Carrier: If the proto's cell_provider field is present and non-empty,
//       it is output; otherwise, the "unknown-carrier" constant is output.
//   (2) Battery percentage: If the proto's battery_percentage field is present
//       and within the range [0, 100], it is output; if the field is present
//       but not in [0, 100], 0 is output if the input is <0 and 100 is output
//       if the input is >100; if the field is not present, 100 is output.
//   (3) Signal strength: If the proto's connection_strength field is present
//       and within the range [0, 4], it is multiplied by 25 and output in a new
//       range [0, 100]; if the field is present but not in [0, 4], 0 is output
//       if the input is <0 and 100 is output if the input >4; if the field is
//       not present, 100 is output. Note that the multipier is needed because
//       Android's connection strength is a value from 0 to 4, while Chrome OS's
//       signal strength ranges from 0 to 100.
void NormalizeDeviceStatus(const DeviceStatus& status,
                           std::string* carrier_out,
                           int32_t* battery_percentage_out,
                           int32_t* signal_strength_out);

}  // namespace tether

}  // namespace ash

#endif  // CHROMEOS_ASH_COMPONENTS_TETHER_DEVICE_STATUS_UTIL_H_
