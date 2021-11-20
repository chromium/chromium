// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_DBUS_FWUPD_FWUPD_DEVICE_H_
#define CHROMEOS_DBUS_FWUPD_FWUPD_DEVICE_H_

#include <string>

#include "base/component_export.h"

namespace chromeos {

// Structure to hold FwupdDevice data received from fwupd.
struct COMPONENT_EXPORT(CHROMEOS_DBUS_FWUPD) FwupdDevice {
  FwupdDevice();
  FwupdDevice(const std::string& id, const std::string& device_name);
  FwupdDevice(const FwupdDevice& other);
  FwupdDevice& operator=(const FwupdDevice& other);
  ~FwupdDevice();

  std::string id;
  std::string device_name;
};

using FwupdDeviceList = std::vector<FwupdDevice>;

}  // namespace chromeos

#endif  // CHROMEOS_DBUS_FWUPD_FWUPD_DEVICE_H_
