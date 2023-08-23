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

  std::move(start_scan_callback_)
      .Run(std::move(scan_session_remote_),
           /*status=*/mojo_base::mojom::AbslStatusCode::kOk);
}

void FakeNearbyPresence::OnDisconnect() {
  std::move(on_disconnect_callback_).Run();
}

void FakeNearbyPresence::UpdateLocalDeviceMetadata(
    mojom::MetadataPtr metadata) {
  local_device_metadata_ = std::move(metadata);
  std::move(update_local_device_metadata_callback_).Run();
}

void FakeNearbyPresence::UpdateLocalDeviceMetadataAndGenerateCredentials(
    mojom::MetadataPtr metadata,
    FakeNearbyPresence::UpdateLocalDeviceMetadataAndGenerateCredentialsCallback
        callback) {
  local_device_metadata_ = std::move(metadata);
  std::move(callback).Run(std::move(generated_shared_credentials_response_),
                          generate_credentials_response_status_);
}

void FakeNearbyPresence::UpdateRemoteSharedCredentials(
    std::vector<mojom::SharedCredentialPtr> shared_credentials,
    const std::string& account_name,
    FakeNearbyPresence::UpdateRemoteSharedCredentialsCallback callback) {
  std::move(callback).Run(update_remote_shared_credentials_status_);
}

void FakeNearbyPresence::GetLocalSharedCredentials(
    const std::string& account_name,
    FakeNearbyPresence::GetLocalSharedCredentialsCallback callback) {
  std::move(callback).Run(std::move(local_shared_credentials_response_),
                          get_local_shared_credential_status_);
}

}  // namespace ash::nearby::presence
