// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/nearby/presence/fake_nearby_presence_service.h"

namespace ash::nearby::presence {

FakeNearbyPresenceService::FakeNearbyPresenceService() = default;

FakeNearbyPresenceService::~FakeNearbyPresenceService() = default;

void FakeNearbyPresenceService::StartScan(
    ScanFilter scan_filter,
    ScanDelegate* scan_delegate,
    base::OnceCallback<void(std::unique_ptr<ScanSession>,
                            NearbyPresenceService::StatusCode)>
        on_start_scan_callback) {
  NOTIMPLEMENTED();
}

void FakeNearbyPresenceService::Initialize(
    base::OnceClosure on_initialized_callback) {
  NOTIMPLEMENTED();
}
void FakeNearbyPresenceService::UpdateCredentials() {
  NOTIMPLEMENTED();
}

std::unique_ptr<NearbyPresenceConnectionsManager>
FakeNearbyPresenceService::CreateNearbyPresenceConnectionsManager() {
  NOTIMPLEMENTED();
  return nullptr;
}

}  // namespace ash::nearby::presence
