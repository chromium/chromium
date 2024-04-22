// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_SERVICES_NEARBY_PUBLIC_CPP_FAKE_NEARBY_PROCESS_MANAGER_H_
#define CHROMEOS_ASH_SERVICES_NEARBY_PUBLIC_CPP_FAKE_NEARBY_PROCESS_MANAGER_H_

#include <memory>

#include "base/containers/flat_map.h"
#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "base/unguessable_token.h"
#include "chromeos/ash/services/nearby/public/cpp/mock_quick_start_decoder.h"
#include "chromeos/ash/services/nearby/public/cpp/nearby_process_manager.h"
#include "chromeos/ash/services/nearby/public/mojom/quick_start_decoder.mojom.h"

namespace ash {
namespace nearby {

class MockNearbyConnections;
class MockNearbySharingDecoder;

namespace presence {
class FakeNearbyPresence;
}  // namespace presence

class FakeNearbyProcessManager : public NearbyProcessManager {
 public:
  FakeNearbyProcessManager();
  ~FakeNearbyProcessManager() override;

  size_t GetNumActiveReferences() const;
  void SimulateProcessStopped(NearbyProcessShutdownReason shutdown_reason);

  // Return null if there are no active references.
  const MockNearbyConnections* active_connections() const {
    return active_connections_.get();
  }
  const MockNearbySharingDecoder* active_decoder() const {
    return active_decoder_.get();
  }

  // NearbyProcessManager:
  std::unique_ptr<NearbyProcessReference> GetNearbyProcessReference(
      NearbyProcessStoppedCallback on_process_stopped_callback) override;
  void ShutDownProcess() override;

 private:
  class FakeNearbyProcessReference
      : public NearbyProcessManager::NearbyProcessReference {
   public:
    FakeNearbyProcessReference(
        const mojo::SharedRemote<
            ::nearby::connections::mojom::NearbyConnections>& connections,
        const mojo::SharedRemote<
            ::ash::nearby::presence::mojom::NearbyPresence>& presence,
        const mojo::SharedRemote<::sharing::mojom::NearbySharingDecoder>&
            decoder,
        const mojo::SharedRemote<quick_start::mojom::QuickStartDecoder>&
            quick_start_decoder,
        base::OnceClosure destructor_callback);
    ~FakeNearbyProcessReference() override;

   private:
    // NearbyProcessManager::NearbyProcessReference:
    const mojo::SharedRemote<::nearby::connections::mojom::NearbyConnections>&
    GetNearbyConnections() const override;
    const mojo::SharedRemote<::ash::nearby::presence::mojom::NearbyPresence>&
    GetNearbyPresence() const override;
    const mojo::SharedRemote<::sharing::mojom::NearbySharingDecoder>&
    GetNearbySharingDecoder() const override;
    const mojo::SharedRemote<ash::quick_start::mojom::QuickStartDecoder>&
    GetQuickStartDecoder() const override;

    mojo::SharedRemote<::nearby::connections::mojom::NearbyConnections>
        connections_;
    mojo::SharedRemote<::ash::nearby::presence::mojom::NearbyPresence>
        presence_;
    mojo::SharedRemote<::sharing::mojom::NearbySharingDecoder> decoder_;
    mojo::SharedRemote<quick_start::mojom::QuickStartDecoder>
        quick_start_decoder_;
    base::OnceClosure destructor_callback_;
  };

  // KeyedService:
  void Shutdown() override;

  void OnReferenceDeleted(const base::UnguessableToken& reference_id);

  // Map which stores callbacks to be invoked if the Nearby process shuts down
  // unexpectedly, before clients release their references.
  base::flat_map<base::UnguessableToken, NearbyProcessStoppedCallback>
      id_to_process_stopped_callback_map_;

  // Null if no outstanding references exist.
  std::unique_ptr<MockNearbyConnections> active_connections_;
  std::unique_ptr<presence::FakeNearbyPresence> active_presence_;
  std::unique_ptr<MockNearbySharingDecoder> active_decoder_;
  std::unique_ptr<MockQuickStartDecoder> active_quick_start_decoder_;

  // Unbound if no outstanding references exist.
  mojo::SharedRemote<::nearby::connections::mojom::NearbyConnections>
      connections_remote_;
  mojo::SharedRemote<::ash::nearby::presence::mojom::NearbyPresence>
      presence_remote_;
  mojo::SharedRemote<::sharing::mojom::NearbySharingDecoder> decoder_remote_;
  mojo::SharedRemote<quick_start::mojom::QuickStartDecoder>
      quick_start_decoder_remote_;

  base::WeakPtrFactory<FakeNearbyProcessManager> weak_ptr_factory_{this};
};

}  // namespace nearby
}  // namespace ash

#endif  // CHROMEOS_ASH_SERVICES_NEARBY_PUBLIC_CPP_FAKE_NEARBY_PROCESS_MANAGER_H_
