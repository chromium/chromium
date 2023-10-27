// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/services/nearby/public/cpp/mock_nearby_connections.h"

namespace ash::nearby {

MockNearbyConnections::MockNearbyConnections() {
  mojo::PendingRemote<NearbyConnectionsMojom> pending_remote;
  receiver_set_.Add(this, pending_remote.InitWithNewPipeAndPassReceiver());
  shared_remote_.Bind(std::move(pending_remote), /*bind_task_runner=*/nullptr);
}

MockNearbyConnections::~MockNearbyConnections() = default;

void MockNearbyConnections::BindInterface(
    mojo::PendingReceiver<NearbyConnectionsMojom> pending_receiver) {
  receiver_set_.Add(this, std::move(pending_receiver));
}

}  // namespace ash::nearby
