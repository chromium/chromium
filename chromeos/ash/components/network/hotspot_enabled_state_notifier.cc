// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/network/hotspot_enabled_state_notifier.h"

namespace ash {

HotspotEnabledStateNotifier::HotspotEnabledStateNotifier() = default;

HotspotEnabledStateNotifier::~HotspotEnabledStateNotifier() {
  if (hotspot_controller_ && hotspot_controller_->HasObserver(this)) {
    hotspot_controller_->RemoveObserver(this);
  }
}

void HotspotEnabledStateNotifier::Init(HotspotController* hotspot_controller) {
  hotspot_controller_ = hotspot_controller;
}

void HotspotEnabledStateNotifier::OnHotspotTurnedOn(bool wifi_turned_off) {
  for (auto& observer : observers_) {
    observer->OnHotspotTurnedOn(wifi_turned_off);
  }
}

void HotspotEnabledStateNotifier::OnHotspotTurnedOff(
    hotspot_config::mojom::DisableReason disable_reason) {
  for (auto& observer : observers_) {
    observer->OnHotspotTurnedOff(disable_reason);
  }
}

void HotspotEnabledStateNotifier::ObserveEnabledStateChanges(
    mojo::PendingRemote<hotspot_config::mojom::HotspotEnabledStateObserver>
        observer) {
  if (hotspot_controller_ && !hotspot_controller_->HasObserver(this)) {
    hotspot_controller_->AddObserver(this);
  }
  observers_.Add(std::move(observer));
}

}  // namespace ash
