// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/services/secure_channel/public/cpp/client/nearby_connector.h"

namespace ash::secure_channel {

NearbyConnector::NearbyConnector() = default;

NearbyConnector::~NearbyConnector() = default;

mojo::PendingRemote<mojom::NearbyConnector>
NearbyConnector::GeneratePendingRemote() {
  mojo::PendingRemote<mojom::NearbyConnector> pending_remote;
  receiver_set_.Add(this, pending_remote.InitWithNewPipeAndPassReceiver());
  return pending_remote;
}

}  // namespace ash::secure_channel
