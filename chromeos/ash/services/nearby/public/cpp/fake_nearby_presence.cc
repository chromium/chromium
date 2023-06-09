// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/services/nearby/public/cpp/fake_nearby_presence.h"

namespace ash::nearby::presence {

FakeNearbyPresence::FakeNearbyPresence() {
  mojo::PendingRemote<ash::nearby::presence::mojom::NearbyPresence>
      pending_remote;
  receiver_set_.Add(this, pending_remote.InitWithNewPipeAndPassReceiver());
  shared_remote_.Bind(std::move(pending_remote), /*bind_task_runner=*/nullptr);
}

FakeNearbyPresence::~FakeNearbyPresence() = default;

void FakeNearbyPresence::BindInterface(
    mojo::PendingReceiver<ash::nearby::presence::mojom::NearbyPresence>
        pending_receiver) {
  receiver_set_.Add(this, std::move(pending_receiver));
}

void FakeNearbyPresence::SetScanObserver(
    mojo::PendingRemote<mojom::ScanObserver> scan_observer) {
  scan_observer_remote_.Bind(std::move(scan_observer), nullptr);
}

void FakeNearbyPresence::StartScan(
    mojom::ScanRequestPtr scan_request,
    FakeNearbyPresence::StartScanCallback callback) {
  start_scan_callback_ = std::move(callback);
  mojo::PendingRemote<mojom::ScanSession> scan_session_remote =
      scan_session_.BindNewPipeAndPassRemote();
  scan_session_remote_ = std::move(scan_session_remote);
  scan_session_.set_disconnect_handler(base::BindOnce(
      &FakeNearbyPresence::OnDisconnect, weak_ptr_factory_.GetWeakPtr()));
}

void FakeNearbyPresence::RunStartScanCallback() {
  // Run the callback to pass the |scan_session_remote| back to the client to
  // hold on to.
  std::move(start_scan_callback_)
      .Run(std::move(scan_session_remote_),
           /*status=*/ash::nearby::presence::mojom::StatusCode::kOk);
}

void FakeNearbyPresence::OnDisconnect() {
  on_disconnect_called_ = true;
}

void FakeNearbyPresence::UpdateLocalDeviceMetadata(
    mojom::MetadataPtr metadata) {}

void FakeNearbyPresence::UpdateLocalDeviceMetadataAndGenerateCredentials(
    mojom::MetadataPtr metadata,
    FakeNearbyPresence::UpdateLocalDeviceMetadataAndGenerateCredentialsCallback
        callback) {
  std::move(callback).Run(std::move(shared_credentials_), status_);
}

}  // namespace ash::nearby::presence
