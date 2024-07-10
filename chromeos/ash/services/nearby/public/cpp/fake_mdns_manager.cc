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
  discovery_sessions_.insert(service_type);
  std::move(callback).Run(true);
}

void FakeMdnsManager::StopDiscoverySession(
    const std::string& service_type,
    StopDiscoverySessionCallback callback) {
  std::move(callback).Run(discovery_sessions_.count(service_type));
  discovery_sessions_.erase(service_type);
}

void FakeMdnsManager::AddObserver(
    ::mojo::PendingRemote<::sharing::mojom::MdnsObserver> observer) {
  observers_.Add(std::move(observer));
}

void FakeMdnsManager::NotifyObserversServiceFound(
    ::sharing::mojom::NsdServiceInfoPtr service_info) {
  for (auto& observer : observers_) {
    observer->ServiceFound(service_info.Clone());
  }
}

void FakeMdnsManager::NotifyObserversServiceLost(
    ::sharing::mojom::NsdServiceInfoPtr service_info) {
  for (auto& observer : observers_) {
    observer->ServiceLost(service_info.Clone());
  }
}

}  // namespace ash::nearby
