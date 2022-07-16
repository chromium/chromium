// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/services/bluetooth_config/fake_fast_pair_delegate.h"

namespace chromeos {
namespace bluetooth_config {

FakeFastPairDelegate::FakeFastPairDelegate() = default;

FakeFastPairDelegate::~FakeFastPairDelegate() = default;

void FakeFastPairDelegate::SetDeviceNameManager(
    DeviceNameManager* device_name_manager) {
  device_name_manager_ = device_name_manager;
}

}  // namespace bluetooth_config
}  // namespace chromeos
