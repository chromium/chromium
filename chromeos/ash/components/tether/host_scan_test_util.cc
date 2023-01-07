// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/tether/host_scan_test_util.h"

namespace ash {

namespace tether {

namespace host_scan_test_util {

std::unordered_map<std::string, HostScanCacheEntry> CreateTestEntries() {
  std::unordered_map<std::string, HostScanCacheEntry> entries;

  entries.emplace(kTetherGuid0,
                  *HostScanCacheEntry::Builder()
                       .SetTetherNetworkGuid(kTetherGuid0)
                       .SetDeviceName(kTetherDeviceName0)
                       .SetCarrier(kTetherCarrier0)
                       .SetBatteryPercentage(kTetherBatteryPercentage0)
                       .SetSignalStrength(kTetherSignalStrength0)
                       .SetSetupRequired(kTetherSetupRequired0)
                       .Build());

  entries.emplace(kTetherGuid1,
                  *HostScanCacheEntry::Builder()
                       .SetTetherNetworkGuid(kTetherGuid1)
                       .SetDeviceName(kTetherDeviceName1)
                       .SetCarrier(kTetherCarrier1)
                       .SetBatteryPercentage(kTetherBatteryPercentage1)
                       .SetSignalStrength(kTetherSignalStrength1)
                       .SetSetupRequired(kTetherSetupRequired1)
                       .Build());

  entries.emplace(kTetherGuid2,
                  *HostScanCacheEntry::Builder()
                       .SetTetherNetworkGuid(kTetherGuid2)
                       .SetDeviceName(kTetherDeviceName2)
                       .SetCarrier(kTetherCarrier2)
                       .SetBatteryPercentage(kTetherBatteryPercentage2)
                       .SetSignalStrength(kTetherSignalStrength2)
                       .SetSetupRequired(kTetherSetupRequired2)
                       .Build());

  entries.emplace(kTetherGuid3,
                  *HostScanCacheEntry::Builder()
                       .SetTetherNetworkGuid(kTetherGuid3)
                       .SetDeviceName(kTetherDeviceName3)
                       .SetCarrier(kTetherCarrier3)
                       .SetBatteryPercentage(kTetherBatteryPercentage3)
                       .SetSignalStrength(kTetherSignalStrength3)
                       .SetSetupRequired(kTetherSetupRequired3)
                       .Build());

  return entries;
}

}  // namespace host_scan_test_util

}  // namespace tether

}  // namespace ash
