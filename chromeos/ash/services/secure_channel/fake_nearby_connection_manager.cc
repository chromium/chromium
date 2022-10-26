// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/services/secure_channel/fake_nearby_connection_manager.h"

namespace ash::secure_channel {

FakeNearbyConnectionManager::FakeNearbyConnectionManager() = default;

FakeNearbyConnectionManager::~FakeNearbyConnectionManager() = default;

void FakeNearbyConnectionManager::PerformAttemptNearbyInitiatorConnection(
    const DeviceIdPair& device_id_pair) {}

void FakeNearbyConnectionManager::PerformCancelNearbyInitiatorConnectionAttempt(
    const DeviceIdPair& device_id_pair) {}

}  // namespace ash::secure_channel
