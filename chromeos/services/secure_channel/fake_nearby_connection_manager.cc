// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/services/secure_channel/fake_nearby_connection_manager.h"

namespace chromeos {

namespace secure_channel {

FakeNearbyConnectionManager::FakeNearbyConnectionManager() = default;

FakeNearbyConnectionManager::~FakeNearbyConnectionManager() = default;

void FakeNearbyConnectionManager::PerformAttemptNearbyInitiatorConnection(
    const DeviceIdPair& device_id_pair) {}

void FakeNearbyConnectionManager::PerformCancelNearbyInitiatorConnectionAttempt(
    const DeviceIdPair& device_id_pair) {}

}  // namespace secure_channel

}  // namespace chromeos
