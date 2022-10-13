// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_TETHER_HOST_SCAN_TEST_UTIL_H_
#define CHROMEOS_ASH_COMPONENTS_TETHER_HOST_SCAN_TEST_UTIL_H_

#include <string>
#include <unordered_map>

#include "chromeos/ash/components/tether/host_scan_cache_entry.h"

namespace ash {

namespace tether {

namespace host_scan_test_util {

constexpr char kTetherGuid0[] = "kTetherGuid0";
constexpr char kTetherGuid1[] = "kTetherGuid1";
constexpr char kTetherGuid2[] = "kTetherGuid2";
constexpr char kTetherGuid3[] = "kTetherGuid3";

constexpr char kTetherDeviceName0[] = "kDeviceName0";
constexpr char kTetherDeviceName1[] = "kDeviceName1";
constexpr char kTetherDeviceName2[] = "kDeviceName2";
constexpr char kTetherDeviceName3[] = "kDeviceName3";

constexpr char kTetherCarrier0[] = "kTetherCarrier0";
constexpr char kTetherCarrier1[] = "kTetherCarrier1";
constexpr char kTetherCarrier2[] = "kTetherCarrier2";
constexpr char kTetherCarrier3[] = "kTetherCarrier3";

constexpr int kTetherBatteryPercentage0 = 20;
constexpr int kTetherBatteryPercentage1 = 40;
constexpr int kTetherBatteryPercentage2 = 60;
constexpr int kTetherBatteryPercentage3 = 80;

constexpr int kTetherSignalStrength0 = 25;
constexpr int kTetherSignalStrength1 = 50;
constexpr int kTetherSignalStrength2 = 75;
constexpr int kTetherSignalStrength3 = 100;

constexpr bool kTetherSetupRequired0 = true;
constexpr bool kTetherSetupRequired1 = false;
constexpr bool kTetherSetupRequired2 = true;
constexpr bool kTetherSetupRequired3 = false;

// Returns a map from tether network GUID to entry containing test entries.
// The returned map has 4 entries corresponding to the 4 sets of constains
// defined above.
std::unordered_map<std::string, HostScanCacheEntry> CreateTestEntries();

}  // namespace host_scan_test_util

}  // namespace tether

}  // namespace ash

#endif  // CHROMEOS_ASH_COMPONENTS_TETHER_HOST_SCAN_TEST_UTIL_H_
