// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/policy/external_storage/device_id.h"

#include <limits>

#include "base/logging.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "chromeos/ash/components/disks/disk.h"

namespace policy {

// static
const char DeviceId::kVendorId[] = "vendor_id";
const char DeviceId::kProductId[] = "product_id";

DeviceId::DeviceId(uint16_t vid, uint16_t pid) : vid(vid), pid(pid) {}

// static
std::optional<DeviceId> DeviceId::FromDict(const base::Value& value) {
  // Verify value is a dict.
  if (!value.is_dict()) {
    LOG(ERROR) << "Value not a dict: " << value.DebugString();
    return std::nullopt;
  }
  const base::Value::Dict& dict = value.GetDict();

  std::optional<int> vid = dict.FindInt(kVendorId);
  std::optional<int> pid = dict.FindInt(kProductId);

  // Verify values exist and are integers.
  if (!vid.has_value() || !pid.has_value()) {
    LOG(ERROR) << "vid or pid missing or not an int: " << value.DebugString();
    return std::nullopt;
  }

  // Verify values are in the uint16 range.
  if (vid.value() < std::numeric_limits<uint16_t>::min() ||
      vid.value() > std::numeric_limits<uint16_t>::max() ||
      pid.value() < std::numeric_limits<uint16_t>::min() ||
      pid.value() > std::numeric_limits<uint16_t>::max()) {
    LOG(ERROR) << "vid or pid out of range: " << value.DebugString();
    return std::nullopt;
  }

  return DeviceId(static_cast<uint16_t>(vid.value()),
                  static_cast<uint16_t>(pid.value()));
}

// static
std::optional<DeviceId> DeviceId::FromUint32(uint32_t vid, uint32_t pid) {
  // Verify values are in the uint16 range.
  if (vid > std::numeric_limits<uint16_t>::max() ||
      pid > std::numeric_limits<uint16_t>::max()) {
    LOG(ERROR) << base::StringPrintf("vid (%d) or pid (%d) out of range", vid,
                                     pid);
    return std::nullopt;
  }

  return DeviceId(static_cast<uint16_t>(vid), static_cast<uint16_t>(pid));
}

// static
std::optional<DeviceId> DeviceId::FromVidPid(std::string_view vid,
                                             std::string_view pid) {
  uint32_t vid_int, pid_int;
  if (!base::HexStringToUInt(vid, &vid_int) ||
      !base::HexStringToUInt(pid, &pid_int)) {
    LOG(ERROR) << base::StringPrintf(
        "Couldn't convert vid ('%s') or pid ('%s') to uint", vid, pid);
    return std::nullopt;
  }

  return FromUint32(vid_int, pid_int);
}

// static
std::optional<DeviceId> DeviceId::FromDisk(const ash::disks::Disk* disk) {
  if (!disk) {
    LOG(ERROR) << "disk is nullptr";
    return std::nullopt;
  }
  return DeviceId::FromVidPid(disk->vendor_id(), disk->product_id());
}

base::Value::Dict DeviceId::ToDict() const {
  return base::Value::Dict().Set(kVendorId, vid).Set(kProductId, pid);
}

}  // namespace policy
