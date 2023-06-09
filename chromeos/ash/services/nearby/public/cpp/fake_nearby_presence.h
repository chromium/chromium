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

  // ScanSession:
  void OnDisconnect();

  void RunStartScanCallback();
  bool WasOnDisconnectCalled() { return on_disconnect_called_; }

  mojo::SharedRemote<mojom::ScanObserver> ReturnScanObserver() {
    return scan_observer_remote_;
  }

  void SetGenerateCredentialsResponse(
      std::vector<mojom::SharedCredentialPtr> shared_credentials,
      mojom::StatusCode status) {
    shared_credentials_ = std::move(shared_credentials);
    status_ = status;
  }

 private:
  mojo::SharedRemote<mojom::ScanObserver> scan_observer_remote_;

  mojo::ReceiverSet<::ash::nearby::presence::mojom::NearbyPresence>
      receiver_set_;
  mojo::SharedRemote<::ash::nearby::presence::mojom::NearbyPresence>
      shared_remote_;
  mojo::Receiver<mojom::ScanSession> scan_session_{this};
  FakeNearbyPresence::StartScanCallback start_scan_callback_;
  mojo::PendingRemote<mojom::ScanSession> scan_session_remote_;

  bool on_disconnect_called_ = false;
  std::vector<mojom::SharedCredentialPtr> shared_credentials_;
  mojom::StatusCode status_ = mojom::StatusCode::kFailure;
  base::WeakPtrFactory<FakeNearbyPresence> weak_ptr_factory_{this};
};

}  // namespace ash::nearby::presence

#endif  // CHROMEOS_ASH_SERVICES_NEARBY_PUBLIC_CPP_FAKE_NEARBY_PRESENCE_H_
