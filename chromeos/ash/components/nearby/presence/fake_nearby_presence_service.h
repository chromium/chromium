// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_NEARBY_PRESENCE_FAKE_NEARBY_PRESENCE_SERVICE_H_
#define CHROMEOS_ASH_COMPONENTS_NEARBY_PRESENCE_FAKE_NEARBY_PRESENCE_SERVICE_H_

#include "chromeos/ash/components/nearby/presence/enums/nearby_presence_enums.h"
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
      base::OnceCallback<void(std::unique_ptr<ScanSession>, enums::StatusCode)>
          on_start_scan_callback) override;
  void Initialize(base::OnceClosure on_initialized_callback) override;
  void UpdateCredentials() override;
  std::unique_ptr<NearbyPresenceConnectionsManager>
  CreateNearbyPresenceConnectionsManager() override;

  void FinishInitialization();
  void FinishStartScan(enums::StatusCode status_code);

  ScanDelegate* scan_delegate() { return scan_delegate_; }

  const std::optional<ScanFilter> scan_filter() const { return scan_filter_; }

 private:
  raw_ptr<ScanDelegate> scan_delegate_;
  std::optional<ScanFilter> scan_filter_ = std::nullopt;
  base::OnceCallback<void(std::unique_ptr<ScanSession>, enums::StatusCode)>
      pending_on_start_scan_callback_;
  base::OnceClosure pending_on_initialized_callback_;
};

}  // namespace ash::nearby::presence

#endif  // CHROMEOS_ASH_COMPONENTS_NEARBY_PRESENCE_FAKE_NEARBY_PRESENCE_SERVICE_H_
