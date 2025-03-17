// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_POLICY_EXTERNAL_STORAGE_DEVICE_ID_H_
#define CHROMEOS_ASH_COMPONENTS_POLICY_EXTERNAL_STORAGE_DEVICE_ID_H_

#include <cstdint>
#include <optional>
#include <string_view>

#include "base/component_export.h"
#include "base/values.h"

namespace ash::disks {
class Disk;
}  // namespace ash::disks

namespace policy {

// `DeviceId` corresponds to the `UsbDeviceId` type from
// `components/policy/resources/templates/common_schemas.yaml`. It is used to
// read and unpack the policy value into a proper C++ type.
//
// It consists of the vendor_id/product_id external storage identifier.
struct COMPONENT_EXPORT(CHROMEOS_ASH_COMPONENTS_POLICY) DeviceId {
  static const char kVendorId[];
  static const char kProductId[];

  DeviceId(uint16_t vid, uint16_t pid);

  // Constructs from a Value::Dict:
  // {
  //   "vendor_id": 39612,
  //   "product_id": 57072
  // }
  static std::optional<DeviceId> FromDict(const base::Value& dict);

  // Constructs from uint32_t, checking the range.
  static std::optional<DeviceId> FromUint32(uint32_t vid, uint32_t pid);

  // Constructs from hex strings:
  // ("9abc", "def0") -> DeviceId(39612, 57072)
  static std::optional<DeviceId> FromVidPid(std::string_view vid,
                                            std::string_view pid);

  // Constructs from given Disk* (disk->vendor_id() and disk->product_id()).
  static std::optional<DeviceId> FromDisk(const ash::disks::Disk* disk);

  // Creates a Value::Dict from the current DeviceId. Opposite of `FromDict`.
  base::Value::Dict ToDict() const;

  friend bool operator==(const DeviceId&, const DeviceId&) = default;

  uint16_t vid;
  uint16_t pid;
};

}  // namespace policy

#endif  // CHROMEOS_ASH_COMPONENTS_POLICY_EXTERNAL_STORAGE_DEVICE_ID_H_
