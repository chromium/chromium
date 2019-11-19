// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/device/bluetooth/le/scan_filter.h"

#include "base/stl_util.h"
#include "chromecast/device/bluetooth/bluetooth_util.h"
#include "third_party/re2/src/re2/re2.h"

namespace chromecast {
namespace bluetooth {

// static
ScanFilter ScanFilter::From16bitUuid(uint16_t service_uuid) {
  ScanFilter filter;
  filter.service_uuid = util::UuidFromInt16(service_uuid);
  return filter;
}

ScanFilter::ScanFilter() = default;
ScanFilter::ScanFilter(const ScanFilter& other) = default;
ScanFilter::ScanFilter(ScanFilter&& other) = default;
ScanFilter::~ScanFilter() = default;

bool ScanFilter::Matches(const LeScanResult& scan_result) const {
  if (name && name != scan_result.Name()) {
    return false;
  }

  if (service_uuid) {
    base::Optional<LeScanResult::UuidList> all_uuids =
        scan_result.AllServiceUuids();
    if (!all_uuids) {
      return false;
    }

    if (!base::Contains(*all_uuids, *service_uuid)) {
      return false;
    }
  }

  if (!name && regex_name) {
    base::Optional<std::string> scan_name = scan_result.Name();
    if (!scan_name || !RE2::PartialMatch(*scan_name, *regex_name)) {
      return false;
    }
  }

  return true;
}

}  // namespace bluetooth
}  // namespace chromecast
