// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/services/nearby/public/cpp/fake_mdns_manager.h"

namespace ash::nearby {

FakeMdnsManager::FakeMdnsManager() = default;

FakeMdnsManager::~FakeMdnsManager() = default;

void FakeMdnsManager::StartDiscoverySession(
    const std::string& service_type,
    StartDiscoverySessionCallback callback) {
  // NOT IMPLEMENTED
}
void FakeMdnsManager::StopDiscoverySession(
    const std::string& service_type,
    StopDiscoverySessionCallback callback) {
  // NOT IMPLEMENTED
}
void FakeMdnsManager::AddObserver(
    ::mojo::PendingRemote<::sharing::mojom::MdnsObserver> observer) {
  // NOT IMPLEMENTED
}

}  // namespace ash::nearby
