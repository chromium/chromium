// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/network/hotspot_enabled_state_notifier.h"
#include "chromeos/ash/components/network/network_event_log.h"

namespace ash {

HotspotEnabledStateNotifier::HotspotEnabledStateNotifier() = default;

HotspotEnabledStateNotifier::~HotspotEnabledStateNotifier() {
  if (hotspot_state_handler_ && hotspot_state_handler_->HasObserver(this)) {
    hotspot_state_handler_->RemoveObserver(this);
  }

  if (hotspot_controller_ && hotspot_controller_->HasObserver(this)) {
    hotspot_controller_->RemoveObserver(this);
  }
}

void HotspotEnabledStateNotifier::Init(
    HotspotStateHandler* hotspot_state_handler,
    HotspotController* hotspot_controller) {
  hotspot_controller_ = hotspot_controller;
  hotspot_state_handler_ = hotspot_state_handler;
}

void HotspotEnabledStateNotifier::OnHotspotStatusChanged() {
  hotspot_config::mojom::HotspotState hotspot_state =
      hotspot_state_handler_->GetHotspotState();
  if (hotspot_state != hotspot_config::mojom::HotspotState::kDisabled) {
    return;
  }

  std::optional<hotspot_config::mojom::DisableReason> disable_reason =
      hotspot_state_handler_->GetDisableReason();

  if (!disable_reason) {
    NET_LOG(EVENT) << "Disable reason is not set in state handler";
    return;
  }

  if (disable_reason.value() ==
      hotspot_config::mojom::DisableReason::kUserInitiated) {
    NET_LOG(EVENT) << "Skipping recording user initiated disable events "
                      "reported from platform";
    return;
  }

  for (auto& observer : observers_) {
    observer->OnHotspotTurnedOff(disable_reason.value());
  }
}

void HotspotEnabledStateNotifier::OnHotspotTurnedOn() {
  for (auto& observer : observers_) {
    observer->OnHotspotTurnedOn();
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
  if (hotspot_state_handler_ && !hotspot_state_handler_->HasObserver(this)) {
    hotspot_state_handler_->AddObserver(this);
  }
  if (hotspot_controller_ && !hotspot_controller_->HasObserver(this)) {
    hotspot_controller_->AddObserver(this);
  }
  observers_.Add(std::move(observer));
}

}  // namespace ash
