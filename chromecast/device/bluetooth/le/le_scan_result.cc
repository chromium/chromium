// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/device/bluetooth/le/le_scan_result.h"

#include <algorithm>

#include "base/logging.h"
#include "chromecast/device/bluetooth/bluetooth_util.h"

namespace chromecast {
namespace bluetooth {

namespace {

template <typename T, bluetooth_v2_shlib::Uuid (*converter)(T)>
std::optional<LeScanResult::UuidList> GetUuidsFromShort(
    const std::map<uint8_t, std::vector<std::vector<uint8_t>>>& type_to_data,
    uint8_t type) {
  auto it = type_to_data.find(type);
  if (it == type_to_data.end()) {
    return std::nullopt;
  }

  LeScanResult::UuidList ret;
  for (const auto& field : it->second) {
    if (field.size() % sizeof(T)) {
      LOG(ERROR) << "Invalid length, expected multiple of " << sizeof(T);
      return std::nullopt;
    }

    for (size_t i = 0; i < field.size(); i += sizeof(T)) {
      // Uuids are transmitted in little endian byte order. (Bluetooth Core
      // Specification v4.0 Vol 3 Part C Section 11.1).
      T value = 0;
      for (size_t j = 0; j < sizeof(T); ++j) {
        value |= field[i + j] << 8 * j;
      }

      ret.push_back(converter(value));
    }
  }

  return ret;
}

std::optional<LeScanResult::UuidList> GetUuidsAsUuid(
    const std::map<uint8_t, std::vector<std::vector<uint8_t>>>& type_to_data,
    uint8_t type) {
  auto it = type_to_data.find(type);
  if (it == type_to_data.end()) {
    return std::nullopt;
  }

  LeScanResult::UuidList ret;
  for (const auto& field : it->second) {
    if (field.size() % sizeof(bluetooth_v2_shlib::Uuid)) {
      LOG(ERROR) << "Invalid length, expected multiple of "
                 << sizeof(bluetooth_v2_shlib::Uuid);
      return std::nullopt;
    }

    for (size_t i = 0; i < field.size();
         i += sizeof(bluetooth_v2_shlib::Uuid)) {
      ret.emplace_back();
      // GAP UUIDs are little endian and bluetooth_v2_shlib::Uuid is big endian.
      std::reverse_copy(field.begin() + i,
                        field.begin() + i + sizeof(bluetooth_v2_shlib::Uuid),
                        ret.back().begin());
    }
  }

  return ret;
}

}  // namespace

LeScanResult::LeScanResult() = default;
LeScanResult::LeScanResult(const LeScanResult& other) = default;
LeScanResult::~LeScanResult() = default;

bool LeScanResult::SetAdvData(base::span<const uint8_t> advertisement_data) {
  std::map<uint8_t, std::vector<std::vector<uint8_t>>> new_type_to_data;

  size_t i = 0;
  while (i < advertisement_data.size()) {
    if (i + 1 == advertisement_data.size()) {
      LOG(ERROR) << "Malformed BLE packet";
      return false;
    }

    // http://www.argenox.com/bluetooth-low-energy-ble-v4-0-development/library/a-ble-advertising-primer/
    // Format:
    // [size][type][payload     ]
    // [i   ][i+1 ][i+2:i+1+size]
    //
    // Note: size does not include its own byte
    uint8_t size = advertisement_data[i];
    uint8_t type = advertisement_data[i + 1];

    // Avoid infinite loop if invalid data
    if (size == 0 || i + 1 + size > advertisement_data.size()) {
      LOG(ERROR) << "Invalid size";
      return false;
    }

    base::span<const uint8_t> data =
        advertisement_data.subspan(i + 2, size - 1);
    new_type_to_data[type].emplace_back(data.begin(), data.end());

    i += (size + 1);
  }

  adv_data.assign(advertisement_data.begin(), advertisement_data.end());
  type_to_data.swap(new_type_to_data);
  return true;
}

std::optional<std::string> LeScanResult::Name() const {
  auto it = type_to_data.find(kGapCompleteName);
  if (it != type_to_data.end()) {
    DCHECK_GE(it->second.size(), 1u);
    return std::string(reinterpret_cast<const char*>(it->second[0].data()),
                       it->second[0].size());
  }

  it = type_to_data.find(kGapShortName);
  if (it != type_to_data.end()) {
    DCHECK_GE(it->second.size(), 1u);
    return std::string(reinterpret_cast<const char*>(it->second[0].data()),
                       it->second[0].size());
  }

  return std::nullopt;
}

std::optional<uint8_t> LeScanResult::Flags() const {
  auto it = type_to_data.find(kGapFlags);
  if (it == type_to_data.end()) {
    return std::nullopt;
  }

  DCHECK_GE(it->second.size(), 1u);
  if (it->second[0].size() != 1) {
    LOG(ERROR) << "Invalid length for flags";
    return std::nullopt;
  }

  return it->second[0][0];
}

std::optional<LeScanResult::UuidList> LeScanResult::AllServiceUuids() const {
  bool any_exist = false;
  UuidList ret;
  auto insert_if_exists = [&ret, &any_exist](std::optional<UuidList> list) {
    if (list) {
      any_exist = true;
      ret.insert(ret.end(), list->begin(), list->end());
    }
  };

  insert_if_exists(IncompleteListOf16BitServiceUuids());
  insert_if_exists(CompleteListOf16BitServiceUuids());
  insert_if_exists(IncompleteListOf32BitServiceUuids());
  insert_if_exists(CompleteListOf32BitServiceUuids());
  insert_if_exists(IncompleteListOf128BitServiceUuids());
  insert_if_exists(CompleteListOf128BitServiceUuids());

  if (!any_exist) {
    return std::nullopt;
  }

  return ret;
}

std::optional<LeScanResult::UuidList>
LeScanResult::IncompleteListOf16BitServiceUuids() const {
  return GetUuidsFromShort<uint16_t, util::UuidFromInt16>(
      type_to_data, kGapIncomplete16BitServiceUuids);
}

std::optional<LeScanResult::UuidList>
LeScanResult::CompleteListOf16BitServiceUuids() const {
  return GetUuidsFromShort<uint16_t, util::UuidFromInt16>(
      type_to_data, kGapComplete16BitServiceUuids);
}

std::optional<LeScanResult::UuidList>
LeScanResult::IncompleteListOf32BitServiceUuids() const {
  return GetUuidsFromShort<uint32_t, util::UuidFromInt32>(
      type_to_data, kGapIncomplete32BitServiceUuids);
}

std::optional<LeScanResult::UuidList>
LeScanResult::CompleteListOf32BitServiceUuids() const {
  return GetUuidsFromShort<uint32_t, util::UuidFromInt32>(
      type_to_data, kGapComplete32BitServiceUuids);
}

std::optional<LeScanResult::UuidList>
LeScanResult::IncompleteListOf128BitServiceUuids() const {
  return GetUuidsAsUuid(type_to_data, kGapIncomplete128BitServiceUuids);
}

std::optional<LeScanResult::UuidList>
LeScanResult::CompleteListOf128BitServiceUuids() const {
  return GetUuidsAsUuid(type_to_data, kGapComplete128BitServiceUuids);
}

LeScanResult::ServiceDataMap LeScanResult::AllServiceData() const {
  ServiceDataMap ret;

  auto sd16 = ServiceData16Bit();
  ret.insert(sd16.begin(), sd16.end());

  auto sd32 = ServiceData32Bit();
  ret.insert(sd32.begin(), sd32.end());

  auto sd128 = ServiceData128Bit();
  ret.insert(sd128.begin(), sd128.end());

  return ret;
}

LeScanResult::ServiceDataMap LeScanResult::ServiceData16Bit() const {
  ServiceDataMap ret;
  auto it = type_to_data.find(kGapServicesData16bit);
  if (it == type_to_data.end()) {
    return ret;
  }

  for (const auto& data : it->second) {
    uint16_t uuid = 0;
    if (data.size() < sizeof(uuid)) {
      LOG(ERROR) << "Invalid service data, too short";
      ret.clear();
      return ret;
    }
    uuid = data[1] << 8 | data[0];
    ret[util::UuidFromInt16(uuid)] =
        std::vector<uint8_t>(data.begin() + sizeof(uuid), data.end());
  }

  return ret;
}

LeScanResult::ServiceDataMap LeScanResult::ServiceData32Bit() const {
  ServiceDataMap ret;
  auto it = type_to_data.find(kGapServicesData32bit);
  if (it == type_to_data.end()) {
    return ret;
  }

  for (const auto& data : it->second) {
    uint32_t uuid = 0;
    if (data.size() < sizeof(uuid)) {
      LOG(ERROR) << "Invalid service data, too short";
      ret.clear();
      return ret;
    }
    uuid = data[3] << 24 | data[2] << 16 | data[1] << 8 | data[0];
    ret[util::UuidFromInt32(uuid)].assign(data.begin() + sizeof(uuid),
                                          data.end());
  }

  return ret;
}

LeScanResult::ServiceDataMap LeScanResult::ServiceData128Bit() const {
  ServiceDataMap ret;
  auto it = type_to_data.find(kGapServicesData128bit);
  if (it == type_to_data.end()) {
    return ret;
  }

  for (const auto& data : it->second) {
    bluetooth_v2_shlib::Uuid uuid;
    if (data.size() < sizeof(uuid)) {
      LOG(ERROR) << "Invalid service data, too short";
      ret.clear();
      return ret;
    }
    std::reverse_copy(data.begin(), data.begin() + sizeof(uuid), uuid.begin());
    ret[uuid].assign(data.begin() + sizeof(uuid), data.end());
  }

  return ret;
}

std::map<uint16_t, std::vector<uint8_t>> LeScanResult::ManufacturerData()
    const {
  std::map<uint16_t, std::vector<uint8_t>> ret;
  auto it = type_to_data.find(kGapManufacturerData);
  if (it == type_to_data.end()) {
    return ret;
  }

  for (const auto& data : it->second) {
    uint16_t id = 0;
    if (data.size() < sizeof(id)) {
      LOG(ERROR) << "Invalid manufacturer data, too short";
      ret.clear();
      return ret;
    }
    id = data[1] << 8 | data[0];
    ret[id].assign(data.begin() + sizeof(id), data.end());
  }

  return ret;
}

}  // namespace bluetooth
}  // namespace chromecast
