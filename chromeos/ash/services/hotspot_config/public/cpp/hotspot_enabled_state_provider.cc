// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/services/hotspot_config/public/cpp/hotspot_enabled_state_provider.h"

namespace ash::hotspot_config {

HotspotEnabledStateProvider::HotspotEnabledStateProvider() = default;

HotspotEnabledStateProvider::~HotspotEnabledStateProvider() = default;

void HotspotEnabledStateProvider::ObserveEnabledStateChanges(
    mojo::PendingRemote<mojom::HotspotEnabledStateObserver> observer) {
  observers_.Add(std::move(observer));
}

void HotspotEnabledStateProvider::NotifyHotspotTurnedOn(bool wifi_turned_off) {
  for (auto& observer : observers_) {
    observer->OnHotspotTurnedOn(wifi_turned_off);
  }
}

void HotspotEnabledStateProvider::NotifyHotspotTurnedOff(
    mojom::DisableReason reason) {
  for (auto& observer : observers_) {
    observer->OnHotspotTurnedOff(reason);
  }
}

}  // namespace ash::hotspot_config