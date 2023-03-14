// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/services/sharing/nearby/platform/ble_v2_peripheral.h"

namespace nearby::chrome {

BleV2Peripheral::BleV2Peripheral() = default;

BleV2Peripheral::BleV2Peripheral(BleV2Peripheral&&) = default;

BleV2Peripheral& BleV2Peripheral::operator=(BleV2Peripheral&&) = default;

BleV2Peripheral::~BleV2Peripheral() = default;

std::string BleV2Peripheral::GetAddress() const {
  return "NOT_IMPLEMENTED";
}

}  // namespace nearby::chrome
