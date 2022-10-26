// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_SERVICES_SECURE_CHANNEL_FAKE_NEARBY_CONNECTION_MANAGER_H_
#define CHROMEOS_ASH_SERVICES_SECURE_CHANNEL_FAKE_NEARBY_CONNECTION_MANAGER_H_

#include "chromeos/ash/services/secure_channel/nearby_connection_manager.h"

namespace ash::secure_channel {

class FakeNearbyConnectionManager : public NearbyConnectionManager {
 public:
  FakeNearbyConnectionManager();
  FakeNearbyConnectionManager(const FakeNearbyConnectionManager&) = delete;
  FakeNearbyConnectionManager& operator=(const FakeNearbyConnectionManager&) =
      delete;
  ~FakeNearbyConnectionManager() override;

  using NearbyConnectionManager::DoesAttemptExist;
  using NearbyConnectionManager::GetDeviceIdPairsForRemoteDevice;
  using NearbyConnectionManager::NotifyNearbyInitiatorConnectionSuccess;
  using NearbyConnectionManager::NotifyNearbyInitiatorFailure;

 private:
  // NearbyConnectionManager:
  void PerformAttemptNearbyInitiatorConnection(
      const DeviceIdPair& device_id_pair) override;
  void PerformCancelNearbyInitiatorConnectionAttempt(
      const DeviceIdPair& device_id_pair) override;
};

}  // namespace ash::secure_channel

#endif  // CHROMEOS_ASH_SERVICES_SECURE_CHANNEL_FAKE_NEARBY_CONNECTION_MANAGER_H_
