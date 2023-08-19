// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/services/bluetooth_config/fast_pair_delegate.h"
#include "ash/constants/ash_features.h"

namespace ash::bluetooth_config {

FastPairDelegate::FastPairDelegate() = default;

FastPairDelegate::~FastPairDelegate() = default;

void FastPairDelegate::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void FastPairDelegate::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

void FastPairDelegate::NotifyFastPairableDevicesChanged(
    const std::vector<mojom::PairedBluetoothDevicePropertiesPtr>&
        fast_pairable_devices) {
  CHECK(base::FeatureList::IsEnabled(
      features::kFastPairDevicesBluetoothSettings));
  for (auto& obs : observers_) {
    obs.OnFastPairableDevicesChanged(fast_pairable_devices);
  }
}

}  // namespace ash::bluetooth_config
