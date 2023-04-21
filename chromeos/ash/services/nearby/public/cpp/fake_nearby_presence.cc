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

}  // namespace ash::nearby::presence
