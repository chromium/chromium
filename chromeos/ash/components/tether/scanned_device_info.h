// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_TETHER_SCANNED_DEVICE_INFO_H_
#define CHROMEOS_ASH_COMPONENTS_TETHER_SCANNED_DEVICE_INFO_H_

#include "base/types/expected.h"
#include "chromeos/ash/components/tether/proto/tether.pb.h"

namespace ash::tether {

struct ScannedDeviceInfo {
  ScannedDeviceInfo(const std::string& device_id,
                    const std::string& device_name,
                    std::optional<DeviceStatus> device_status,
                    bool setup_required,
                    bool notifications_enabled);
  ScannedDeviceInfo(const ScannedDeviceInfo&);
  ~ScannedDeviceInfo();

  friend bool operator==(const ScannedDeviceInfo& first,
                         const ScannedDeviceInfo& second);

  std::string device_id;
  std::string device_name;
  std::optional<DeviceStatus> device_status;
  bool notifications_enabled;
  bool setup_required;
};

}  // namespace ash::tether

#endif  // CHROMEOS_ASH_COMPONENTS_TETHER_SCANNED_DEVICE_INFO_H_
