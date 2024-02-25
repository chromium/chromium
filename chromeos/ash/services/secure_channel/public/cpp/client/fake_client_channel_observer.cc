// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/services/secure_channel/public/cpp/client/fake_client_channel_observer.h"

namespace ash::secure_channel {

FakeClientChannelObserver::FakeClientChannelObserver() = default;

FakeClientChannelObserver::~FakeClientChannelObserver() = default;

void FakeClientChannelObserver::OnDisconnected() {
  is_disconnected_ = true;
}

void FakeClientChannelObserver::OnMessageReceived(const std::string& payload) {
  received_messages_.push_back(payload);
}

void FakeClientChannelObserver::OnNearbyConnectionStateChagned(
    mojom::NearbyConnectionStep step,
    mojom::NearbyConnectionStepResult result) {
  nearby_connection_step_ = step;
  nearby_connection_step_result_ = result;
}

}  // namespace ash::secure_channel
