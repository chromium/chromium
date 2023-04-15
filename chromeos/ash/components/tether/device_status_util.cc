// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/tether/device_status_util.h"

#include <algorithm>

namespace ash {

namespace tether {

void NormalizeDeviceStatus(const DeviceStatus& status,
                           std::string* carrier_out,
                           int32_t* battery_percentage_out,
                           int32_t* signal_strength_out) {
  // Use a sentinel value if carrier information is not available. This value is
  // special-cased and replaced with a localized string in the settings UI.
  if (carrier_out) {
    constexpr char kDefaultCellCarrierName[] = "unknown-carrier";
    *carrier_out =
        (!status.has_cell_provider() || status.cell_provider().empty())
            ? kDefaultCellCarrierName
            : status.cell_provider();
  }

  // If battery or signal strength are missing, assume they are 100. For
  // battery percentage, force the value to be between 0 and 100. For signal
  // strength, convert from Android signal strength to Chrome OS signal
  // strength and force the value to be between 0 and 100.
  if (battery_percentage_out) {
    *battery_percentage_out =
        status.has_battery_percentage()
            ? std::clamp(status.battery_percentage(), 0, 100)
            : 100;
  }
  if (signal_strength_out) {
    // Android signal strength is measured between 0 and 4 (inclusive), but
    // Chrome OS signal strength is measured between 0 and 100 (inclusive). In
    // order to convert between Android signal strength to Chrome OS signal
    // strength, the value must be multiplied by the below value.
    constexpr int32_t kConversionFactor = 100 / 4;
    *signal_strength_out =
        status.has_connection_strength()
            ? std::clamp(kConversionFactor * status.connection_strength(), 0,
                         100)
            : 100;
  }
}

}  // namespace tether

}  // namespace ash
