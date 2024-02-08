// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/services/secure_channel/fake_nearby_connection_state_listener.h"

namespace ash::secure_channel {

FakeNearbyConnectionStateListener::FakeNearbyConnectionStateListener() =
    default;
FakeNearbyConnectionStateListener::~FakeNearbyConnectionStateListener() =
    default;

void FakeNearbyConnectionStateListener::OnNearbyConnectionStateChanged(
    mojom::NearbyConnectionStep nearby_connection_step,
    mojom::NearbyConnectionStepResult result) {
  nearby_connection_step_ = nearby_connection_step;
  nearby_connection_step_result_ = result;
}
}  // namespace ash::secure_channel
