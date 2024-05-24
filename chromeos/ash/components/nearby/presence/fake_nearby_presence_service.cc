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
    base::OnceCallback<void(std::unique_ptr<ScanSession>, enums::StatusCode)>
        on_start_scan_callback) {
  scan_filter_ = scan_filter;
  scan_delegate_ = scan_delegate;
  pending_on_start_scan_callback_ = std::move(on_start_scan_callback);
}

void FakeNearbyPresenceService::Initialize(
    base::OnceClosure on_initialized_callback) {
  CHECK(!pending_on_initialized_callback_);
  pending_on_initialized_callback_ = std::move(on_initialized_callback);
}
void FakeNearbyPresenceService::UpdateCredentials() {
  NOTIMPLEMENTED();
}

std::unique_ptr<NearbyPresenceConnectionsManager>
FakeNearbyPresenceService::CreateNearbyPresenceConnectionsManager() {
  NOTIMPLEMENTED();
  return nullptr;
}

void FakeNearbyPresenceService::FinishInitialization() {
  CHECK(pending_on_initialized_callback_);
  std::move(pending_on_initialized_callback_).Run();
}

void FakeNearbyPresenceService::FinishStartScan(enums::StatusCode status_code) {
  CHECK(pending_on_start_scan_callback_);
  mojo::PendingRemote<ash::nearby::presence::mojom::ScanSession> remote;
  std::move(pending_on_start_scan_callback_)
      .Run(std::make_unique<ScanSession>(std::move(remote), base::DoNothing()),
           status_code);
}

}  // namespace ash::nearby::presence
