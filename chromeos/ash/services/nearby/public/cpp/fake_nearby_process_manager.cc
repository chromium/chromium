// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/services/nearby/public/cpp/fake_nearby_process_manager.h"

#include <utility>

#include "base/functional/bind.h"
#include "base/notreached.h"
#include "chromeos/ash/services/nearby/public/cpp/fake_nearby_presence.h"
#include "chromeos/ash/services/nearby/public/cpp/mock_nearby_connections.h"
#include "chromeos/ash/services/nearby/public/cpp/mock_nearby_sharing_decoder.h"
#include "chromeos/ash/services/nearby/public/mojom/quick_start_decoder.mojom.h"
#include "mojo/public/cpp/bindings/shared_remote.h"

namespace ash {
namespace nearby {

FakeNearbyProcessManager::FakeNearbyProcessReference::
    FakeNearbyProcessReference(
        const mojo::SharedRemote<
            ::nearby::connections::mojom::NearbyConnections>& connections,
        const mojo::SharedRemote<
            ::ash::nearby::presence::mojom::NearbyPresence>& presence,
        const mojo::SharedRemote<::sharing::mojom::NearbySharingDecoder>&
            decoder,
        const mojo::SharedRemote<quick_start::mojom::QuickStartDecoder>&
            quick_start_decoder,
        base::OnceClosure destructor_callback)
    : connections_(connections),
      presence_(presence),
      decoder_(decoder),
      quick_start_decoder_(quick_start_decoder),
      destructor_callback_(std::move(destructor_callback)) {}

FakeNearbyProcessManager::FakeNearbyProcessReference::
    ~FakeNearbyProcessReference() {
  std::move(destructor_callback_).Run();
}

const mojo::SharedRemote<::nearby::connections::mojom::NearbyConnections>&
FakeNearbyProcessManager::FakeNearbyProcessReference::GetNearbyConnections()
    const {
  return connections_;
}

const mojo::SharedRemote<::ash::nearby::presence::mojom::NearbyPresence>&
FakeNearbyProcessManager::FakeNearbyProcessReference::GetNearbyPresence()
    const {
  return presence_;
}

const mojo::SharedRemote<::sharing::mojom::NearbySharingDecoder>&
FakeNearbyProcessManager::FakeNearbyProcessReference::GetNearbySharingDecoder()
    const {
  return decoder_;
}

const mojo::SharedRemote<ash::quick_start::mojom::QuickStartDecoder>&
FakeNearbyProcessManager::FakeNearbyProcessReference::GetQuickStartDecoder()
    const {
  return quick_start_decoder_;
}

FakeNearbyProcessManager::FakeNearbyProcessManager() = default;

FakeNearbyProcessManager::~FakeNearbyProcessManager() = default;

size_t FakeNearbyProcessManager::GetNumActiveReferences() const {
  return id_to_process_stopped_callback_map_.size();
}

void FakeNearbyProcessManager::SimulateProcessStopped(
    NearbyProcessShutdownReason shutdown_reason) {
  active_connections_.reset();
  active_presence_.reset();
  active_decoder_.reset();
  weak_ptr_factory_.InvalidateWeakPtrs();

  base::flat_map<base::UnguessableToken, NearbyProcessStoppedCallback> old_map =
      std::move(id_to_process_stopped_callback_map_);
  id_to_process_stopped_callback_map_.clear();

  for (auto& entry : old_map) {
    std::move(entry.second).Run(shutdown_reason);
  }
}

void FakeNearbyProcessManager::Shutdown() {
  SimulateProcessStopped(NearbyProcessShutdownReason::kNormal);
}

std::unique_ptr<NearbyProcessManager::NearbyProcessReference>
FakeNearbyProcessManager::GetNearbyProcessReference(
    NearbyProcessStoppedCallback on_process_stopped_callback) {
  if (!active_connections_) {
    active_connections_ = std::make_unique<MockNearbyConnections>();
  }

  if (!active_presence_) {
    active_presence_ = std::make_unique<presence::FakeNearbyPresence>();
  }

  if (!active_decoder_) {
    active_decoder_ = std::make_unique<MockNearbySharingDecoder>();
  }

  if (!active_quick_start_decoder_) {
    active_quick_start_decoder_ = std::make_unique<MockQuickStartDecoder>();
  }

  auto id = base::UnguessableToken::Create();
  id_to_process_stopped_callback_map_.emplace(
      id, std::move(on_process_stopped_callback));

  return std::make_unique<FakeNearbyProcessReference>(
      active_connections_->shared_remote(), active_presence_->shared_remote(),
      active_decoder_->shared_remote(),
      active_quick_start_decoder_->shared_remote(),
      base::BindOnce(&FakeNearbyProcessManager::OnReferenceDeleted,
                     weak_ptr_factory_.GetWeakPtr(), id));
}

void FakeNearbyProcessManager::ShutDownProcess() {
  NOTIMPLEMENTED();
}

void FakeNearbyProcessManager::OnReferenceDeleted(
    const base::UnguessableToken& reference_id) {
  auto it = id_to_process_stopped_callback_map_.find(reference_id);
  DCHECK(it != id_to_process_stopped_callback_map_.end());
  id_to_process_stopped_callback_map_.erase(it);

  if (id_to_process_stopped_callback_map_.empty()) {
    active_connections_.reset();
    active_presence_.reset();
    active_decoder_.reset();
  }
}

}  // namespace nearby
}  // namespace ash
