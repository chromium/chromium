// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_DBUS_FWUPD_FWUPD_DEVICE_H_
#define CHROMEOS_ASH_COMPONENTS_DBUS_FWUPD_FWUPD_DEVICE_H_

#include <string>

#include "base/component_export.h"

namespace ash {

// Structure to hold FwupdDevice data received from fwupd.
struct COMPONENT_EXPORT(ASH_DBUS_FWUPD) FwupdDevice {
  FwupdDevice();
  FwupdDevice(const std::string& id, const std::string& device_name);
  FwupdDevice(const FwupdDevice& other);
  FwupdDevice& operator=(const FwupdDevice& other);
  ~FwupdDevice();

  std::string id;
  std::string device_name;
};

using FwupdDeviceList = std::vector<FwupdDevice>;

}  // namespace ash

#endif  // CHROMEOS_ASH_COMPONENTS_DBUS_FWUPD_FWUPD_DEVICE_H_
