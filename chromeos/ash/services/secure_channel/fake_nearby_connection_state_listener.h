// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_SERVICES_SECURE_CHANNEL_FAKE_NEARBY_CONNECTION_STATE_LISTENER_H_
#define CHROMEOS_ASH_SERVICES_SECURE_CHANNEL_FAKE_NEARBY_CONNECTION_STATE_LISTENER_H_

#include "chromeos/ash/services/secure_channel/public/mojom/nearby_connector.mojom-shared.h"
#include "chromeos/ash/services/secure_channel/public/mojom/nearby_connector.mojom.h"

namespace ash::secure_channel {

class FakeNearbyConnectionStateListener
    : public mojom::NearbyConnectionStateListener {
 public:
  FakeNearbyConnectionStateListener();
  FakeNearbyConnectionStateListener(const FakeNearbyConnectionStateListener&) =
      delete;
  FakeNearbyConnectionStateListener& operator=(
      const FakeNearbyConnectionStateListener&) = delete;
  ~FakeNearbyConnectionStateListener() override;

  mojom::NearbyConnectionStep nearby_connection_step() {
    return nearby_connection_step_;
  }
  mojom::NearbyConnectionStepResult nearby_connection_step_result() {
    return nearby_connection_step_result_;
  }

 private:
  // mojom::NearbyConnectionStateListener:
  void OnNearbyConnectionStateChanged(
      mojom::NearbyConnectionStep nearby_connection_step,
      mojom::NearbyConnectionStepResult result) override;

  mojom::NearbyConnectionStep nearby_connection_step_;
  mojom::NearbyConnectionStepResult nearby_connection_step_result_;
};
}  // namespace ash::secure_channel

#endif  // CHROMEOS_ASH_SERVICES_SECURE_CHANNEL_FAKE_NEARBY_CONNECTION_STATE_LISTENER_H_
