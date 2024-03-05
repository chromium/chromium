// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/data_migration/testing/fake_nearby_process_manager.h"

#include "base/notimplemented.h"
#include "chromeos/ash/services/nearby/public/mojom/nearby_decoder.mojom.h"
#include "chromeos/ash/services/nearby/public/mojom/nearby_presence.mojom.h"
#include "chromeos/ash/services/nearby/public/mojom/quick_start_decoder.mojom.h"
#include "mojo/public/cpp/bindings/pending_remote.h"

namespace data_migration {
namespace {

using NearbyConnectionsMojom = ::nearby::connections::mojom::NearbyConnections;

class FakeNearbyProcessReference
    : public ash::nearby::NearbyProcessManager::NearbyProcessReference {
 public:
  explicit FakeNearbyProcessReference(
      mojo::SharedRemote<NearbyConnectionsMojom> nearby_connections)
      : nearby_connections_(std::move(nearby_connections)) {}
  FakeNearbyProcessReference(const FakeNearbyProcessReference&) = delete;
  FakeNearbyProcessReference& operator=(const FakeNearbyProcessReference&) =
      delete;
  ~FakeNearbyProcessReference() override = default;

  const mojo::SharedRemote<NearbyConnectionsMojom>& GetNearbyConnections()
      const override {
    return nearby_connections_;
  }

  // None of the below are used for data migration.
  const mojo::SharedRemote<ash::nearby::presence::mojom::NearbyPresence>&
  GetNearbyPresence() const override {
    NOTIMPLEMENTED();
    return nearby_presence_;
  }

  const mojo::SharedRemote<::sharing::mojom::NearbySharingDecoder>&
  GetNearbySharingDecoder() const override {
    NOTIMPLEMENTED();
    return nearby_sharing_decoder_;
  }

  const mojo::SharedRemote<ash::quick_start::mojom::QuickStartDecoder>&
  GetQuickStartDecoder() const override {
    NOTIMPLEMENTED();
    return quick_start_decoder_;
  }

 private:
  const mojo::SharedRemote<NearbyConnectionsMojom> nearby_connections_;
  const mojo::SharedRemote<ash::nearby::presence::mojom::NearbyPresence>
      nearby_presence_;
  const mojo::SharedRemote<::sharing::mojom::NearbySharingDecoder>
      nearby_sharing_decoder_;
  const mojo::SharedRemote<ash::quick_start::mojom::QuickStartDecoder>
      quick_start_decoder_;
};

}  // namespace

FakeNearbyProcessManager::FakeNearbyProcessManager(
    std::string_view remote_endpoint_id)
    : fake_nearby_connections_(std::move(remote_endpoint_id)) {}

FakeNearbyProcessManager::~FakeNearbyProcessManager() = default;

std::unique_ptr<ash::nearby::NearbyProcessManager::NearbyProcessReference>
FakeNearbyProcessManager::GetNearbyProcessReference(
    NearbyProcessStoppedCallback on_process_stopped_callback) {
  mojo::PendingRemote<NearbyConnectionsMojom> pending_remote;
  receiver_set_.Add(&fake_nearby_connections_,
                    pending_remote.InitWithNewPipeAndPassReceiver());
  return std::make_unique<FakeNearbyProcessReference>(
      mojo::SharedRemote<NearbyConnectionsMojom>(std::move(pending_remote)));
}

void FakeNearbyProcessManager::ShutDownProcess() {
  receiver_set_.Clear();
}

}  // namespace data_migration
