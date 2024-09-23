// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_DEVICE_BLUETOOTH_LE_LE_SCAN_RESULT_H_
#define CHROMECAST_DEVICE_BLUETOOTH_LE_LE_SCAN_RESULT_H_

#include <map>
#include <optional>
#include <string>
#include <vector>

#include "base/containers/span.h"
#include "chromecast/public/bluetooth/bluetooth_types.h"

namespace chromecast {
namespace bluetooth {

struct LeScanResult {
  // https://www.bluetooth.com/specifications/assigned-numbers/generic-access-profile
  enum : uint8_t {
    kGapFlags = 0x01,
    kGapIncomplete16BitServiceUuids = 0x02,
    kGapComplete16BitServiceUuids = 0x03,
    kGapIncomplete32BitServiceUuids = 0x04,
    kGapComplete32BitServiceUuids = 0x05,
    kGapIncomplete128BitServiceUuids = 0x06,
    kGapComplete128BitServiceUuids = 0x07,
    kGapShortName = 0x08,
    kGapCompleteName = 0x09,
    kGapTxPowerLevel = 0x0a,
    kGap128bitServiceSolicitations = 0x15,
    kGapServicesData16bit = 0x16,
    kGapAppearance = 0x19,
    kGapServicesData32bit = 0x20,
    kGapServicesData128bit = 0x21,
    kGapManufacturerData = 0xff,
  };

  LeScanResult();
  LeScanResult(const LeScanResult& other);
  ~LeScanResult();

  bool SetAdvData(base::span<const uint8_t> adv_data);

  std::optional<std::string> Name() const;

  std::optional<uint8_t> Flags() const;

  using UuidList = std::vector<bluetooth_v2_shlib::Uuid>;
  std::optional<UuidList> AllServiceUuids() const;
  std::optional<UuidList> IncompleteListOf16BitServiceUuids() const;
  std::optional<UuidList> CompleteListOf16BitServiceUuids() const;
  std::optional<UuidList> IncompleteListOf32BitServiceUuids() const;
  std::optional<UuidList> CompleteListOf32BitServiceUuids() const;
  std::optional<UuidList> IncompleteListOf128BitServiceUuids() const;
  std::optional<UuidList> CompleteListOf128BitServiceUuids() const;

  using ServiceDataMap =
      std::map<bluetooth_v2_shlib::Uuid, std::vector<uint8_t>>;
  ServiceDataMap AllServiceData() const;
  ServiceDataMap ServiceData16Bit() const;
  ServiceDataMap ServiceData32Bit() const;
  ServiceDataMap ServiceData128Bit() const;

  std::map<uint16_t, std::vector<uint8_t>> ManufacturerData() const;

  bluetooth_v2_shlib::Addr addr;
  std::vector<uint8_t> adv_data;
  int rssi = -255;

  std::map<uint8_t, std::vector<std::vector<uint8_t>>> type_to_data;
};

}  // namespace bluetooth
}  // namespace chromecast

#endif  // CHROMECAST_DEVICE_BLUETOOTH_LE_LE_SCAN_RESULT_H_
