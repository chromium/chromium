// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_SERVICES_NEARBY_PUBLIC_CPP_FAKE_NEARBY_PRESENCE_H_
#define CHROMEOS_ASH_SERVICES_NEARBY_PUBLIC_CPP_FAKE_NEARBY_PRESENCE_H_

#include "chromeos/ash/services/nearby/public/mojom/nearby_presence.mojom.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "mojo/public/cpp/bindings/shared_remote.h"

#include "base/memory/weak_ptr.h"
#include "chromeos/ash/services/nearby/public/mojom/nearby_presence.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/mojom/base/absl_status.mojom-forward.h"
#include "testing/gmock/include/gmock/gmock.h"

using NearbyPresenceMojom = ::ash::nearby::presence::mojom::NearbyPresence;

namespace ash::nearby::presence {

class FakeNearbyPresence : public mojom::NearbyPresence,
                           public mojom::ScanSession {
 public:
  FakeNearbyPresence();
  FakeNearbyPresence(const FakeNearbyPresence&) = delete;
  FakeNearbyPresence& operator=(const FakeNearbyPresence&) = delete;
  ~FakeNearbyPresence() override;

  const mojo::SharedRemote<::ash::nearby::presence::mojom::NearbyPresence>&
  shared_remote() const {
    return shared_remote_;
  }

  void BindInterface(
      mojo::PendingReceiver<ash::nearby::presence::mojom::NearbyPresence>
          pending_receiver);

  // NearbyPresence:
  void SetScanObserver(
      mojo::PendingRemote<mojom::ScanObserver> scan_observer) override;
  void StartScan(mojom::ScanRequestPtr scan_request,
                 FakeNearbyPresence::StartScanCallback callback) override;
  void UpdateLocalDeviceMetadata(mojom::MetadataPtr metadata) override;
  void UpdateLocalDeviceMetadataAndGenerateCredentials(
      mojom::MetadataPtr metadata,
      FakeNearbyPresence::
          UpdateLocalDeviceMetadataAndGenerateCredentialsCallback callback)
      override;
  void UpdateRemoteSharedCredentials(
      std::vector<mojom::SharedCredentialPtr> shared_credentials,
      const std::string& account_name,
      FakeNearbyPresence::UpdateRemoteSharedCredentialsCallback callback)
      override;
  void GetLocalSharedCredentials(
      const std::string& account_name,
      FakeNearbyPresence::GetLocalSharedCredentialsCallback callback) override;

  // ScanSession:
  void OnDisconnect();

  mojo::SharedRemote<mojom::ScanObserver> ReturnScanObserver() {
    return scan_observer_remote_;
  }

  void SetGenerateCredentialsResponse(
      std::vector<mojom::SharedCredentialPtr> shared_credentials,
      mojo_base::mojom::AbslStatusCode status) {
    generated_shared_credentials_response_ = std::move(shared_credentials);
    generate_credentials_response_status_ = status;
  }

  mojom::Metadata* GetLocalDeviceMetadata() {
    return local_device_metadata_.get();
  }

  void SetUpdateLocalDeviceMetadataCallback(base::OnceClosure callback) {
    update_local_device_metadata_callback_ = std::move(callback);
  }

  void SetUpdateRemoteCredentialsStatus(
      mojo_base::mojom::AbslStatusCode status) {
    update_remote_shared_credentials_status_ = status;
  }

  void SetOnDisconnectCallback(base::OnceClosure callback) {
    on_disconnect_callback_ = std::move(callback);
  }

  void SetLocalSharedCredentialsResponse(
      std::vector<mojom::SharedCredentialPtr> shared_credentials,
      mojo_base::mojom::AbslStatusCode status) {
    local_shared_credentials_response_ = std::move(shared_credentials);
    get_local_shared_credential_status_ = status;
  }

 private:
  mojo::SharedRemote<mojom::ScanObserver> scan_observer_remote_;
  mojom::MetadataPtr local_device_metadata_;
  base::OnceClosure update_local_device_metadata_callback_;

  mojo::ReceiverSet<::ash::nearby::presence::mojom::NearbyPresence>
      receiver_set_;
  mojo::SharedRemote<::ash::nearby::presence::mojom::NearbyPresence>
      shared_remote_;
  mojo::Receiver<mojom::ScanSession> scan_session_{this};
  FakeNearbyPresence::StartScanCallback start_scan_callback_;
  mojo::PendingRemote<mojom::ScanSession> scan_session_remote_;
  std::vector<mojom::SharedCredentialPtr>
      generated_shared_credentials_response_;
  std::vector<mojom::SharedCredentialPtr> local_shared_credentials_response_;
  mojo_base::mojom::AbslStatusCode generate_credentials_response_status_ =
      mojo_base::mojom::AbslStatusCode::kNotFound;
  mojo_base::mojom::AbslStatusCode update_remote_shared_credentials_status_ =
      mojo_base::mojom::AbslStatusCode::kOk;
  base::OnceClosure on_disconnect_callback_;
  mojo_base::mojom::AbslStatusCode get_local_shared_credential_status_ =
      mojo_base::mojom::AbslStatusCode::kOk;
  base::WeakPtrFactory<FakeNearbyPresence> weak_ptr_factory_{this};
};

}  // namespace ash::nearby::presence

#endif  // CHROMEOS_ASH_SERVICES_NEARBY_PUBLIC_CPP_FAKE_NEARBY_PRESENCE_H_
