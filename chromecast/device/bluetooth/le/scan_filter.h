// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_DEVICE_BLUETOOTH_LE_SCAN_FILTER_H_
#define CHROMECAST_DEVICE_BLUETOOTH_LE_SCAN_FILTER_H_

#include <optional>
#include <string>

#include "chromecast/device/bluetooth/le/le_scan_result.h"
#include "chromecast/public/bluetooth/bluetooth_types.h"

namespace chromecast {
namespace bluetooth {

struct ScanFilter {
  // Helper function to get ScanFilter which matches 16 bit |service_uuid|.
  static ScanFilter From16bitUuid(uint16_t service_uuid);

  ScanFilter();
  ScanFilter(const ScanFilter& other);
  ScanFilter(ScanFilter&& other);
  ~ScanFilter();

  bool Matches(const LeScanResult& scan_result) const;

  // Exact name.
  std::optional<std::string> name;

  // RE2 partial match on name. This is ignored if |name| is specified.
  // https://github.com/google/re2
  std::optional<std::string> regex_name;

  std::optional<bluetooth_v2_shlib::Uuid> service_uuid;
};

}  // namespace bluetooth
}  // namespace chromecast

#endif  // CHROMECAST_DEVICE_BLUETOOTH_LE_SCAN_FILTER_H_
