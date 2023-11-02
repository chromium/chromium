// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "chromeos/constants/devicetype.h"

namespace syncer {

std::string GetChromeOSDeviceNameFromType() {
  switch (chromeos::GetDeviceType()) {
    case chromeos::DeviceType::kChromebase:
      return "Chromebase";
    case chromeos::DeviceType::kChromebit:
      return "Chromebit";
    case chromeos::DeviceType::kChromebook:
      return "Chromebook";
    case chromeos::DeviceType::kChromebox:
      return "Chromebox";
    case chromeos::DeviceType::kUnknown:
      break;
  }
  return "Chromebook";
}

std::string GetPersonalizableDeviceNameInternal() {
  return GetChromeOSDeviceNameFromType();
}

}  // namespace syncer
