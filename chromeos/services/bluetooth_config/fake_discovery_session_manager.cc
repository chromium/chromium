// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/services/bluetooth_config/fake_discovery_session_manager.h"

namespace chromeos {
namespace bluetooth_config {

FakeDiscoverySessionManager::FakeDiscoverySessionManager(
    AdapterStateController* adapter_state_controller,
    DeviceCache* device_cache)
    : DiscoverySessionManager(adapter_state_controller, device_cache) {}

FakeDiscoverySessionManager::~FakeDiscoverySessionManager() = default;

void FakeDiscoverySessionManager::SetIsDiscoverySessionActive(bool is_active) {
  if (is_discovery_session_active_ == is_active)
    return;

  is_discovery_session_active_ = is_active;

  if (is_discovery_session_active_)
    NotifyDiscoveryStarted();
  else
    NotifyDiscoveryStoppedAndClearActiveClients();
}

bool FakeDiscoverySessionManager::IsDiscoverySessionActive() const {
  return is_discovery_session_active_;
}

}  // namespace bluetooth_config
}  // namespace chromeos
