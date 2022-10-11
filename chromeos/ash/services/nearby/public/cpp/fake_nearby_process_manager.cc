// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/services/nearby/public/cpp/fake_nearby_process_manager.h"

#include <utility>

#include "base/bind.h"
#include "chromeos/ash/services/nearby/public/cpp/mock_nearby_connections.h"
#include "chromeos/ash/services/nearby/public/cpp/mock_nearby_sharing_decoder.h"

namespace ash {
namespace nearby {

FakeNearbyProcessManager::FakeNearbyProcessReference::
    FakeNearbyProcessReference(
        const mojo::SharedRemote<
            location::nearby::connections::mojom::NearbyConnections>&
            connections,
        const mojo::SharedRemote<sharing::mojom::NearbySharingDecoder>& decoder,
        base::OnceClosure destructor_callback)
    : connections_(connections),
      decoder_(decoder),
      destructor_callback_(std::move(destructor_callback)) {}

FakeNearbyProcessManager::FakeNearbyProcessReference::
    ~FakeNearbyProcessReference() {
  std::move(destructor_callback_).Run();
}

const mojo::SharedRemote<
    location::nearby::connections::mojom::NearbyConnections>&
FakeNearbyProcessManager::FakeNearbyProcessReference::GetNearbyConnections()
    const {
  return connections_;
}

const mojo::SharedRemote<sharing::mojom::NearbySharingDecoder>&
FakeNearbyProcessManager::FakeNearbyProcessReference::GetNearbySharingDecoder()
    const {
  return decoder_;
}

FakeNearbyProcessManager::FakeNearbyProcessManager() = default;

FakeNearbyProcessManager::~FakeNearbyProcessManager() = default;

size_t FakeNearbyProcessManager::GetNumActiveReferences() const {
  return id_to_process_stopped_callback_map_.size();
}

void FakeNearbyProcessManager::SimulateProcessStopped(
    NearbyProcessShutdownReason shutdown_reason) {
  active_connections_.reset();
  active_decoder_.reset();
  weak_ptr_factory_.InvalidateWeakPtrs();

  base::flat_map<base::UnguessableToken, NearbyProcessStoppedCallback> old_map =
      std::move(id_to_process_stopped_callback_map_);
  id_to_process_stopped_callback_map_.clear();

  for (auto& entry : old_map)
    std::move(entry.second).Run(shutdown_reason);
}

void FakeNearbyProcessManager::Shutdown() {
  SimulateProcessStopped(NearbyProcessShutdownReason::kNormal);
}

std::unique_ptr<NearbyProcessManager::NearbyProcessReference>
FakeNearbyProcessManager::GetNearbyProcessReference(
    NearbyProcessStoppedCallback on_process_stopped_callback) {
  if (!active_connections_)
    active_connections_ = std::make_unique<MockNearbyConnections>();
  if (!active_decoder_)
    active_decoder_ = std::make_unique<MockNearbySharingDecoder>();

  auto id = base::UnguessableToken::Create();
  id_to_process_stopped_callback_map_.emplace(
      id, std::move(on_process_stopped_callback));

  return std::make_unique<FakeNearbyProcessReference>(
      active_connections_->shared_remote(), active_decoder_->shared_remote(),
      base::BindOnce(&FakeNearbyProcessManager::OnReferenceDeleted,
                     weak_ptr_factory_.GetWeakPtr(), id));
}

void FakeNearbyProcessManager::OnReferenceDeleted(
    const base::UnguessableToken& reference_id) {
  auto it = id_to_process_stopped_callback_map_.find(reference_id);
  DCHECK(it != id_to_process_stopped_callback_map_.end());
  id_to_process_stopped_callback_map_.erase(it);

  if (id_to_process_stopped_callback_map_.empty()) {
    active_connections_.reset();
    active_decoder_.reset();
  }
}

}  // namespace nearby
}  // namespace ash
