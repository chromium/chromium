// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_NEARBY_PRESENCE_FAKE_NEARBY_PRESENCE_SERVICE_H_
#define CHROMEOS_ASH_COMPONENTS_NEARBY_PRESENCE_FAKE_NEARBY_PRESENCE_SERVICE_H_

#include "chromeos/ash/components/nearby/presence/nearby_presence_service.h"

namespace ash::nearby::presence {

class FakeNearbyPresenceService : public NearbyPresenceService {
 public:
  FakeNearbyPresenceService();
  FakeNearbyPresenceService(const FakeNearbyPresenceService&) = delete;
  FakeNearbyPresenceService& operator=(const FakeNearbyPresenceService&) =
      delete;
  ~FakeNearbyPresenceService() override;

  // NearbyPresenceService:
  void StartScan(
      ScanFilter scan_filter,
      ScanDelegate* scan_delegate,
      base::OnceCallback<void(std::unique_ptr<ScanSession>, StatusCode)>
          on_start_scan_callback) override;
  void Initialize(base::OnceClosure on_initialized_callback) override;
  void UpdateCredentials() override;
  std::unique_ptr<NearbyPresenceConnectionsManager>
  CreateNearbyPresenceConnectionsManager() override;
};

}  // namespace ash::nearby::presence

#endif  // CHROMEOS_ASH_COMPONENTS_NEARBY_PRESENCE_FAKE_NEARBY_PRESENCE_SERVICE_H_
