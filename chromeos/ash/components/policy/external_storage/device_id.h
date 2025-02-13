// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_POLICY_EXTERNAL_STORAGE_DEVICE_ID_H_
#define CHROMEOS_ASH_COMPONENTS_POLICY_EXTERNAL_STORAGE_DEVICE_ID_H_

#include <cstdint>

#include "base/component_export.h"

namespace policy {

// `DeviceId` corresponds to the `UsbDeviceId` type from
// `components/policy/resources/templates/common_schemas.yaml`. It is used to
// read and unpack the policy value into a proper C++ type.
//
// It consists of the vendor_id/product_id external storage identifier.
class COMPONENT_EXPORT(CHROMEOS_ASH_COMPONENTS_POLICY) DeviceId {
 public:
  DeviceId(uint16_t vid, uint16_t pid);

  // TODO(isandrk): Move to private section when more code gets added.
  uint16_t vid;
  uint16_t pid;
};

}  // namespace policy

#endif  // CHROMEOS_ASH_COMPONENTS_POLICY_EXTERNAL_STORAGE_DEVICE_ID_H_
