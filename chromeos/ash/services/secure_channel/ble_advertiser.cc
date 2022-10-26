// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/services/secure_channel/ble_advertiser.h"

namespace ash::secure_channel {

BleAdvertiser::BleAdvertiser(Delegate* delegate) : delegate_(delegate) {}

BleAdvertiser::~BleAdvertiser() = default;

void BleAdvertiser::NotifyAdvertisingSlotEnded(
    const DeviceIdPair& device_id_pair,
    bool replaced_by_higher_priority_advertisement) {
  delegate_->OnAdvertisingSlotEnded(device_id_pair,
                                    replaced_by_higher_priority_advertisement);
}

void BleAdvertiser::NotifyFailureToGenerateAdvertisement(
    const DeviceIdPair& device_id_pair) {
  delegate_->OnFailureToGenerateAdvertisement(device_id_pair);
}

}  // namespace ash::secure_channel
